#ifndef ICI_SEMAPHORE_H
#define ICI_SEMAPHORE_H

#include <fwd.h>

#ifdef ICI_USE_POSIX_THREADS

#include <pthread.h>

typedef struct
{
    unsigned int    sem_count;
    unsigned long   sem_nwaiters;
    pthread_mutex_t sem_mutex;
    pthread_cond_t  sem_condvar;
}
sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int count);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);

#endif

#endif /* #ifndef ICI_SEMAPHORE_H */
