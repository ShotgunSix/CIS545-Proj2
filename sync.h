/*
 * NAME, etc.
 *
 * sync.h
 */

#ifndef _STHREAD_SYNC_H_
#define _STHREAD_SYNC_H_

struct waiter {
        sthread_t thread;                 // handle for waiting thread
        struct waiter *next;             // pointer to next link in queue
};

struct sthread_rwlock_struct {   

        volatile unsigned_long guard;
        int num_reader;
        int writer_active;
        struct waiter *writer_head;
        struct waiter *writer_tail;
        struct waiter *reader_head;
        struct waiter *reader_tail;

        /* FILL ME IN! */
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
