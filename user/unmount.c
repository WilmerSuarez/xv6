#include "kernel/types.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    uint stderr = 2;

    /* Must receive mount point argument */
    if(argc < 2) {
        printf(stderr, "Usage: unmount [mount point]\n");
        exit(1);
    }

    /* Unmount the device */
    if(unmount(argv[1]) < 0) {
        printf(stderr, "Unmounting failed. See console.\n");
    }

    exit(0);
}