#include "ledit.h"
#include "defs.h"

/*
*   Allows edeting of a line of input before 'ENTER' is 
*   pressed. Used by all shell sessions. 
*/
void ledit(int (*getc)(int), void (*putc)(int c, int coms), int coms, int *doprocdump, struct input *inp) {
  int c;

  // Reads the data recevied on console (c = getc()) from function *getc
  while((c = getc(coms)) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      *doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(inp->e != inp->w &&
            inp->buf[(inp->e-1) % INPUT_BUF] != '\n') {
        inp->e--;
        putc(BACKSPACE, coms);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(inp->e != inp->w){
        inp->e--;
        putc(BACKSPACE, coms);
      }
      break;
    default:
      if(c != 0 && inp->e-inp->r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        inp->buf[inp->e++ % INPUT_BUF] = c;
        putc(c, coms);
        if(c == '\n' || c == C('D') || inp->e == inp->r+INPUT_BUF){
          inp->w = inp->e;
          wakeup(&inp->r);
        }
      }
      break;
    }
  }
}
