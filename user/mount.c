#include "kernel/types.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    mkdir("test");
    mount();
    
    exit(0);
}