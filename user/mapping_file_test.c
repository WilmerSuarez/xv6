#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/mmap.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    int stdout = 1, stderr = 2;
    
    /* Open README file */
    int fd = open("./README", O_RDONLY);
    char *addr; /* File starting address */
    struct stat s;

    /* Get file info */
    fstat(fd, &s);

    /* Map File */
    if((addr = mmap(fd, s.size, 0, MAP_FILE)) == MAP_FAILED) {
        printf(stderr, "Mapping failed\n");
        exit(1);
    }

    printf(stdout, "Mapped Region Start Address: %x\n", addr);

    /* Read File */
    for(uint i = 0; i < s.size; ++i) {
        printf(stdout, "%s", addr);
    }

    exit(0);
}