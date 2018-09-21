#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
  struct rtcdate r;

  // Right time
  r.hour = 6;
  r.minute = 19;
  r.second = 00;
  r.month = 9;
  r.day = 20;
  r.year = 2018;

  // Wrong time 
  /*
  r.hour = 25;
  r.minute = 64;
  r.second = 65;
  r.month = 16;
  r.day = 40;
  r.year = 20000;*/

  setdate(&r);

  getdate(&r);
  printf(1, "Time: %d:%d:%d %d/%d/%d\n", r.hour, r.minute, r.second, r.month, r.day, r.year);

  exit();
}
