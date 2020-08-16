#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/mmap.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    int stdout = 1, stderr = 2;
    char *addr; /* Mapped Region Starting Address */
    uint l = 4096 * 5; /* Length of Mapped Region */

    if((addr = mmap(0, l, 0, 0)) == MAP_FAILED) {
        printf(stderr, "Mapping failed\n");
        exit(1);
    }

    printf(stdout, "Mapped Region Start Address: %x\n", addr);
    
    char *str = "Hello, my name is Wilmer!";

    /* Write string to mapped region */
    for(uint i = 0; i < strlen(str); ++i) {
        addr[i] = str[i]; 
    }

    /* Read the modified region */ 
    for(uint i = 0; i < strlen(addr); ++i) {
        printf(stdout, "%c", addr[i]);
    }
    printf(stdout, "\n");

    /* Unmap */
    if(munmap(addr) < 0) {
        printf(stderr, "Unmapping failed\n");
        exit(1);
    }

    /* Attempt to access mapping again - Expecting Failure */
    printf(stdout, "%c", addr[0]);

    exit(0);
}