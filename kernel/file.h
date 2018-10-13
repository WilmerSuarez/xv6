#ifndef FILE_H
#define FILE_H

// Major device numbers - Device Driver
#define CONSOLE 1
#define UART 2

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int); // Reads from a device
  int (*write)(struct inode*, char*, int); // Writes to a device
};

extern struct devsw devsw[];

// Table that holds device instantiation information
struct deviceinit {
  char *name;
  uint majordn;
  uint minordn;
};

extern struct deviceinit deviceinit[NDEV];

#endif // FILE_H
