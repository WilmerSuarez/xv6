// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

#define DISK_NUM      4

static struct spinlock idelock;

/* idequeue points to the buf now being read/written to the disk */
/* idequeue->qnext points to the next buf to be processed */
/* idelock must be held while manipulating queue */
static struct buf *idequeue;

static int havedisk[DISK_NUM];
static void idestart(struct buf*, uint, uint);

/* Wait for IDE controllers to become ready */
static int
idewait(int checkerr, uint ba) {
  int r;

  /* Wait for controller to be ready by testing the command/status register */
  while(((r = inb(ba + 7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY);

  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

/* Check if disk is present */
static void
diskp(uint dnum, uint ba, uint d) {
  outb((ba + 6), 0xe0 | (d<<4));
  for(uint i = 0; i < 1000; ++i) {
    if(inb(ba + 7) != 0){
      havedisk[dnum] = 1;
      break;
    }
  }
}

int
ideread(struct inode *ip, char *dst, int n, uint off) { 
  uint block_addr, total, n_bytes;
  struct buf *b;

  /* Test for End of File */
  if((off > (DSIZE*BSIZE)) || (off+n > (DSIZE*BSIZE)))
    return -1;

  iunlock(ip);
  
  /* Read n bytes from disk */
  for(total = 0; total < n; total+=n_bytes, off+=n_bytes, dst+=n_bytes) {
    /* Calculate disk block address */ 
    block_addr = ((off/BSIZE) % DSIZE);
    /* Read associated block */
    b = bread(ip->minor, block_addr);
    /* Determine hfow many bytes to read next */
    n_bytes = min(n - total, BSIZE - off%BSIZE);
    /* Read and update data buffer from block */
    memmove(dst, b->data + off%BSIZE, n_bytes);
    brelse(b);
  }

  ilock(ip);

  /* Return number of bytes read - used for updating file offset */
  return n; 
}

int
idewrite(struct inode *ip, char *buf, int n, uint off) { 
  uint block_addr, total, n_bytes;
  struct buf *b;

  /* Test for End of File */
  if((off > (DSIZE*BSIZE)) || (off+n > (DSIZE*BSIZE)))
    return -1;

  /* Prevent Writing to disk 0 or disk 1*/
  if(ip->minor == 0 || ip->minor == 1) 
    return -1;

  iunlock(ip);

  /* Read n bytes from disk and write to Source */
  for(total = 0; total < n; total+=n_bytes, off+=n_bytes, buf+=n_bytes) {
    /* Calculate disk block address */ 
    block_addr = ((off/BSIZE) % DSIZE);
    /* Read associated block */
    b = bread(ip->minor, block_addr);
    /* Determine how many bytes to write next */
    n_bytes = min(n - total, BSIZE - off%BSIZE);
    /* Write data from buffer param to block */
    memmove(b->data + off%BSIZE, buf, n_bytes);
    /* Write updated block back to disk */
    bwrite(b);
    brelse(b);
  }

  ilock(ip);

  return n;
}

void
ideinit(void) {
  initlock(&idelock, "ide");
  
  /* Initialize IDE Device Switch entry */
  devsw[IDE].write = idewrite;
  devsw[IDE].read = ideread;
  
  /* Enable IRQ 14 & wait for Primary IDE controller to be ready */
  ioapicenable(IRQ_IDE_P, ncpu - 1);
  idewait(0, BASE_ADDR1);

  havedisk[0] = 1;
  /* Check if disk 1 is present */
  diskp(1, BASE_ADDR1, 1);
  
  /* Enable IRQ 15 & wait for Secondary IDE controller to be ready */
  ioapicenable(IRQ_IDE_S, ncpu - 1);
  idewait(0, BASE_ADDR3);
  
  /* Check if disk 2 is present */
  diskp(2, BASE_ADDR3, 0);
  /* Check if disk 3 is present */
  diskp(3, BASE_ADDR3, 1);

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b, uint channel, uint channelctr) {
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
    
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) 
    panic("idestart");

  idewait(0, channel);
  outb(channelctr, 0);                  // generate interrupt
  outb(channel + 2, sector_per_block);  // number of sectors
  outb(channel + 3, sector & 0xff);
  outb(channel + 4, (sector >> 8) & 0xff);
  outb(channel + 5, (sector >> 16) & 0xff);
  outb(channel + 6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(channel + 7, write_cmd);
    outsl(channel, b->data, BSIZE/4);
  } else {
    outb(channel + 7, read_cmd);
  }
}

/*
  IDE Interrupt handler
  Invoked whenever the disk driver has completed a request
  For both, Primary & Secondary controllers 
  Controller Determined by parameters:
    channel - primary/secondary channel
    channelctrl - primary/secondary channel control port
*/
void
ideintr(uint channel, uint channelctr) {
  struct buf *b;
  // First queued buffer is the active request.
  acquire(&idelock);

  // If idequeue is empty (no disk access requests)
  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  // Whatever was at the top of the queue has been serviced, so move to next entry in queue
  idequeue = b->qnext;

  // Read data if needed
  if(!(b->flags & B_DIRTY) && idewait(1, channel) >= 0)
    insl(channel, b->data, BSIZE/4);

  // Wake process waiting for this buf
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  sem_V(&b->sem);

  // Start disk on next buf in queue.
  // If idequeue is not empty (more requests to service)
  if(idequeue != 0)
    idestart(idequeue, channel, channelctr);
  
  release(&idelock);
}

//PAGEBREAK!
/* 
  Sync buf with disk.
    - If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
    - Else if B_VALID is not set, read buf from disk, set B_VALID.
*/
void
iderw(struct buf *b) {
  struct buf **pp; 

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID) // If B_VALID set & B_DIRT NOT SET
    panic("iderw: nothing to do");
  for(uint i = 0; i < DISK_NUM; ++i) {
    if(b->dev != 0 && !havedisk[i])
      panic("iderw: an ide disk is not present");
  }

  // Initialize semaphore
  sem_init(&b->sem, 0);

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to corresponding idequeue.
  b->qnext = 0;
  // idequeue is FIFO. Iterate to end of queue and append buf. 
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext);
  *pp = b;

  // If first request to access disk
  // If only one buffer in idequeue (first element in queue), execute its request
  if(idequeue == b) {
    if(b->dev < 2) {
      idestart(b, BASE_ADDR1, BASE_ADDR2);
    } else {
      idestart(b, BASE_ADDR3, BASE_ADDR4);
    }
  }
  
  // If ^ failed... idequeue not empty
  // disk already started taking requests from queue. No need to start again.
  release(&idelock);
  
  // sem_P will put Processes asking for disk access to sleep until 
  // request has been fulfilled (B_VALID is set)
  sem_P(&b->sem);
}
