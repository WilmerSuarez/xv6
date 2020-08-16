#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
  int pid, estatus;

  pid = fork();
  if(pid > 0) {
    pid = wait(&estatus);
    *(int*)-1 = 14;
  } else if(pid == 0) {
    printf(1, "[In Child Process]\n");
  } else {
    printf(1, "Fork Error\nd");
    exit(1);
  }

  exit(0);
}
