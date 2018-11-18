#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "user.h"

#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)

/* Number of Inodes */
#define NINODES 200

/* 
  Disk layout: 
  [ boot block | sb block | log | inode blocks | free bit map | data blocks ]
*/
int nbitmap = FSSIZE/(BSIZE*8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

/* Function Prototypes */
void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint, struct dinode *);
void rsect(uint, void *);
uint ialloc(ushort);
void iappend(uint, void *, int);
void bzero(void *, int);
void bcopy(void *, void *, int);

/* Convert Unsigned Short datatype to intel byte order */
ushort
xshort(ushort x) {
    ushort y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

/* Convert Unsigned Integer datatype to intel byte order */
uint
xint(uint x) {
    uint y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

uint stdout = 1, stderr = 2;

int
main(int argc, char *argv[]) {
    uint rootino, off;
    struct dirent de;
    char buf[BSIZE];
    struct dinode din;

    static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    /* Make sure user passsed in disk node name argument */
    if(argc < 2){
        printf(stderr, "Usage: umkfs [disk_node]\n");
        exit(1);
    }

    /* Open Disk Node */
    fsfd = open(argv[1], O_RDWR);
    if(fsfd < 0){
      printf(stderr, "Open Error");
      exit(1);
    }

    /* 1 fs block = 1 disk sector */
    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = FSSIZE - nmeta;

    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2+nlog);
    sb.bmapstart = xint(2+nlog+ninodeblocks);

    printf(stdout, "nmeta %d (boot, super, log blocks %d inode blocks %d, bitmap blocks %d) blocks %d total %d\n",
            nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

    /* The first free block that we can allocate */
    freeblock = nmeta;     

    /* Zero out all FS blocks */
    for(uint i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

    /* Write the Super Block to the first block in Disk */
    memset(buf, 0, sizeof(buf)); 
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    /* Allocate Root Directory Inode */
    rootino = ialloc(T_DIR);

    /* Create '.' entry in root Dir */
    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    /* Create '..' entry in root Dir */ 
    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));

    /* Fix size of Root Directory Inode */
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off/BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock);

    exit(0);
}

void 
bzero(void *s, int n) {
  memset(s, 0, n);
}

void
bcopy(void *src, void *dst, int n) {
  memmove(dst, src, n);
}

void
wsect(uint sec, void *buf) {
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    printf(stderr, "lseek Error");
    exit(1);
  }
  if(write(fsfd, buf, BSIZE) != BSIZE){
    printf(stderr, "Write Error");
    exit(1);
  }
}

void
winode(uint inum, struct dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  /* Read disk sector bn */
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  /* Write back updated disk sector */
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf) {
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    printf(stderr, "lseek Error");
    exit(1);
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    printf(stderr, "Write Error");
    exit(1);
  }
}

uint
ialloc(ushort type) {
  uint inum = freeinode++;
  struct dinode din;

  /* Zero the inode struct */
  bzero(&din, sizeof(din));
  /* Mark it as allocated */
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void
balloc(int used) {
  uchar buf[BSIZE];
  int i;

  printf(stdout, "balloc: first %d blocks have been allocated\n", used);
  bzero(buf, BSIZE);
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  printf(stdout, "balloc: write bitmap block at sector %d\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n) {
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  while(n > 0){
    fbn = off / BSIZE;
    if(fbn < NDIRECT){
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if(xint(din.addrs[NDIRECT]) == 0){
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn-NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
