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
#include "semaphore.h"

#define COM1 0x3f8 // COM1 port num
#define COM2 0x2f8 // COM2 port num
#define NCOM 2     // Number of COM ports

static int uart;    // is there a uart?
static int com;     // COM1 or COM2 (0 or 1)?

static struct input uart_input_buff[NCOM] = { 0 };
static struct output uart_out_buff[NCOM] = { 
  { .rear = 127, .front = 0, .len = 0, .busy = 0 },
  { .rear = 127, .front = 0, .len = 0, .busy = 0 }  
};

/* UART lock */
static struct {
  struct spinlock lock;
  struct semaphore sem;
  int locking;
} uart_lk;

static void 
uartstart(int coms) {
  // If buffer busy flag set or buffer is empty
  if(uart_out_buff[com].busy || uart_out_buff[com].len == 0)  
    return;
  // If transmission is busy 
  if(!(inb(coms+5) & 0x20)) {
    uart_out_buff[com].busy = 1;
    return;
  }
  /* Dequeue next character */
  char c = uart_out_buff[com].buf[uart_out_buff[com].front];  
  uart_out_buff[com].front = (uart_out_buff[com].front + 1) % OUTPUT_BUFF;  
  uart_out_buff[com].len--; // Decrease length of Queue

  if(c == '\n') {
      outb(coms + 0, c);
      c = '\r';
      outb(coms + 0, c);
    } else {
      outb(coms + 0, c);
  }

}

// Outputs characters to COM1 or COM2
static void
uartputc(int c, int coms) {
  struct proc *curproc = myproc();

  #ifdef DEBUG
    cprintf("In uartputc(): COM PORT: %x, char: %d buflen: %d\n", coms, c, uart_out_buff[com].len);
  #endif
  // If UART not initialized 
  if(!uart)
    return;
  // If output buffer is full and there is a process running, go to sleep
  while(uart_out_buff[com].len >= OUTPUT_BUFF && curproc) {
    release(&uart_lk.lock);
    #ifdef DEBUG
      cprintf("sleeping\n");
    #endif
    sem_P(&uart_lk.sem);  // wait for buffer
    acquire(&uart_lk.lock);
  }
  uart_out_buff[com].rear = (uart_out_buff[com].rear + 1) % OUTPUT_BUFF; 
  uart_out_buff[com].buf[uart_out_buff[com].rear] = c;  // Enqueue character
  uart_out_buff[com].len++; // Increase length of queue
  
  uartstart(coms);
}

static void 
uart_putc(int c, int coms) {
  if(c == BACKSPACE) {
    uartputc('\b', coms);
    uartputc(' ', coms);
    uartputc('\b', coms);
  } else {
    uartputc(c, coms);
  }
}

// Gets the character sent to COM1 or COM2
static int
uartgetc(int coms) {
  if(!uart)
    return -1;
  if(!(inb(coms + 5) & 0x01))
    return -1;
  return inb(coms + 0);
}

int
uartwrite(struct inode *ip, char *buf, int n) {
  int i;

  iunlock(ip);
  acquire(&uart_lk.lock);
  for(i = 0; i < n; i++) {
    if(ip->minor == 1) {
      uart_putc(buf[i] & 0xff, COM1);
    } else if(ip->minor == 2){
      uart_putc(buf[i] & 0xff, COM2);
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
    while(uart_input_buff[ip->minor-1].r == uart_input_buff[ip->minor-1].w){
      if(myproc()->killed){
        release(&uart_lk.lock);
        ilock(ip);
        return -1;
      }
      sleep(&uart_input_buff[ip->minor-1].r, &uart_lk.lock);
    }
    c = uart_input_buff[ip->minor-1].buf[uart_input_buff[ip->minor-1].r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        uart_input_buff[ip->minor-1].r--;
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

  // Initialize Semaphore
  sem_init(&uart_lk.sem, 0);

  // Turn off the FIFO
  outb(coms+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(coms+3, 0x80);    // Unlock divisor
  outb(coms+0, 115200/9600);
  outb(coms+1, 0);
  outb(coms+3, 0x03);    // Lock divisor, 8 data bits.
  outb(coms+4, 0);
  outb(coms+1, 0x03);    // Enable RX & TX interrupts.

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
  for(p="xv6...\r\n"; *p; p++) {
    uartputc(*p, coms);
  }
}

void
uartinit(void) {
  initcom(COM1);
  initcom(COM2);
}

static void rx_tx(uint IIR, int coms) {
  int doprocdump = 0;

  // RX or TX?  
  if(IIR & RX) {
    // RX
    acquire(&uart_lk.lock);
    ledit(uartgetc, uart_putc, coms, &doprocdump, &uart_input_buff[com]);
    release(&uart_lk.lock);
    if(doprocdump) {
      procdump();  // now call procdump() wo. uart_lk.lock held
    }
  } 
  if(IIR & TX) {
    // TX
    #ifdef DEBUG
      cprintf("Transmitting\n");
    #endif
    uart_out_buff[com].busy = 0;
    uartstart(coms);
    #ifdef DEBUG
      cprintf("Waking up\n");
    #endif
    sem_V(&uart_lk.sem);  // Wakeup processes waiting for output
  } 
}

// IRQ_COM1 interrupt handler
void
uartintr(int dev) {
  com = dev;
  uint IIR = 0;

  // Check which port caused the interrupt
  if(!com) {
    IIR = inb(COM1+2);
    rx_tx(IIR, COM1);
  } else {
    IIR = inb(COM2+2);
    rx_tx(IIR, COM2);
  } 
}
