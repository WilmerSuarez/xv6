#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "mmap.h"

/*  
  Find an umapped address range of size 'length' within 
  the user address space of the current process.
  Return the starting address.
*/
static void *
get_unmapped(struct file *f, uint length, uint *e_addr) {
  struct proc *curproc = myproc();
  void *s_addr;
  /* Previously Mapped Node */
  struct map_node *prev_m_node = &curproc->m.maps[curproc->m.map_count - 1];
  
  /* Get Unmapped Range */
  if(curproc->m.map_count == 0) { /* First Mapping */
    /* Ending Address */
    *e_addr = MAPPINGSTART - length; 
    s_addr = (void *)*e_addr; /* Mapping "Grows" Downward */
    *e_addr = MAPPINGSTART; 
    /* First Available Address of Mapped Region */
    return s_addr;
  } else {
    *e_addr = (uint)prev_m_node->s_addr - length;
    /* Bottom of Previous Mapping */
    s_addr = (void *)*e_addr; /* Mapping "Grows" Downward */
    *e_addr = (uint)prev_m_node->s_addr;
    return s_addr;
  }
}

/*
  Mapping a file.
  Selects a currently unused address range of size length 
  within the calling process' address space.
*/
void *
mmap_file(struct file *f, uint length, uint offset, int flags) {
  struct stat s;
  void *s_addr = 0;
  uint e_addr = 0;
  struct proc *curproc = myproc();

  /* Process reached Mapping limit */
  if(curproc->m.map_count == MAPMAX) 
    return MAP_FAILED;

  /* Mapped region length cannot be 0 */
  if(!length) 
      return MAP_FAILED;

  /* Get file info */
  filestat(f, &s);

  /* File type must be FD_INODE */
  if(f->type != FD_INODE) 
    return MAP_FAILED;

  /* Cannot map larger than the size of the file */
  if(!((length+offset) <= s.size)) 
    return MAP_FAILED;
    
  /* Length must be page aligned */
  length = PGROUNDUP(length);

  /* 
    Find an umapped range within the user 
    address space of the specified length.
  */
  s_addr = get_unmapped(f, length, &e_addr);

  /* Store mapping Information */
  curproc->m.map_count++;  // Increment number of mappings for current process
  struct map_node n = {
    .dirty = 0,
    .mapped = 1,
    .s_addr = s_addr,
    .e_addr = (void *)e_addr,
    .length = length,
    .file = f,
    .offset = offset,
    .flags = flags
  };
  /* Add to Mapping List */
  curproc->m.maps[curproc->m.map_count - 1] = n;

  /* 
    Return start address of newly mapped region.
  */
  return s_addr;
}

/*
  Anonymous Mapping.
  Selects a currently unused address range of size length 
  within the calling process' address space.
*/
void *
mmap_anon(uint length, int flags) {
  void *s_addr = 0;
  uint e_addr = 0;
  struct proc *curproc = myproc();

  /* Process reached Mapping limit */
  if(curproc->m.map_count == MAPMAX) 
    return MAP_FAILED;

  /* Mapped region length cannot be 0 */
  if(!length) 
      return (void *)-1;

  /* Length must be page aligned */
  length = PGROUNDUP(length);

  /* 
    Find an umapped range within the user 
    address space of the specified length.
  */
  s_addr = get_unmapped(0, length, &e_addr);
  
  /* Store mapping Information */
  curproc->m.map_count++;  // Increment number of mappings for current process
  struct map_node n = {
    .dirty = 0,
    .mapped = 1,
    .s_addr = s_addr,
    .e_addr = (void *)e_addr,
    .length = length,
    .file = 0,
    .offset = 0,
    .flags = flags
  };
  /* Add to Mapping List */
  curproc->m.maps[curproc->m.map_count - 1] = n;

  /* 
    Return start address of newly mapped region.
  */
  return s_addr;
}

/*
  Unmaps a perviously mapped reagion starting 
  at addr.
*/
int
munmap(void *addr) {
  struct proc *curproc = myproc();
  pde_t *pde;
  pte_t *pgtab;
  pte_t *pte;
  uint phyaddr, length = 0, i;

  /* Get given address mapping info */
  for(i = 0; i < MAPMAX; ++i) {
    if(curproc->m.maps[i].s_addr == addr) {
      length = curproc->m.maps[i].length;
      break;
    }
  }

  /* Unmap region previously mapped by mmap() */
  for(uint i = 0; i < (length/PGSIZE); ++i) {
    /* Get PDE addr */
    pde = &curproc->pgdir[PDX(addr + (i * PGSIZE))];
    /* Get Page Table addr */
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
    /* Get Page Table Entry */
    pte = &pgtab[PTX(addr + (i * PGSIZE))];
    if((*pte & PTE_P) != PTE_P) {
      return -1; /* Page Table Entry not present */
    }
    /* Get the address of physical memory page */
    phyaddr = PTE_ADDR(*pte);
    /* Free physical memory page */
    kfree((char *)P2V(phyaddr));
    *pte = 0;
  }

  return 0;
}
