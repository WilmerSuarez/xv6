// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "kernel/file.h"
#include "user.h"
#define NUMDEVTBL 3

char *argv[] = { "sh", 0 };

struct deviceinit deviceinit[NDEV] = {
  { .name = "console", .majordn = 1, .minordn = 1 },
  { .name = "com1", .majordn = 2, .minordn = 1 },
  { .name = "com2", .majordn = 2, .minordn = 2 }
};

int
main(void) {
  int pid, wpid, estatus;
    for(uint i = 0; i < (NUMDEVTBL); ++i) {
      close(0);
      close(1);
      close(2);
      if(open(deviceinit[i].name, O_RDWR) < 0){
        mknod(deviceinit[i].name, deviceinit[i].majordn, deviceinit[i].minordn);
        open(deviceinit[i].name, O_RDWR);
      } 

      pid = fork();

      if(pid < 0) {
        printf(1, "init: fork failed\n");
        exit(1);
      }

      if(pid == 0) {
        dup(0);  // stdin
        dup(1);  // stdout
        dup(2);  // stderr
        // Make sure shell never exits
        for(;;) {
          printf(1, "init: starting sh\n");
          exec("sh", argv);
          printf(1, "sh exiting\n");
          exit(1);
        }
      }
    }

  while((wpid=wait(&estatus)) >= 0 && wpid != pid)
    printf(1, "zombie!\n"); 

}
