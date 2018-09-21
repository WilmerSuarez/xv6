#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
  struct rtcdate r;

  getdate(&r);

  printf(1, "Time(UTC): %d:%d:%d %d/%d/%d\n", r.hour, r.minute, r.second, r.month, r.day, r.year);

  exit();
}
