#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "spinlock.h"

/*
*   Initializes a semaphore.
*   Member val: Semaphore value.
*   Member semlock: Used to ensure ssemaphore functions are atomic.
*/
struct semaphore {
    int val;
    struct spinlock semlock;
};

/*
*   Initialize a semaphore with the passed in value.
*   Paramater sp: Semaphore to be initialized.
*   Paramater val: Initialization value - represents number of resources available.
*/
void sem_init(struct semaphore *sp, int val);

/*
*   Allows process to access shared resources. Puts the process
*   to sleep if not yet allowed. Process awaits until sem_V calls wakeup
*   to try again.
*   Paramater sp: Semaphore to be updated.
*/
void sem_P(struct semaphore *sp);

/*
*   If a process is sleeping on the current semaphore, wake it up.
*   Paramater sp: Semaphore to be updated.
*/
void sem_V(struct semaphore *sp);

#endif
