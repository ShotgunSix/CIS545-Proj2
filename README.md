# Assignment-2-CIS545: Multithreading and Read-Write Locks

## Group Info:
* Group Number: 21
* Member 1:
  * Jeremy Sorkin
  * CSU-ID: 2934722
  * Contribution: 50%
* Member 2:
  * Steven Vetrano
  * CSU-ID: 2766483
  * Contribution: 50% 

## `rwlock` data structure:
Here is our implementation of `rwlock`s found in `sync.h`:
```
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
```
`guard` is a spinlock-bit used to synchronize access to `rwlock` and yield the cpu if unavailable.\
`num_reader` counts the number of readers currently in the critical section.\
`writer_active` is a boolean that tracks whether a writer is currently in the critical section.\
`writer_head` and `writer_tail` are pointers to the head and tail of the waiting writer's queue:
```
 writer_head                                     writer_tail
  |                                               |
  v                                               v
+----------+        +----------+                +-------------+
| writer1| o-+----->| writer2| o-+-----> ...    | writer_n| / |
+----------+        +----------+                +-------------+
```
Similarly, `reader_head` and `reader_tail` are pointers to the head and tail of the waiting reader's queue.

## Supported behavior:

### Multiple readers: 
This is implemented in the functions `sthread_read_lock`,`sthread_read_unlock`, and `wake_next`.\
For acquiring the read-lock:
```
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
```
We only check that there is no active writer or waiting writers, allowing multiple readers to simultaneously acquire the read lock.\
For releasing the read-lock:
```
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
```
We wait for all the active readers to exit the critical section before calling `wake_next`: 
```
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
```
Assuming there are no waiting writers, `wake_next` will empty-out the waiting readers queue allowing all of them to simultaneously enter the critical region. 

### Favoring writers over readers:
This is implemented in the functions `sthread_read_lock` and `wake_next`.\
When acquiring the read-lock in `sthread_read_lock`, if there are waiting writers then the reader is forced to wait in the waiting readers queue giving priority to waiting writers over active readers.\
When waking the next waiting thread in `wake_next`, we first check the waiting writers queue before the waiting readers queue giving priority to waiting writers.

### Fairness:
This is implemented by the waiting writers and waiting readers queues. The FIFO nature of the queues ensures that the readers and writers waiting the longest get to acquire their respective locks first. 

## Testing supported behavior:
To make sure our `rwlock` structure correctly implements the above supported behavior, we created `test.c` file to demonstrate it.\
There are 4 different kinds of threads each with access to the shared data `balance`:
- `bcheck`: Acquires the read-lock, reads `balance`, then releases the lock:
```
int bcheck(void *arg) {
  int i;
  int trubal;
  int threadno = (int)arg;
  for (i = 0; i<5; i++) {
    sthread_read_lock(&mylock);
    trubal = balance;
    printf("thread%d ===> Reader thread iteration %d, balance = %d\n", threadno, i, trubal);
    fflush(stdout);
    sthread_read_unlock(&mylock);
    //sleep(1);
  }
  return 0;
}
```
- `slowreader`: Acquires the read-lock, goes to sleep, then wakes up and releases the lock:
```
int slowreader(void *arg) {
  int threadno = (int)arg;
  sthread_read_lock(&mylock);
  printf("thread%d ===> This reader thread is very slow\n", threadno);
  sleep(10);
  printf("thread%d ===> I am finally done! #############\n", threadno);
  sthread_read_unlock(&mylock);
  return 0;
}
```
- `deposit`: Acquires the write-lock, adds 1 to the balance, then releases the lock:
```
int deposit(void *arg) {
  int i, tmp;
  //k = 0;
  int threadno = (int)arg;
  for (i=0; i<NUMLOOP; i++) {
    sthread_write_lock(&mylock);
    tmp = balance;
    tmp = tmp + 1;
    balance = tmp;
    printf("thread%d ===> Deposit thread iteration %d\n", threadno, i);
    fflush(stdout);
    sthread_write_unlock(&mylock);
  }
  return 0;
}
```
- `withdraw`: Acquires the write-lock, subtracts 1 from the balance, then releases the lock:
```
int withdraw(void *arg) {
  int i;
  int threadno = (int)arg;
  for (i=0; i<NUMLOOP; i++) {
    sthread_write_lock(&mylock);
    balance -= 1;
    printf("thread%d ===> Withdrawal thread iteration %d\n", threadno, i);
    fflush(stdout);
    sthread_write_unlock(&mylock);
  }
  return 0;
}
```
We create 40 of these threads, 10 of each kind:
```
  for (i = 0; i<NUMTHR; i++) {
    if (sthread_create(&thr3[i], bcheck, (void *)(NUMTHR+NUMTHR+i)) == -1)
      fprintf(stderr, "%s: sthread_create: %s\n", argv[0], strerror(errno));

    if (sthread_create(&thr1[i], deposit, (void *)(1+i)) == -1)
      fprintf(stderr, "%s: sthread_create: %s\n", argv[0], strerror(errno));

    if (sthread_create(&thr2[i], withdraw, (void *)(NUMTHR+i)) == -1)
      fprintf(stderr, "%s: sthread_create: %s\n", argv[0], strerror(errno));
  
    if (sthread_create(&thr4[i], slowreader, (void *)(NUMTHR+NUMTHR+NUMTHR+i)) == -1)
      fprintf(stderr, "%s: sthread_create: %s\n", argv[0], strerror(errno));    
  }
```
We then run `test.c` and observe the threads' behavior:

### Favoring Writers over Readers:
When the Readers and Writers queues fill-up, the Writers will hog the write lock and starve the Readers till the very end:
```
./test
thread20 ===> Reader thread iteration 0, balance = 10000
thread20 ===> Reader thread iteration 1, balance = 10000
thread1 ===> Deposit thread iteration 0
thread20 ===> Reader thread iteration 2, balance = 10001
...
thread2 ===> Deposit thread iteration 0
thread11 ===> Withdrawal thread iteration 0
thread3 ===> Deposit thread iteration 0
thread12 ===> Withdrawal thread iteration 0
thread4 ===> Deposit thread iteration 0
thread13 ===> Withdrawal thread iteration 0
thread5 ===> Deposit thread iteration 0
thread14 ===> Withdrawal thread iteration 0
thread6 ===> Deposit thread iteration 0
thread15 ===> Withdrawal thread iteration 0
thread7 ===> Deposit thread iteration 0
thread16 ===> Withdrawal thread iteration 0
thread8 ===> Deposit thread iteration 0
thread17 ===> Withdrawal thread iteration 0
thread9 ===> Deposit thread iteration 0
thread18 ===> Withdrawal thread iteration 0
...
```
### Multiple Readers:
When a slow-reader falls asleep holding the read-lock, multiple other readers simultaneously acquire the read-lock and go through their iterations:
```
thread31 ===> This reader thread is very slow
thread32 ===> This reader thread is very slow
thread22 ===> Reader thread iteration 0, balance = 10000
thread23 ===> Reader thread iteration 0, balance = 10000
thread33 ===> This reader thread is very slow
thread25 ===> Reader thread iteration 0, balance = 10000
...
thread22 ===> Reader thread iteration 4, balance = 10000
thread22 ===> Reader thread iteration 4, balance = 10000
thread29 ===> Reader thread iteration 4, balance = 10000
the balnce is 10000
the balnce is 10000
the balnce is 10000
thread31 ===> I am finally done! #############
```

## Compilation and Execution:
To compile the sthread library and test program, simply run `make test`.\
To execute the test program, run `./test`.\
Here is an example run of `test`:
```
./test
thread20 ===> Reader thread iteration 0, balance = 10000
thread20 ===> Reader thread iteration 1, balance = 10000
thread1 ===> Deposit thread iteration 0
thread20 ===> Reader thread iteration 2, balance = 10001
thread20 ===> Reader thread iteration 3, balance = 10001
thread20 ===> Reader thread iteration 4, balance = 10001
thread1 ===> Deposit thread iteration 1
thread1 ===> Deposit thread iteration 2
thread1 ===> Deposit thread iteration 3
...
thread33 ===> I am finally done! #############
thread32 ===> I am finally done! #############
the balnce is 10000
the balnce is 10000
the balnce is 10000
the balnce is 10000
the balnce is 10000
the balnce is 10000
...
```