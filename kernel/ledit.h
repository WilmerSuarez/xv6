#ifndef LEDIT_H
#define LEDIT_H
#include "types.h"

#define INPUT_BUF     128
#define OUTPUT_BUFF   128
#define BACKSPACE     0x100
#define TX            2
#define RX            4
#define RX_TX         6

#define C(x)  ((x)-'@')  // Control-x
 
struct input {
  char buf[INPUT_BUF];
  uint r;   // Read index
  uint w;   // Write index
  uint e;   // Edit index
};

struct output {
  char buf[OUTPUT_BUFF];
  uint rear;  // End of queue
  uint front; // Beginning of queue
  uint len;   // Number of elements in the buffer
  uint busy;  // busy flag for transmission
};

/*
*   Allows editing of a line of input before 'ENTER' is 
*   pressed. 
*/
void ledit(int (*)(int), void (*)(int, int), int, int *,  struct input *inp);

#endif // LEDIT_H