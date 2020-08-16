#ifndef __MMAP_H__
#define __MMAP_H__

#define MAP_FAILED  (void*)-1   // Error when Memory Mapping
#define MAP_SHARED  1   
#define MAP_FILE    2

#define MAPMAX 10               // Max number of mappings per process

#define E_W   0x00000002        // Write Bit of Error Word in Trap Frame

#endif // __MMAP_H__