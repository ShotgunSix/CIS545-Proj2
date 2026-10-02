/*
 * NAME, etc.
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
// function to ensure smooth queuing
void enqueue(struct waiter **head, struct waiter **tail, struct waiter *w){
		
	w->next = NULL;			//makes sure theres no chain attached to the end of the new link
	if (*head == NULL){		// check to see if queue is empty
		*head = w;			// set new linked head to w
		*tail = w;			// set new linked tail to w
	} else {				// if queue is already populated
		(*tail)->next = w;	// point from last link tail to new link
		*tail = w;			// set new link as the tail in the queue
	}
}

struct waiter* dequeue(struct waiter **head, struct waiter **tail){
		
	struct waiter *w;

	if (*head == NULL){		// if queue is empty, back out immediately
		return NULL;	
	} 

	w = *head; 			// save front of line waiter to return value later on
	*head = w->next;	// move the head forward one spot to be next in line

	if (*head == NULL){		// second conditional to make sure tail value is correct if queue is empty
		*tail = NULL;  		// if head is NULL, tail must also be set to NULL
	}
	return w;
}

static void wake_next(sthread_rwlock_t *rwlock)
{
        struct waiter *w;
        struct waiter *r;

        w = dequeue(&rwlock->writer_head, &rwlock->writer_tail);

        if (w != NULL) {
                rwlock->writer_active = 1;
                sthread_wake(w->thread);
        } else {
                r = dequeue(&rwlock->reader_head, &rwlock->reader_tail);
                while (r != NULL) {
                        rwlock->num_reader += 1;
                        sthread_wake(r->thread);
                        r = dequeue(&rwlock->reader_head, &rwlock->reader_tail);
                }
        }
}
/*
 * rwlock routines
 */

int sthread_rwlock_init(sthread_rwlock_t *rwlock)
{
	// initialize fields to a default value
	rwlock->guard = 0;
    rwlock->num_reader = 0;
	rwlock->writer_active = 0;
	rwlock->writer_head = NULL;
	rwlock->writer_tail = NULL;
	rwlock->reader_head = NULL;
	rwlock->reader_tail = NULL;
	
	/* FILL ME IN! */
        return 0;
}

// assumed lock is done being used when called
int sthread_rwlock_destroy(sthread_rwlock_t *rwlock)
{		
		rwlock->guard = 0;
		// reset reader count back to zero
		rwlock->num_reader = 0;
		// reset writer count back to zero
		rwlock->writer_active = 0;
	
		/* FILL ME IN! */
        return 0;
}

int sthread_read_lock(sthread_rwlock_t *rwlock)
{
		struct waiter self;
        self.thread = sthread_self();

        while (test_and_set_bit(&rwlock->guard))
                sched_yield();

        if ( rwlock->writer_active == 0 && rwlock->writer_head == NULL ) {
                rwlock->num_reader += 1;
                clear_bit(&rwlock->guard);
                return 0;
        } else {
                enqueue(&rwlock->reader_head, &rwlock->reader_tail, &self);
                clear_bit(&rwlock->guard);
                sthread_suspend();
        }

        return 0;
}

int sthread_read_try_lock(sthread_rwlock_t *rwlock)
{
        /* Not required */
        return 0;
}

int sthread_read_unlock(sthread_rwlock_t *rwlock)
{

		while (test_and_set_bit(&rwlock->guard))
                sched_yield();

        rwlock->num_reader -= 1;

        if ( rwlock->num_reader == 0 ) {
                wake_next(rwlock);
        }

        clear_bit(&rwlock->guard);
        return 0;
        
}

int sthread_write_lock(sthread_rwlock_t *rwlock)
{
        /* FILL ME IN! */
        return 0;
}

int sthread_write_try_lock(sthread_rwlock_t *rwlock)
{
        /* Not required */
        return 0;
}

int sthread_write_unlock(sthread_rwlock_t *rwlock)
{
        /* FILL ME IN! */
        return 0;
}
