/*
 * Counting semapores implemented with pthreads condition variables.
 */

#include <semaphore.h>

#ifdef ICI_USE_POSIX_THREADS

int
sem_init(sem_t *sem, int pshared, unsigned int count)
{
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t condvar_attr;

    (void)pshared;

    if (pthread_mutexattr_init(&mutex_attr) == -1)
    {
	ici_get_last_errno("mutexattr_init", NULL);
	return -1;
    }
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&sem->sem_mutex, &mutex_attr) == -1)
    {
	ici_get_last_errno("mutex_init", NULL);
	pthread_mutexattr_destroy(&mutex_attr);
	return -1;
    }
    pthread_mutexattr_destroy(&mutex_attr);
    pthread_condattr_init(&condvar_attr);
    if (pthread_cond_init(&sem->sem_condvar, &condvar_attr) == -1)
    {
	ici_get_last_errno("cond_init", NULL);
	pthread_condattr_destroy(&condvar_attr);
	pthread_mutex_destroy(&sem->sem_mutex);
	return -1;
    }
    pthread_condattr_destroy(&condvar_attr);
    sem->sem_count = count;
    sem->sem_nwaiters = 0;
    return 0;
}

int
sem_destroy(sem_t *sem)
{
    pthread_cond_destroy(&sem->sem_condvar);
    pthread_mutex_destroy(&sem->sem_mutex);
    return 0;
}

static int
lock(sem_t *sem)
{
    if (pthread_mutex_lock(&sem->sem_mutex) == -1)
    {
	ici_get_last_errno("mutex_lock", NULL);
	return -1;
    }
    return 0;
}

static int
unlock(sem_t *sem)
{
    if (pthread_mutex_unlock(&sem->sem_mutex) == -1)
    {
	ici_get_last_errno("mutex_unlock", NULL);
	return -1;
    }
    return 0;
}

static int
unlock_and_fail(sem_t *sem, const char *what)
{
    ici_get_last_errno(what, NULL);
    (void)pthread_mutex_unlock(&sem->sem_mutex);
    return -1;
}

int
sem_wait(sem_t *sem)
{
    if (lock(sem))
	return -1;
    ++sem->sem_nwaiters;
    while (sem->sem_count == 0)
    {
	if (pthread_cond_wait(&sem->sem_condvar, &sem->sem_mutex) == -1)
	    return unlock_and_fail(sem, "cond_wait");
    }
    --sem->sem_nwaiters;
    --sem->sem_count;
    return unlock(sem);
}

int
sem_post(sem_t *sem)
{
    if (lock(sem))
	return -1;
    if (sem->sem_nwaiters > 0)
    {
	if (pthread_cond_signal(&sem->sem_condvar) == -1)
	    return unlock_and_fail(sem, "cond_signal");
    }
    ++sem->sem_count;
    return unlock(sem);
}

#endif /* ICI_USE_POSIX_THREADS */
