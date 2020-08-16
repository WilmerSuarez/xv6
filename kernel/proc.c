#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "date.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
static void sched(void);
static struct proc *roundrobin(void);
struct spinlock swaplock;

void
pinit(void) {
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void) {
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  // Find unsused process slot in ptable
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  /* Initialize Mapping Count */
  p->m.map_count = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n) {
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  // Make sure that heap doesn't collide with the stack
  if(curproc->stack_sz != 0) {
    if((sz + n) >= ((KERNBASE - (curproc->stack_sz * PGSIZE)) - PGSIZE)) 
      return -1;
  }
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void) {
  int i, pid;
  struct proc *np; // New process 
  struct proc *curproc = myproc();

  // Allocate process.
  // Sets up kernel stack
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from curproc.
  // Copy page directory from the parent process to the child process
  // If failed, revert previous allocation
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz, curproc->stack_sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz; 
  np->stack_sz = curproc->stack_sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(int estatus) {
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  estatus = estatus & 0x7FFFFFFF; // Mask out the sign bit (32nd bit (MSB))

  curproc->exit_status = estatus;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait() (waiting for child to finish (exit())).
  wakeup1(curproc->parent);

  // Pass abandoned children to init process
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(int *estatus) {
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found child process. Clean up & release resources (free memory)
        if(estatus) {
          if(p->killed) {
            *estatus = -1;
            p->exit_status = -1;
          } else {
            *estatus = p->exit_status;
          }
        }
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      if(estatus) {
        curproc->exit_status = -1;
        *estatus = -1;
      }
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU idle loop.
// Each CPU calls idle() after setting itself up.
// Idle never returns.  It loops, executing a HLT instruction in each
// iteration.  The HLT instruction waits for an interrupt (such as a
// timer interrupt) to occur.  Actual work gets done by the CPU when
// the scheduler is invoked to switch the CPU from the idle loop to
// a process context.
void
idle(void) {
  sti(); // Enable interrupts on this processor
  for(;;) {
    if(!(readeflags()&FL_IF))
      panic("idle non-interruptible");
    hlt(); // Wait for an interrupt
  }
}

// The process scheduler.
//
// Assumes ptable.lock is held, and no other locks.
// Assumes interrupts are disabled on this CPU.
// Assumes proc->state != RUNNING (a process must have changed its
// state before calling the scheduler).
// Saves and restores intena because the original xv6 code did.
// (Original comment:  Saves and restores intena because intena is
// a property of this kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would break in the few
// places where a lock is held but there's no process.)
//
// When invoked, does the following:
//  - choose a process to run
//  - swtch to start running that process (or idle, if none)
//  - eventually that process transfers control
//      via swtch back to the scheduler.

static void
sched(void) {
  int intena;
  struct proc *p;
  struct context **oldcontext;
  struct cpu *c = mycpu();
  
  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(c->ncli != 1)
    panic("sched locks");
  if(readeflags()&FL_IF)
    panic("sched interruptible");

  // Determine the current context, which is what we are switching from.
  // c->proc is NULL if CPU is idle. Pointer to process struct otherwise.
  if(c->proc) { 
    // If there is a process currently running on this CPU
    if(c->proc->state == RUNNING)
      panic("sched running");
    oldcontext = &c->proc->context;
  } else { 
    // If CPU is idle
    oldcontext = &(c->scheduler);
  }

  // Choose next process to run.
  if((p = roundrobin()) != 0) {
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    p->state = RUNNING;
    switchuvm(p);
    if(c->proc != p) { 
      // If selected process is different from the one 
      // currently being run on this CPU
      c->proc = p;
      intena = c->intena;
      swtch(oldcontext, p->context);
      // This code is reached when the process that was swapped is chosen
      // to run again. Might come back on another CPU v
      mycpu()->intena = intena;  // We might return on a different CPU.
    }
  } else {
    // No process to run -- switch to the idle loop.
    switchkvm();
    if(oldcontext != &(c->scheduler)) {
      c->proc = 0;
      intena = c->intena;
      swtch(oldcontext, c->scheduler);
      mycpu()->intena = intena;
    }
  }
}

// Round-robin scheduler.
// The same variable is used by all CPUs to determine the starting index.
// It is protected by the process table lock, so no additional lock is
// required.
static int rrindex;

static struct proc *
roundrobin() {
  // Loop over process table looking for process to run.
  for(int i = 0; i < NPROC; i++) {
    struct proc *p = &ptable.proc[(i + rrindex + 1) % NPROC];
    if(p->state != RUNNABLE)
      continue;
    rrindex = p - ptable.proc;
    return p;
  }
  return 0;
}

// Called from timer interrupt to reschedule the CPU.
void
reschedule(void) {
  struct cpu *c = mycpu();

  acquire(&ptable.lock);
  if(c->proc) {
    if(c->proc->state != RUNNING)
      panic("current process not in running state");
    c->proc->state = RUNNABLE;
  }
  sched();
  // NOTE: there is a race here.  We need to release the process
  // table lock before idling the CPU, but as soon as we do, it
  // is possible that an an event on another CPU could cause a process
  // to become ready to run.  The undesirable (but non-catastrophic)
  // consequence of such an occurrence is that this CPU will idle until
  // the next timer interrupt, when in fact it could have been doing
  // useful work.  To do better than this, we would need to arrange
  // for a CPU releasing the process table lock to interrupt all other
  // CPUs if there could be any runnable processes.
  release(&ptable.lock);
}

// Give up the CPU for one scheduling round.
void
yield(void) {
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void) {
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1 // wakeup() race impossible with ptable.lock held
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched(); // Returns with ptable.lock held

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan) {
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid) {
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      p->exit_status = -1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void) {
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// System call - getdate
// Get the curret date using cmostime() 
int 
getdate(struct rtcdate *r) {
  cmostime(r);
  return 0;
}

// System call - setdate
// Write date to RTC registers.
int
setdate(struct rtcdate *r) {
  if(validate_date(r) >= 0 && validate_time(r) >= 0){
    cmostime_write(r);
  } else {
    return -1;
  }

  return 0;
}

int validate_date(struct rtcdate *r) {
// Validate year
  if(r->year >= 2000 && r->year <= 2099) {
    // Validate month
    if(r->month >= 1 && r->month <= 12) {
      // Validate days
      if((r->day >= 1 && r->day <= 31) && (r->month == 1 || r->month == 3 || r->month == 5 || r->month == 7 || r->month == 8 || r->month == 10 || r->month == 12)) {
        return 0;
      } else if((r->day >= 1 && r->day <= 30) && (r->month == 4 || r->month == 6 || r->month == 9 || r->month == 11)) {
        return 0;
      } else if((r->day >= 1 && r->day <= 28) && (r->month == 2)) {
        return 0;
      } else if(r->day == 29 && r->month == 2 && ((r->year%4 == 0 && r->year%100 != 0) || r->year%400 == 0)) {
        return 0;
      }
    }
  }

  return -1;
}

int 
validate_time(struct rtcdate *r) {
  // Validate seconds
  if(r->second >= 0 && r->second < 60) {
    // Validate minutes
    if(r->minute >= 0 && r->minute < 60) {
      // Validate hours 
      if(r->hour >= 0 && r->hour < 24) {
        return 0;
      }
    }
  }
  
  return -1;
}

/*
  Modified exit function used by a
  dameon if it returns.
*/
void
daemonexit(void) {
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  acquire(&ptable.lock);

  /* Init process might be sleeping in wait() (waiting for its children to exit) */
  wakeup1(curproc->parent);

  // Pass abandoned children to init process
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie daemonexit");
}

/*
  Used to create "kernel-only" threads. 
  Allocates a process table entry, kernel stack, and kernel page table.
  No user address space.
*/
void 
kfork(void (*func)(void)) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  /* Find unsused process slot in ptable */
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return;

/* Unsused process table entry found */
found:
  p->pid = nextpid++;
  p->parent = initproc;
  release(&ptable.lock);

  /* Allocate kernel stack */
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return;
  }
  sp = p->kstack + KSTACKSIZE;

  /* No trapframe required, proess will spend entire lifetime in the kernel */
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)daemonexit;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)func;

  /* Allocate kernel Page Table */
  if((p->pgdir = setupkvm()) == 0)
    panic("kfork");

  /* Allow process to be scheduled */
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

/* ---------- DAEMONS ---------- */
/*static void 
ticktock(void) {
  uint i = 100;
  uint _ticks;*/

  /* 
    Release ptable.lock still being
    held by scheduler 
  */
  /*release(&ptable.lock);

  for(;;) {
    acquire(&tickslock);
    _ticks = ticks;
    while((ticks - _ticks) < i) {
      sleep(&ticks, &tickslock);  
    }
    cprintf("100 Ticks!\n");
    release(&tickslock);
  }
}*/

/*
  Used by swaprobin() to remeber the starting index in the ptable 
  to test for the next process to swap out 

static int procindex; 
 

  Round-robin policy used by the swap daemon to determine which is 
  the next process that should be swapped out to the disk.

static struct proc *
swaprobin() {
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
   Loop over process table looking for process to swap 
  for(uint i = 0; i < NPROC; i++) {
    struct proc *p = &ptable.proc[(i + procindex + 1) % NPROC];
     Make sure the current process or a process already swapped doesn't get swapped-out 
    if(p == curproc || p->swapped)  
      continue;
    procindex = p - ptable.proc; 
    p->swapped = 1; // Process will be swapped 
    p->t = 0;  // Time the process has spent swapped-out
    return p;
    release(&ptable.lock);
  }
  release(&ptable.lock);
  return 0;
}

 
  Find a process to swap back into RAM 

static struct proc *
swap_in() {
  struct proc *p;
   Traverse the proctable for the process that has been swapped-out for the longest time 
  acquire(&ptable.lock);
    p = &ptable.proc[0];
  for(uint i = 1; i < NPROC; i++) {
    if(ptable.proc[i].t > p->t) {
      p = &ptable.proc[i];
    }
  }
  if(p->t && p->swapped) {
    release(&ptable.lock);
    return p;
  } 
  release(&ptable.lock);
  return 0;
}

 
  Used by swap daemon to get the PTEs of the process
  being swapped-out

static pte_t *
walkswap(pde_t *pgdir, const void *va) {
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
     Get address of Page Table pointed to by top 20 bits of PDE 
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde)); 
  }
   Get address of Page Table Entry in Page Table 
  return &pgtab[PTX(va)]; 
}


 Bitmap modification and access Macros 
#define SETBIT(MAP, INDEX)    (MAP[(INDEX/8)] |= (1<<(INDEX%8)))
#define CLEARBIT(MAP, INDEX)  (MAP[(INDEX/8)] &= ~(1<<(INDEX%8)))
#define GETBIT(MAP, INDEX)    (MAP[(INDEX/8)] & (1<<(INDEX%8)))
*/
//static void
//swap(void) {
  //const uint devno = 0;
  /* Stores the byte-size goal the swap daemon needs to free */ 
  //uint freesz;
  /* Process getting swapped-in */
  //struct proc *inproc;
  /* Process getting swapped-out */
  //struct proc *outproc;
  /* Bitmap used to implement a first fit policy for the disk sectors */
  //uchar disk_bitmap[1250] = { 0 };
  /* Store amount of disk space required for process (number of blocks/disk sectors) */
  //uint dspace = 0;
  /* Used to store the number of consecutive sectors */
  //uint sectors = 0;
  /* Buffer for R/W disk */
  //struct buf *procbuf;
  /* Holds index of first empty position in bitmap */
  //uint index = 0;
  /* Holds address space PTEs for processes being swapped-out */
  //pte_t *pte;
  /* Holds address of Kernel Virtual Address */
  //char * addr;
  //uchar data[512] = { 0 };
  
  /* 
    Release ptable.lock still being
    held by scheduler 
  *//*
  release(&ptable.lock);

  for(;;) {
    acquire(&swaplock);
  
      Awoken when 100 ticks have passed on cpu0 or 
      when kalloc runs out of memory to allocate 
    
    sleep(&swapp, &swaplock);  
    //cprintf("Swap awake!\n");
    release(&swaplock);

     Increment the amount of time swapped process have been in the disk 
    //incrtime();

     Find process to swap-in 
    inproc = swap_in();

     
        Target amount of bytes to free = 
        Size of inproc process memory + Size of its Stack + Size of its Page Table  
        + Size of memory needed by kalloc (mem_amount) (if any) 
    
    if(inproc)
      freesz = inproc->sz + (inproc->stack_sz * PGSIZE) + PGSIZE;
    if(mem_amount)
      freesz += (mem_amount * PGSIZE);
     
      If no swapped process is found and if no memory needs to
      be freed for kalloc, then no swapping occurs.
    
    if(!inproc && !mem_amount)
      goto do_nothing;

    Select process to be swapped out - using round robin policy 
    while((outproc = swaprobin()) != 0) {
       Caluclate amount of disk space (number of blocks) needed to hold the process 
       ((Size of usercode + data + stack + kernel stack) / PGSIZE) * 8 
      dspace = ((outproc->sz + (outproc->stack_sz * PGSIZE) + PGSIZE) / PGSIZE) * 8;

       Traverse the Disk Bitmap to find enough unsused consecutive sectors on the disk 
      for(index; index < SDSIZE; ++index) {
        if(GETBIT(disk_bitmap, index)) {
           Block is in use 
           Reset the number of consecutive sectors 
          sectors = 0;
        } else {
           Block is not in use 
           Increment the number of consecutive sectors 
          sectors++;
           If enough sectors found, set beginning sector of process being swapped-out 
          if(sectors == dspace) {
            outproc->sector = index - dspace + 1;
             Set sectors as allocated 
            for(unsigned a = outproc->sector; a < dspace; ++a)
              SETBIT(disk_bitmap, a);
            break;
          }
        }
      }

      uint sect = outproc->sector;
       Store user code, data, and heap in disk 
      for(uint i = 0; 
      (i < outproc->sz) && (sect < ((outproc->sz / PGSIZE) * 8)); 
      i+=PGSIZE, sect+=8) {
         Get next PDE 
        pte = walkswap(outproc->pgdir, (void *) i);
         Get Kernel Virtual Address to get data for disk 
        addr = P2V(PTE_ADDR(*pte));
         Write data to disk 
        procbuf = bread(devno, sect);
        memmove(procbuf->data, addr, BSIZE);
        for(uint k = sect+1; k < sect+8; ++k) {
          bwrite(procbuf);
          brelse(procbuf);
          procbuf = bread(devno, k);
          memmove(procbuf->data, addr+=BSIZE, BSIZE);
        }
         Free user mem page 
        kfree(addr);
      }

       Store user stack in disk 
      for(uint i = (KERNBASE - (outproc->stack_sz*PGSIZE)); 
      (i < KERNBASE) && (sect < ((outproc->stack_sz  * 8) + ((outproc->sz / PGSIZE) * 8))); 
      i+=PGSIZE, sect+=8) {
         Get next PDE 
        pte = walkswap(outproc->pgdir, (void *) i);
         Get Kernel Virtual Address to get data for disk 
        addr = P2V(PTE_ADDR(*pte));
         Write data to disk 
        procbuf = bread(devno, sect);
        memmove(procbuf->data, addr, BSIZE);
        for(uint k = sect+1; k < sect+8; ++k) {
          bwrite(procbuf);
          brelse(procbuf);
          procbuf = bread(devno, k);
          memmove(procbuf->data, addr+=BSIZE, BSIZE);
        }
         Free user stack page 
        kfree(addr);
      }

       Store kernel stack in disk 
       Write data to disk 
      addr = outproc->kstack;
      procbuf = bread(devno, sect);
      memmove(procbuf->data, addr, BSIZE);
      for(uint k = sect+1; k < sect+8; ++k) {
        bwrite(procbuf);
        brelse(procbuf);
        procbuf = bread(devno, k);
        memmove(procbuf->data, addr+=BSIZE, BSIZE);
      }
       Free kernel stack page
      kfree(outproc->kstack);

       Free page directory 
      freevm(outproc->pgdir);
      
      // swapped in proc set swapped to 0 and t to 0
       Test if target amount reached 
      freesz -= (dspace / 8) * PGSIZE;
      if(freesz <= 0 && inproc) {
         Memory goal reached. Swap in page 
         Allocate page directory 

      } else if(freesz <= 0 && !inproc) {
         Wakeup Kalloc. Enough memory has been freed for it to continue 
        wakeup(&swapp);
        break;
      }
    }
do_nothing:
  } 
}
*/
void
daemonsinit(void) {
  /* Alerts console every 100 ticks */
  //kfork(ticktock);
  /* Manages movement of Processes between RAM and Disk */
  //kfork(swap);
}
  