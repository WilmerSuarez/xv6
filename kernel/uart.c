// Intel 8250 serial port (UART).
#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "ledit.h"

#define COM1 0x3f8
#define COM2 0x2f8
#define NCOM 2

static int uart;    // is there a uart?
static int com;     // COM1 or COM2?

static struct input input_uart[NCOM];

static struct {
  struct spinlock lock;
  int locking;
} uart_lk;

// Outputs characters to COM1
static void
uartputc(int c, int coms) {
  int i;

  if(!uart)
    return;
  for(i = 0; i < 128 && !(inb(coms + 5) & 0x20); i++)
    microdelay(10);
  if(c == '\n') {
    outb(coms + 0, c);
    c = '\r';
    outb(coms + 0, c);
  } else {
    outb(coms + 0, c);
  }
}

int
uartwrite(struct inode *ip, char *buf, int n) {
  int i;

  iunlock(ip);
  acquire(&uart_lk.lock);
  for(i = 0; i < n; i++) {
    if(ip->minor == 1) {
      uartputc(buf[i] & 0xff, COM1);
    } else {
      uartputc(buf[i] & 0xff, COM2);
    }
  }
    
  release(&uart_lk.lock);
  ilock(ip);

  return n;
}

int
uartread(struct inode *ip, char *dst, int n) {
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&uart_lk.lock);
  while(n > 0){
    while(input_uart[ip->minor-1].r == input_uart[ip->minor-1].w){
      if(myproc()->killed){
        release(&uart_lk.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input_uart[ip->minor-1].r, &uart_lk.lock);
    }
    c = input_uart[ip->minor-1].buf[input_uart[ip->minor-1].r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input_uart[ip->minor-1].r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&uart_lk.lock);
  ilock(ip);

  return target - n;
}

static void
initcom(int coms) {
  char *p;

  // Turn off the FIFO
  outb(coms+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(coms+3, 0x80);    // Unlock divisor
  outb(coms+0, 115200/9600);
  outb(coms+1, 0);
  outb(coms+3, 0x03);    // Lock divisor, 8 data bits.
  outb(coms+4, 0);
  outb(coms+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if(inb(coms+5) == 0xFF)
    return;
  uart = 1;

  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(coms+2);
  inb(coms+0);

  initlock(&uart_lk.lock, "uart");

  // Initialize UART Device Switch entry
  devsw[UART].write = uartwrite;
  devsw[UART].read = uartread;
  uart_lk.locking = 1;

  if(coms == 0x3f8) {
    ioapicenable(IRQ_COM1, 0);
  } else {
    ioapicenable(IRQ_COM2, 0);
  }

  // Announce that we're here.
  for(p="xv6...\r\n"; *p; p++)
    uartputc(*p, coms);
}

void
uartinit(void) {
  initcom(COM1);
  initcom(COM2);
}

static void 
uart_putc(int c) {
  if(c == BACKSPACE) {
    uartputc('\b', input_uart[com].port);
    uartputc(' ', input_uart[com].port);
    uartputc('\b', input_uart[com].port);
  } else {
    uartputc(c, input_uart[com].port);
  }
}

// Gets the character sent to COM1
static int
uartgetc(void) {
  if(!uart)
    return -1;
  if(!(inb(input_uart[com].port + 5) & 0x01))
    return -1;
  return inb(input_uart[com].port + 0);
}

// IRQ_COM1 interrupt handler
void
uartintr(int dev) {
  int doprocdump = 0;

  com = dev - 1;
  
  if(com == 0) {
    input_uart[com].port = COM1;
  } else {
    input_uart[com].port = COM2;
  }

  acquire(&uart_lk.lock);
  ledit(uartgetc, uart_putc, &doprocdump, &input_uart[com]);
  release(&uart_lk.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. uart_lk.lock held
  }
}
