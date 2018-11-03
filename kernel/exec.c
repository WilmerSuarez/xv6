#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv) {
  // path: path of the executable 
  // argv: command line arguments

  char *s, *last, *mem;
  int i, off;
  uint argc, sz, stack_sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  // Open the named binary path
  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip); // Lock the inode
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)  // Check that the file contains an ELF binary
    goto bad;

  if((pgdir = setupkvm()) == 0) // Setup new page table (with kernel part) and allocate new page directory
    goto bad;

  // Load program into memory.
  sz = 0; // Initialize user address space size
  // Parse through all the elf program headers
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD) // Only load into memory segments of type LOAD
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)  // Does the sum overflow a 32 bit integer. Prevents kernel privileges for user programs
      goto bad;
    // allocuvm() allocates physical memory for each ELF segment and page tables
    // Checks that the virtual address is below KERNBASE
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    // loaduvm() loads each segment into memory @ location ph.vaddr (code/text & data)
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate a page of memory for the User Stack 
  // below KERNBASE.
  sp = KERNBASE - PGSIZE;
  /* Increment the amount of page allocation needed - used by swap daemon */
  mem_amount++;
  // Allocate one page (4096 Bytes) of physical memory
  if((mem = kalloc()) == 0) { 
    cprintf("Could not allocate initial Stack Page");
    return -1;
  }  
  // Initialize Physical Page to 0
  memset(mem, 0, PGSIZE); 
  // Map the page address to a physical page.
  if(mappages(pgdir, (char*)sp, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0)
    goto bad;
  // Initialize stack size and point to start of stack (Just under KERNBASE). 
  stack_sz = 1;
  sp = KERNBASE - 1;

  // Push argument strings, one at a time, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0) // Copy argument string to top of stack one at a time
      goto bad;
    ustack[3+argc] = sp; // Record pointers to argument strings
  }
  ustack[3+argc] = 0; // Null pointer at the end of main argv

  // First 3 entries in User Stack
  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  // Push the three entires to User stack
  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image. 
  // Fill process structrure
  // Fill process trap frame before starting the program
  oldpgdir = curproc->pgdir; // old image
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->stack_sz = stack_sz; 
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc); // Install new image
  freevm(oldpgdir); // Free old image
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
