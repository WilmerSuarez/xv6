#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user.h"

#define TESTBIT(BMAP, INDEX)    (BMAP[(INDEX/8)] & (1<<(INDEX%8)))

int
main(int argc, char *argv[]) {
    uint fd_d1, stdout = 1;
    uint fblocks = 0, finodes = 0, numbblock;
    uint isize = sizeof(struct dinode);
    struct superblock *sb = malloc(sizeof(struct superblock));

    /* Open Disk 1 */
    fd_d1 = open("disk1", O_RDONLY);

    /* Read Super Block */
    lseek(fd_d1, BSIZE, SEEK_SET); // Start at the Block 1 - SUPER BLOCK
    read(fd_d1, sb, sizeof(struct superblock));

    /* Calculate number of free blocks */
    numbblock = (FSSIZE/(BSIZE*8) + 1); // Number of bitmap blocks
    uchar buf[BSIZE * numbblock]; // Temporary bitmap buffer
    lseek(fd_d1, (sb->bmapstart*BSIZE), SEEK_SET); // Start at 1st Bmap Block
    read(fd_d1, buf, BSIZE * numbblock); // Read bitmap block(s)
    for(uint bi = 0; bi < sb->nblocks; ++bi) {
        if(!TESTBIT(buf, bi))
            fblocks++;
    }

    /* Calculate number of free inodes */
    struct dinode *inode = malloc(isize); // Temporary inode
    lseek(fd_d1, (sb->inodestart*BSIZE), SEEK_SET); // Start at 1st Inode Block
    for(uint in = 0; in < sb->ninodes; ++in) {
        read(fd_d1, inode, isize);
        if(!inode->type) 
            finodes++;
    }

    /* Print current number of free blocks & free inodes */
    printf(stdout, "Number of Free Blocks: %d\nNumber of Free Inodes: %d\n", fblocks, finodes);

    /* Close Disk 1 */
    close(fd_d1);

    exit(0);
}
