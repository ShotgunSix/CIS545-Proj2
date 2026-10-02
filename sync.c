/*
 * Steven Vetrano
 * CSU ID: 2766483
 *
 * sync.c
 *
 * Synchronization routines for SThread
 */

#define _REENTRANT

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "sthread.h"

/*
 * Atomic operations for x86 architecture.
 */
static inline int test_and_set_bit(volatile unsigned long *addr)
{
	int oldval;
	__asm__ __volatile__("xchgl %0, %1"
			: "=r"(oldval), "+m"(*(addr))	/* output */
			: "0"(1)						/* input */
			: "memory"	/* clobbered: changing contents of memory */
			);
	return oldval;
}
static inline void clear_bit(volatile unsigned long *addr)
{
	unsigned long oldval;
	__asm__ __volatile__("xchgl %0, %1"
			: "=r"(oldval), "+m"(*(addr))	/* output */
			: "0"(0)						/* input */
			: "memory"	/* clobbered: changing contents of memory */
			);
}

// helper function to ensure smooth queuing
void enqueue(struct waiter **head, struct waiter **tail, struct waiter *w){
		
	w->next = NULL;			        //makes sure theres no chain attached to the end of the new link
	if (*head == NULL){		        // check to see if queue is empty
		*head = w;			// set new linked head to w
		*tail = w;			// set new linked tail to w
	} else {				// if queue is already populated
		(*tail)->next = w;	        // point from last link tail to new link
		*tail = w;			// set new link as the tail in the queue
	}
}

// helper function to ensure smooth queuing
struct waiter* dequeue(struct waiter **head, struct waiter **tail){
		
	struct waiter *w;

	if (*head == NULL){		// if queue is empty, back out immediately
		return NULL;	
	} 

	w = *head; 			// save front of line waiter to return value later on
	*head = w->next;	        // move the head forward one spot to be next in line

	if (*head == NULL){		// second conditional to make sure tail value is correct if queue is empty
		*tail = NULL;  		// if head is NULL, tail must also be set to NULL
	}
	return w;
}

// helper function for waking next reader/writer in queue
static void wake_next(sthread_rwlock_t *rwlock)
{
        struct waiter *w;                               // writer struct for dequeuing
        struct waiter *r;                               // reader struct for dequeuing

        w = dequeue(&rwlock->writer_head, &rwlock->writer_tail);                        // establish writer queue variable

        if (w != NULL) {                                                                // if there was another writer in queue, set writer lock and wake next thread
                rwlock->writer_active = 1;                                              // set writer active to to 1 (true)
                sthread_wake(w->thread);                                                // wake writer thread to allow execution
        } else {                                                                         // if writer queue is empty, pull readers out of queue and grant them access
                r = dequeue(&rwlock->reader_head, &rwlock->reader_tail);                 //establish reader queue variable  
                while (r != NULL) {                                                      // loop to pull readers from queue until reader queue is empty
                        rwlock->num_reader += 1;                                         // counter to track number of readers who have access
                        sthread_wake(r->thread);                                         // wake reader thread to allow execution
                        r = dequeue(&rwlock->reader_head, &rwlock->reader_tail);         // pull next reader from queue
                }
        }
}
/*
 * rwlock routines
 */

int sthread_rwlock_init(sthread_rwlock_t *rwlock)
{
        rwlock->guard = 0;                              // initialize default rwlock field values
        rwlock->num_reader = 0;
        rwlock->writer_active = 0;
        rwlock->writer_head = NULL;
        rwlock->writer_tail = NULL;
        rwlock->reader_head = NULL;
        rwlock->reader_tail = NULL;
        
        return 0;
}

int sthread_rwlock_destroy(sthread_rwlock_t *rwlock)
{
        rwlock->guard = 0;                      // set spinlock guard back to 0
        rwlock->num_reader = 0;                 // set number of readers back to zero
        rwlock->writer_active = 0;              // set active writers back to zero
       
        return 0;
}

int sthread_read_lock(sthread_rwlock_t *rwlock)
{       
        struct waiter self;                                             //  build threads own waiter node in case it has to wait
        self.thread = sthread_self();                                   //  record this threads identity so it can be woken latern

        while (test_and_set_bit(&rwlock->guard))                        // aquire spinlock or spin until its free
                sched_yield();                                          // yield cpu between attempts

        if ( rwlock->writer_active == 0 && rwlock->writer_head == NULL ) {        // conditional to verify a writer doesnt currently have access and no other writers in queue
                rwlock->num_reader += 1;                                          // counter to track number of readers with access
                clear_bit(&rwlock->guard);                                        // release spinlock
                return 0;
        } else {
                enqueue(&rwlock->reader_head, &rwlock->reader_tail, &self);        // place reader in reader queue
                clear_bit(&rwlock->guard);                                         // release spinlock before putting thread to sleep
                sthread_suspend();                                                 // a writer currently has access, thread placed in waiting
        }

        return 0;
}

int sthread_read_try_lock(sthread_rwlock_t *rwlock)
{
        /* FILL ME IN! */
        return 0;
}

int sthread_read_unlock(sthread_rwlock_t *rwlock)
{       
        
        while (test_and_set_bit(&rwlock->guard))        // aquire spinlock or spin until its free
                sched_yield();                          // yield cpu between attempts

        rwlock->num_reader -= 1;                        // decrement number of readers who have reader access

        if ( rwlock->num_reader == 0 ) {                // conditional to verify all readers have access removed
                wake_next(rwlock);                      // wake next thread in queue
        }

        clear_bit(&rwlock->guard);                      // release spinlock
        return 0;
        
}

int sthread_write_lock(sthread_rwlock_t *rwlock)
{
        
        struct waiter self;                                     // build this threads waiter node in case it has to wait
        self.thread = sthread_self();                           // record this threads identity so it can be woken later

        while (test_and_set_bit(&rwlock->guard))                // aquire spinlock or spin until its free
                sched_yield();                                  // yield cpu between attempts

        if ( rwlock->num_reader == 0 && rwlock->writer_active == 0 ) {          // conditional to check if there are no active readers and writers
                rwlock->writer_active = 1;                                      // activate writers lock
                clear_bit(&rwlock->guard);                                      // release spinlock
                return 0;
        } else {
                enqueue(&rwlock->writer_head, &rwlock->writer_tail, &self);     // place writer into writer queue
                clear_bit(&rwlock->guard);                                      // release spinlock
                sthread_suspend();                                              // suspend thread to wait for writer access
        }

        return 0;
}

int sthread_write_try_lock(sthread_rwlock_t *rwlock)
{
        /* FILL ME IN! */
        return 0;
}

int sthread_write_unlock(sthread_rwlock_t *rwlock)
{

       while (test_and_set_bit(&rwlock->guard))         // aquire spinlock or spin until its free
                sched_yield();                          // yield cpu between attempts

        rwlock->writer_active = 0;              // unlock writers lock
        wake_next(rwlock);                      // wake next thread in queue

        clear_bit(&rwlock->guard);              // clear spinlock guard
        return 0;

}
