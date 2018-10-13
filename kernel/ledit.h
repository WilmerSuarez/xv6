#ifndef LEDIT_H
#define LEDIT_H
#include "types.h"

#define INPUT_BUF 128
#define BACKSPACE 0x100

#define C(x)  ((x)-'@')  // Control-x
 
struct input {
  char buf[INPUT_BUF];
  uint r;   // Read index
  uint w;   // Write index
  uint e;   // Edit index
  int port; // 
};

/*
*   Allows editing of a line of input before 'ENTER' is 
*   pressed. Used by all shell sessions. 
*/
void ledit(int (*)(void), void (*)(int), int *,  struct input *);

#endif // LEDIT_H