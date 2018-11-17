#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void) {
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void) {
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf) {
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit(1);
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit(1);
    return;
  }

  // Handle Page Fault - Allocate page of memory for the stack.
  if(tf->trapno == T_PGFLT) {
    char *mem;
    uint addr, size, npages;
    // Address to bottom of stack
    size = KERNBASE - (myproc()->stack_sz * PGSIZE); 
    // Check if address that caused the fault was below the bottom of the stack
    if(rcr2() < size) {
      npages = (size - PGROUNDDOWN(rcr2())) / PGSIZE;
      /* Increment the amount of page allocation needed - used by swap daemon */
      mem_amount += npages;
      // Size of stack must be less than 4MB.
      if((myproc()->stack_sz + npages) < STACKMAX) {
        // Increase stack size
        myproc()->stack_sz += npages;
        for(uint i = 1; i <= npages; ++i) {
          // Start address of faulting page
          addr = size - (i * PGSIZE); 
          // Allocate one page of physical memory
          mem = kalloc();
          // Initialize page to 0
          memset(mem, 0, PGSIZE);
          // Map the new page to the physical page.
          mappages(myproc()->pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U);
        }
        return;
      }
      cprintf("Reached Stack size limit.\n");
      goto kill;
    }
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks); // Notify any processes that are sleeping waiting for the value of ticks to change
      /* Wakeup swap daemon every 100 ticks */
      if((ticks % 100) == 0)
        //cprintf("wake up swap - from trap\n");
        wakeup(&swapp);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE_P:
    /* Primary IDE controller interrupt */
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE_S:
    /* Secondary IDE controller interrupt */
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr(0);
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM2:
    uartintr(1);
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
kill:
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
    myproc()->exit_status = -1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit(0);

  // Invoke the scheduler on clock tick.
  if(tf->trapno == T_IRQ0+IRQ_TIMER)
    reschedule();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit(0);
}
