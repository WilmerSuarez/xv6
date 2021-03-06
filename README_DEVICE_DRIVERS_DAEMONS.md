# Device Drivers, Swapping
***
## Fully Interrupt-Driven UART Driver 

The UART driver is made buffered and interrupt-driven 
on both the input(receive) and output(transmit) side. 

**Changes made:**
1. Both the RX and TX interrupts were enabled for both COM1 & COM2
   during initialization. 
2. ISR is modified to determine which interrupt occured. 
3. Output buffer (queue) is created to hold the upcoming characters.
   If buffer is full, then the current process is put to sleep.
4. uartstart() funciton created to start outputting characters from  
   the queue if the uart transmitter is not busy. The processes put 
   to sleep when the buffer was full are awaken afer transimission completes.

### Fully Interrupt-Driven UART Driver Tests:
Adding the ```DEBUG=-DDEBUG``` command line argument when compiling
will enable the debug comments to be printed to the console.

Debug comments:
The COM port number, the character to be printed, and the output buffer
length are displayed when entering uartputc(). 
```sleeping``` is printed before a process is put to sleep.
```waking up``` is printed before a process is awaken.
```Transmitting``` is printed when the uart interrupt is transimitting.

 - ```./usertests``` 
	 - [x] passed

***
## Kernel Threads

A function called kfork() is created. The function creates
a thread that only runs in the kernel space. 

**Changes made:**

The kfork() function allocates a process table entry and 
a kernel page table & stack. Similar to allocproc() in proc.c.
The allocated process is made to be a child of the init process.
Finally, the context of the created process is initialized so that
it's execution starts by calling the passed in function and its termination
calls a modified exit function.

### Kernel Threads Tests:

A kernel thread was created that sleeps on
the OS's ticks. A message is displayed to the console everytime
100 ticks have occured (every 10ms). 

 - ```./usertests```
	 - [x] passed
***
