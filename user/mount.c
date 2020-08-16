#include "kernel/types.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    uint stderr = 2;

    /* Must receive source and mount point arguments */
    if(argc < 3) {
        printf(stderr, "Usage: mount [source] [mount point]\n");
        exit(1);
    }

    /* Mount the device */
    if(mount(argv[1], argv[2]) < 0) {
        printf(stderr, "Mounting failed. See console.\n");
    }
    
    exit(0);
}