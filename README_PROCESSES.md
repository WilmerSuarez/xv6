# XV6 PROCESSES

***

## Kernel Semaphores

Add ability to use semaphores in xv6.

**Changes made:**
Three functions were added
**sem_init(struct semaphore \*sp, int val);**

 - Initializes a semaphore with the passed in value.
 
**sem_P(struct semaphore \*sp);**

 - Allows processes to access shared resources. 
 Puts the process to sleep if not yet allowed. 
 Processes wait until sem_V calls wakeup() to try again.

**sem_V(struct semaphore \*sp);**

 - Increments the Semaphore value and calls wakeup() to wakeup any 
processes that are waiting on the current semaphore.

### Kernel Semaphores Tests:
The semaphore implementation was tested with the IDE Disk Device Driver. 
The driver and its interrupt handler used sleep() and wakeup() to more 
efficiently service disk requests. This was replaced with a semaphore. 
The semaphore is initialized with the value of 0. After the Disk Driver 
appends the new disk access request to the ide-queue, the semaphore sees 
that the current semaphore value is 0 and puts the current process to 
sleep. Once the IDEs ISR is executed, after the request has been serviced, 
the associated process is awoken and continues out of the Disk Driver. 

 - ```./usertests``` 
	 - [x] passed
***
## Extensible User Stacks
Stack is made automatically extensible. The stack is placed right below 
the start of the Kernel address space. Instead of being limited to a 
maximum stack size of 4KB, the stack is allowed the use of up to 4MB. 

**Changes made:**
The lines of code in ```exec.c``` that allocated the one page of the 
stack and the guard page beneath it (above the program text/data) were 
replaced with code that allocated one page below KERNBASE. Now, a page 
fault will occur whenever the stack tries to access data below that. 

In ```trap.c``` a handler for the page fault is created. After verifying 
that the page fault was caused by the stack trying to access memory below 
its current  page, a check for the size of the stack plus a new page to 
be less than 4MB is made. If it is not then the process is killed. If it 
is less than 4MB then a new page is allocated below the stack.

Changes to the ```sysproc.c``` file were made to use the new stack 
location to verify the arguments passed to system calls. 

The *fork* function in ```proc.c``` was updated by making the *copyuvm* 
function accept the number of pages of the stack as a parameter. In 
*copyuvm* a new loop is implemented to copy the upper end of the user 
address space (the stack) since the previous implementation only copied 
the lower end.

Lastly, a change to the *growproc* function in ```proc.c``` now makes a 
check to make sure that any memory being allocated for the heap will not 
collide with the stack.


### Extensible User Stacks Tests:
 - ```./usertests```
	 - [x] passed

#### Stack Overflow Test:
```./stack_overflow```
This test causes a stack overflow to see how much memory it used before 
the process is killed.
**Before Automatically Extensible Stack:**
*Number of bytes:* **3776**  
**After Automatically Extensible Stack:** 
*Number of bytes:* **4193984**
***
## Multiple Shell Sessions
Adding support to XV6 to allow multiple shell sessions to run 
simultaneously. The system Console and serial port devices, 
COM1 and COM2 will each support a shell.

**Changes made:**
The console driver ```console.c``` and the device driver ```uart.c``` 
were '*detached*' (the console and the terminal(*COM1*)). Next, the device driver was given the same functionality of the console by:
 1. Allowing it to wait for users to edit a line before pressing '*ENTER*'.
 2. Setting up its own entry in the Device Switch table for reading from and writing
 to the driver.
 
Next, the ```init.c``` file was modified to create a a process for each each device and start
a shell on that process. It also kept track of the init child processes PIDs to start a new shell
whenever a shell terminates. 
To incorporate both COM1 and COM2, the ```uart.c``` file was changed to support 
multiple devices. 

Lastly, both UART devices (COM1 & COM2) were made remotely accesible over a network through the command *telnet*. 

### Multiple Shell Sessions Tests:
 - ./usertests 
	 - [x] passed
 
***
