#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
  struct rtcdate r;
  
  if(argc < 1 || argc > 2) {
    exit(1); // Too little or too many arguments
  }

  if(argc == 1) {
    // Valid time
    r.hour = 11;
    r.minute = 59;
    r.second = 59;
    r.month = 9;
    r.day = 21;
    r.year = 2018;
  } else if(argc == 2) {
    // Invalid time 
    r.hour = 25;
    r.minute = 64;
    r.second = 65;
    r.month = 16;
    r.day = 40;
    r.year = 20000;
  } else {
    exit(1); // Invalid argument
  }

  if(setdate(&r) < 0) {
    exit(1); // Invalid time
  }

  getdate(&r);
  printf(1, "Time: %d:%d:%d %d/%d/%d\n", r.hour, r.minute, r.second, r.month, r.day, r.year);

  exit(0);
}
