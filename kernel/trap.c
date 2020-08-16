#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "mmap.h"

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

static int
stack_pgfault() {
  char *mem;
  uint addr, bottom, npages;
  // Virtual Address to bottom of stack
  bottom = KERNBASE - (myproc()->stack_sz * PGSIZE); 
  // Check if address that caused the fault was below the bottom of the stack
  if(rcr2() < bottom) {
    npages = (bottom - PGROUNDDOWN(rcr2())) / PGSIZE;
    // Size of stack must be less than 4MB
    if((myproc()->stack_sz + npages) < STACKMAX) {
      // Increase stack size
      myproc()->stack_sz += npages;
      for(uint i = 1; i <= npages; ++i) {
        // Starting Virtual Address of faulting page
        addr = bottom - (i * PGSIZE); 
        // Allocate one page of physical memory
        mem = kalloc();
        // Initialize page to 0
        memset(mem, 0, PGSIZE);
        /* Create PTE(s) for the new physical page(s) */
        mappages(myproc()->pgdir, (char*)addr, PGSIZE, V2P(mem), PTE_W|PTE_U);
      }
      return 0;
    }
    return -1;
  }
  return 0;
}

static int
mmap_pgfault(struct trapframe *tf) {
  struct proc *curproc = myproc();
  struct map_node *m = 0;
  uint start, end, w = 0, addr;
  char *mem;

  /* Find Mapped Region Where Fault Occurred */
  for(uint i = 0; i < MAPMAX; ++i) {
    start = (uint)curproc->m.maps[i].s_addr;
    end = (uint)curproc->m.maps[i].e_addr;
    if((rcr2() >= start) && (rcr2() < end)) {
      m = &curproc->m.maps[i];
      break;
    }
  }

  /* Faulting Address is not mapped - Kill Process */
  if(!m) {
    return -1;
  }

  /* Anonymous Memory Mapping */
  if(!(m->flags & MAP_FILE)) {
    for(uint i = 0; i < (m->length/PGSIZE); ++i) {
      start = start + (i * PGSIZE);
      /* Allocate a page of physical memory */
      if(!(mem = kalloc()))
        return -1; // Kill process if page cannot be allocated
      /* Initialize page to 0 (Zero Filled On First Access) */
      memset(mem, 0, PGSIZE);
      /* Create PTE(s) for the new physical page(s) */
      mappages(myproc()->pgdir, (char*)start, PGSIZE, V2P(mem), PTE_W|PTE_U);
    }
  } else {
    /* File Backed Memory Mapping */
    /* Make sure Permissions aren't violated */
    if((tf->err & E_W) && (!m->file->writable)) {
      /* Write Fault and File not Writable - Kil Process */
      return -1;
    } 

    /* If file is writable - Set PTE_W bit */
    if(m->file->writable) {
      w = PTE_W; 
    }

    /* File buffer */
    char buff[m->length];
    /* Read file */
    readi(m->file->ip, buff, m->offset, m->length);

    cprintf("buff dump: %s\n", buff);

    /* Allocate pages and copy file content */
    for(uint i = 0; i < (m->length/PGSIZE); ++i) {
      addr = (i * PGSIZE);
      /* Allocate a page of physical memory */
      if(!(mem = kalloc()))
        return -1; // Kill process if page cannot be allocated
      /* Copy File Buffer content */
      memset(mem, buff[addr], PGSIZE);
      /* Create PTE(s) for the new physical page(s) */
      mappages(myproc()->pgdir, (char*)start, PGSIZE, V2P(mem), w|PTE_U);
    }
  }
  return 0;
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
    if(rcr2() > MAPPINGSTART) {
      /* Handle Stack Allocation */
      if(stack_pgfault() < 0) {
        cprintf("Reached Stack size limit\n");
        goto kill;
      }
      lapiceoi();
      return;
    } else {
      /* Handle mmap Allocation */
      if(mmap_pgfault(tf) < 0) {
        goto kill;
      }
      lapiceoi();
      return;
    }
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks); // Notify any processes that are sleeping waiting for the value of ticks to change
      /* Wakeup swap daemon every 100 ticks */
      //if((ticks % 100) == 0)
        //cprintf("wake up swap - from trap\n");
        //wakeup(&swapp);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE_P:
    /* Primary IDE controller interrupt */
    ideintr(BASE_ADDR1, BASE_ADDR2);
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE_S:
    /* Secondary IDE controller interrupt */
    ideintr(BASE_ADDR3, BASE_ADDR4);
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
