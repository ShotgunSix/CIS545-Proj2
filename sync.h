/*
 * Steven Vetrano
 * CSU ID: 2766483
 *
 * sync.h
 */

#ifndef _STHREAD_SYNC_H_
#define _STHREAD_SYNC_H_

// waiter struct for linked list handling
struct waiter {
        sthread_t thread;
        struct waiter *next;
};

struct sthread_rwlock_struct {   
         
        volatile unsigned long guard;                  //spinlock bit
        int num_reader;                                 // counter for number of readers
        int writer_active;                              // boolean int to track if there is an active writer in critical regions
        struct waiter *writer_head;                     // writer linked list queue
        struct waiter *writer_tail;                     
        struct waiter *reader_head;                     // reader linked list queue
        struct waiter *reader_tail;
};

typedef struct sthread_rwlock_struct sthread_rwlock_t;

int sthread_rwlock_init(sthread_rwlock_t *rwlock);
int sthread_rwlock_destroy(sthread_rwlock_t *rwlock);
int sthread_read_lock(sthread_rwlock_t *rwlock);
int sthread_read_try_lock(sthread_rwlock_t *rwlock);
int sthread_read_unlock(sthread_rwlock_t *rwlock);
int sthread_write_lock(sthread_rwlock_t *rwlock);
int sthread_write_try_lock(sthread_rwlock_t *rwlock);
int sthread_write_unlock(sthread_rwlock_t *rwlock);

#endif
