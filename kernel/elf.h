#ifndef ELF_H
#define ELF_H

// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint magic;       // must equal ELF_MAGIC
  uchar elf[12];
  ushort type;      // Type of file, shared object, etc..
  ushort machine;   // Type of machine (i386, ARM, MIPS, etc...)
  uint version;
  uint entry;       // Virtual Address where program begins execution 
  uint phoff;
  uint shoff;
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

// Program section header
struct proghdr {
  uint type;    // Type of segment 
  uint off;     // Offset of segment in ELF file
  uint vaddr;   // Virtual Address where the segment is to be loaded
  uint paddr;   // Physical address where the segment is to be loaded
  uint filesz; 
  uint memsz;
  uint flags;
  uint align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4

#endif // ELF_H
