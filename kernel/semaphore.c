#include "semaphore.h"
#include "defs.h"

/*
*   Initialize a semaphore with the passed in value.
*/
void sem_init(struct semaphore *sp, int val) {
    sp->val = val;
}

/*
*   Allows processes to access shared resources. Puts the process
*   to sleep if not yet allowed. Processes wait until sem_V calls wakeup()
*   to try again.
*   Sleep until sp->val becomes positive. 
*   Decrements the Semaphore value and returns.
*   *** Atomic ***
*   Atomicity is accomplished using a spinlock. 
*/
void sem_P(struct semaphore *sp) {
    acquire(&sp->semlock);
    while(sp->val <= 0)
        sleep(sp, &sp->semlock);
    sp->val -= 1;
    release(&sp->semlock);
}

/*
*   Increments the Semaphore value and calls wakeup() to wakeup any
*   processes that are waiting on the current semaphore.
*   *** Atomic ***
*   Atomicity is accomplished using a spinlock. 
*/
void sem_V(struct semaphore *sp) {
    acquire(&sp->semlock);
    sp->val += 1;
    wakeup(sp);`
    release(&sp->semlock);
}
