// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "kernel/file.h"
#include "user.h"
#define NUMDEV 3

char *argv[] = { "sh", 0 };
uint pids[NUMDEV];  // Array of child PIDs
uint n = NUMDEV;    // Number of devices

struct deviceinit deviceinit[NDEV] = {
  { .name = "console", .majordn = 1, .minordn = 1 },
  { .name = "com1", .majordn = 2, .minordn = 1 },
  { .name = "com2", .majordn = 2, .minordn = 2 }
};

static void
opendev(uint dev) {
  if(open(deviceinit[dev].name, O_RDWR) < 0){
    mknod(deviceinit[dev].name, deviceinit[dev].majordn, deviceinit[dev].minordn);
    open(deviceinit[dev].name, O_RDWR);
  }
}

int
main(void) {
  int wpid, estatus;

  for(uint i = 0; i < (NUMDEV); ++i) {
    close(0);
    close(1);
    close(2);
    if((pids[i] = fork()) < 0) {
      printf(1, "init: fork failed\n");
      exit(1);
    } else if (pids[i] == 0) {
      opendev(i);
      dup(0);  // stdin
      dup(1);  // stdout
      dup(2);  // stderr
      printf(1, "init: starting sh\n");
      exec("sh", argv);
      printf(1, "sh exiting\n");
      exit(1);
    }
  }

  for(;;) {
    wpid = wait(&estatus);
    for(uint i = 0; i < NUMDEV; ++i) {
      if(wpid == pids[i]) {
        if((pids[i] = fork()) < 0) {
          printf(1, "init: re-fork failed\n");
          exit(1);
        } else if (pids[i] == 0) {
          opendev(i);
          dup(0);  // stdin
          dup(1);  // stdout
          dup(2);  // stderr
          printf(1, "init: re-starting sh\n");
          exec("sh", argv);
          printf(1, "sh exiting\n");
          exit(1);
        }
      }
    }
  }
  
}
