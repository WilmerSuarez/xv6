#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    uint fd_d2, fd_d3, stdout = 1;
    char buf[19];

    // ---------------------------- Disk 2 ---------------------------- //
    printf(stdout, "---------- Test Disk 2 ----------\n");
    /* Open disk2 */
    fd_d2 = open("disk2", O_RDWR);
    printf(stdout, "Opening disk2... \nFD: %d, Mode: O_RDWR\n", fd_d2);
    /* Write disk2 */
    printf(stdout, "Writing to disk2...\n");
    write(fd_d2, "Hello from disk 2!", 19);
    lseek(fd_d2, 0, SEEK_SET);
    /* Read disk2 */
    read(fd_d2, buf, 19);
    printf(stdout, "Reading from disk2: \"%s\"\n", buf);

    // ---------------------------- Disk 3 ---------------------------- //
    /* Open disk3 */
    printf(stdout, "---------- Test Disk 3 ----------\n");
    fd_d3 = open("disk3", O_RDWR);
    printf(stdout, "Opening disk3... \nFD: %d, Mode: O_RDWR\n", fd_d3);
    /* Write disk3 */
    printf(stdout, "Writing to disk3...\n");
    write(fd_d3, "Hello from disk 3!", 19);
    lseek(fd_d3, 0, SEEK_SET);
    /* Read disk3 */
    read(fd_d3, buf, 19);
    printf(stdout, "Reading from disk3: \"%s\"\n", buf);

    printf(stdout, "\nClosing disk2 & disk3\n");
    close(fd_d2);
    close(fd_d3);

    exit(0);
}
