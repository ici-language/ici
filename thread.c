#define ICI_CORE
#include "fwd.h"
#include "exec.h"
#include "array.h"
#include "cfunc.h"
#include "op.h"
#include "catch.h"

#ifdef ICI_USE_WIN32_THREADS
HANDLE                  ici_mutex;
#endif
#ifdef ICI_USE_POSIX_THREADS
pthread_mutex_t     ici_mutex;
static pthread_mutex_t  n_active_threads_mutex;
#endif

long                    ici_n_active_threads;

/*
 * Leave code that uses ICI data. ICI data refers to *any* ICI objects
 * or static variables. You would want to call this because you are
 * about to do something that uses a lot of CPU time or blocks for
 * any real time. But you must not even sniff any of ICI's data until
 * after you call ici_enter() again. ici_leave() releases the global
 * ICI mutex that stops ICI threads from simultaneous access to common data.
 * All ICI objects are "common data" because they are shared between
 * threads.
 *
 * Returns the pointer to the ICI execution context of the current
 * thread. This must be preserved (in a local variable on the stack
 * or some other thread safe location) and passed back to the matching
 * call to ici_enter() you will make some time in the future.
 *
 * If the current thread is in an ICI level critical section (e.g.
 * the test or body of a watifor) this will have no effect.
 */
exec_t *
ici_leave(void)
{
    exec_t              *x;

    x = ici_exec;
    if (!x->x_critsect)
    {
        /*
         * Restore the copies of our stack arrays that are cached
         * in static locations back to the execution context.
         */
        decref(&ici_os);
        decref(&ici_xs);
        decref(&ici_vs);
        *x->x_os = ici_os;
        *x->x_xs = ici_xs;
        *x->x_vs = ici_vs;
#ifdef ICI_USE_WIN32_THREADS
        InterlockedDecrement(&ici_n_active_threads);
        ReleaseMutex(ici_mutex);
#else
# ifdef ICI_USE_POSIX_THREADS
        pthread_mutex_lock(&n_active_threads_mutex);
        --ici_n_active_threads;
        pthread_mutex_unlock(&n_active_threads_mutex);

        pthread_mutex_unlock(&ici_mutex);
# else
        /*
         * It is ok to do ici_leave in implementations with
         * no thread support.
         */
# endif
#endif
    }
    return x;
}

/*
 * Enter code that uses ICI data. ICI data referes to *any* ICI objects
 * or static variables. You must do this after having left ICI's mutex
 * domain, by calling ici_leave(), before you again access any ICI data.
 * This call will re-acquire the global ICI mutex that gates access to
 * common ICI data. You must pass in the ICI execution context pointer
 * that you remembered from the previous matching call to ici_leave().
 *
 * If the thread was in an ICI level critical section when the ici_leave()
 * call was made, then this will have no effect (mirroring the no effect
 * that happened when the ici_leave() was done).
 */
void
ici_enter(exec_t *x)
{
    if (!x->x_critsect)
    {
#ifdef ICI_USE_WIN32_THREADS
        InterlockedIncrement(&ici_n_active_threads);
        WaitForSingleObject(ici_mutex, INFINITE);
#else
# ifdef ICI_USE_POSIX_THREADS
        pthread_mutex_lock(&n_active_threads_mutex);
        ++ici_n_active_threads;
        pthread_mutex_unlock(&n_active_threads_mutex);

        if (pthread_mutex_lock(&ici_mutex) == -1)
            perror("ici_mutex");
# else
        /*
         * It is ok to do ici_enter in implementations with
         * no thread support.
         */
# endif
#endif
        if (x != ici_exec)
        {
            /*
             * This really is a change of contexts. Set the global
             * context pointer, and move the stack arrays to the
             * global cached copies (which we do to save a level of
             * indirection on all accesses).
             */
            ici_exec = x;
            ici_os = *x->x_os;
            ici_xs = *x->x_xs;
            ici_vs = *x->x_vs;
        }
        x->x_os->a_base = NULL;
        x->x_xs->a_base = NULL;
        x->x_vs->a_base = NULL;
        incref(&ici_os);
        incref(&ici_xs);
        incref(&ici_vs);
    }
}

/*
 * Allow a switch away from, and back to, this ICI thread, otherwise
 * no effect.
 */
void
ici_yield(void)
{
    exec_t              *x;

    x = ici_exec;
    if (ici_n_active_threads > 1 && x->x_critsect == 0)
    {
        decref(&ici_os);
        decref(&ici_xs);
        decref(&ici_vs);
        *x->x_os = ici_os;
        *x->x_xs = ici_xs;
        *x->x_vs = ici_vs;
#ifdef ICI_USE_WIN32_THREADS
        ReleaseMutex(ici_mutex);
        WaitForSingleObject(ici_mutex, INFINITE);
#else
# ifdef ICI_USE_POSIX_THREADS
        pthread_mutex_unlock(&ici_mutex);
        sched_yield();
        if (pthread_mutex_lock(&ici_mutex) == -1)
            perror("ici_mutex");
# else
        /*
         * It is ok to do ici_yield in implementations with
         * no thread support.
         */
# endif
#endif
        if (x != ici_exec)
        {
            /*
             * This really is a change of contexts. Set the global
             * context pointer, and move the stack arrays to the
             * global cached copies (which we do to save a level of
             * indirection on all accesses.
             */
            ici_exec = x;
            ici_os = *x->x_os;
            ici_xs = *x->x_xs;
            ici_vs = *x->x_vs;
        }
        x->x_os->a_base = NULL;
        x->x_xs->a_base = NULL;
        x->x_vs->a_base = NULL;
        incref(&ici_os);
        incref(&ici_xs);
        incref(&ici_vs);
    }
}

/*
 * Wait for the given object to be signaled. This is the core primitive of
 * the waitfor ICI language construct. It expects to be called within (one
 * level of) critical section, and releases it (by one level only) around
 * the actual wait.
 */
int
ici_waitfor(object_t *o)
{
    exec_t              *x;
    char                *e;

    e = NULL;
    ici_exec->x_waitfor = o;
    --ici_exec->x_critsect;
    x = ici_leave();
#ifdef ICI_USE_WIN32_THREADS
    /*
     * At this point an ici_wakeup() may happen even before we get
     * into the wait. But it's OK because it doesn't matter if the
     * semaphore is incremented before or after we wait, it will
     * still fall out.
     */
    if (WaitForSingleObject(x->x_semaphore, INFINITE) == WAIT_FAILED)
        e = "wait failed";
#else
# ifdef ICI_USE_POSIX_THREADS
    if (sem_wait(&x->x_semaphore) == -1)
        e = "wait failed";
# else
    e = "attempt to wait in waitfor, but no thread support";
# endif
#endif
    ici_enter(x);
    ++ici_exec->x_critsect;
    if (e != NULL)
    {
        ici_error = e;
        return 1;
    }
    return 0;
}

/*
 * Wake up all ICI threads that are waiting for the given object (and
 * thus allow them re-evaluate their wait expression).
 */
int
ici_wakeup(object_t *o)
{
    exec_t              *x;

    for (x = ici_execs; x != NULL; x = x->x_next)
    {
        if (x->x_waitfor == o)
        {
            x->x_waitfor = NULL;
#ifdef ICI_USE_WIN32_THREADS
            ReleaseSemaphore(x->x_semaphore, 1, NULL);
#else
# ifdef ICI_USE_POSIX_THREADS
            sem_post(&x->x_semaphore);
# else
            /*
             * It is ok to do wakeup calls in implementations
             * with no thread support.
             */
# endif
#endif
        }
    }
    return 0;
}

/*
 * Entry point for a new thread. The passed argument is the pointer
 * to the execution context (exec_t *). It has one ref count that is
 * now considered to be owned by this function. The operand stack
 * of the new context has the ICI function to be called configured on
 * it.
 */
static
#ifdef ICI_USE_WIN32_THREADS
long
WINAPI /* Ensure correct Win32 calling convention. */
ici_thread_base(exec_t *x)
{
#endif
#ifdef ICI_USE_POSIX_THREADS
void *
ici_thread_base(void *arg)
{
    exec_t      *x = arg;
#endif
    int                 n_ops;

    ici_enter(x);
    n_ops = ici_os.a_top - ici_os.a_base;
    if ((x->x_result = ici_evaluate(objof(&o_call), n_ops)) == NULL)
    {
        x->x_result = objof(get_cname(ici_error));
        x->x_state = XS_FAILED;
    }
    else
    {
        decref(x->x_result);
        x->x_state = XS_RETURNED;
    }
    ici_wakeup(objof(x));
    decref(x);
    (void)ici_leave();
    return 0;
}

/*
 * From ICI: exec = thread(callable, arg1, arg2, ...)
 */
static int
f_thread()
{
    exec_t              *x;
    int                 i;

    if (NARGS() < 1 || ici_typeof(ARG(0))->t_call == NULL)
        return ici_argerror(0);

    if ((x = ici_new_exec()) == NULL)
        return 1;
    /*
     * Copy all the arguments to the operand stack of the new thread.
     */
    if (ici_stk_push_chk(x->x_os, NARGS() + 80))
        goto fail;
    for (i = 1; i < NARGS(); ++i)
        x->x_os->a_top[NARGS() - i - 1] = ARG(i);
    x->x_os->a_top += NARGS() - 1;
    /*
     * Now push the number of actuals and the object to call on the
     * new operand stack.
     */
    if ((*x->x_os->a_top = objof(new_int(NARGS() - 1))) == NULL)
        goto fail;
    decref(*x->x_os->a_top);
    ++x->x_os->a_top;
    *x->x_os->a_top++ = ARG(0);
    /*
     * Create the native machine thread. We incref x to give the new thread
     * it's own reference.
     */
    incref(x);
#ifdef ICI_USE_WIN32_THREADS
    {
        HANDLE          thread_h;
        unsigned long   thread_id;

        thread_h = CreateThread(NULL, 1024, ici_thread_base, x, 0, &thread_id);
        if (thread_h == NULL)
        {
            ici_get_last_win32_error();
            decref(x); /* The ref the thread was going to own. */
            goto fail;
        }
        x->x_thread_handle = thread_h;
    }
#else
# ifdef ICI_USE_POSIX_THREADS
    {
        pthread_attr_t  thread_attr;

        pthread_attr_init(&thread_attr);
        pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
        if (pthread_create(&x->x_thread_handle, NULL, ici_thread_base, x) == -1)
        {
            syserr();
            decref(x);
            goto fail;
        }
        pthread_detach(x->x_thread_handle);
        pthread_attr_destroy(&thread_attr);
    }
# else
    ici_error = "this implementation does not support thread creation";
    goto fail;
# endif
#endif
    return ici_ret_with_decref(objof(x));

fail:
    decref(x);
    return 1;
}

static int
f_wakeup()
{
    if (NARGS() != 1)
        return ici_argcount(1);
    if (ici_wakeup(ARG(0)))
        return 1;
    return null_ret();
}

/*
 * Perform any OS specific initialisations concerning thread support.
 * Called once from ici_init() before the first execution context is
 * made.
 */
int
ici_init_thread_stuff(void)
{
#ifdef ICI_USE_WIN32_THREADS
    if ((ici_mutex = CreateMutex(NULL, 0, NULL)) == NULL)
        return ici_get_last_win32_error();
#endif
#ifdef ICI_USE_POSIX_THREADS
    pthread_mutexattr_t mutex_attr;

    if (pthread_mutexattr_init(&mutex_attr) == -1)
    {
        perror("pthread_mutexattr_init");
        syserr();
        return 1;
    }
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&ici_mutex, &mutex_attr) == -1)
    {
        syserr();
        pthread_mutexattr_destroy(&mutex_attr);
        return 1;
    }
    if (pthread_mutex_init(&n_active_threads_mutex, &mutex_attr) == -1)
    {
        syserr();
        pthread_mutexattr_destroy(&mutex_attr);
        (void)pthread_mutex_destroy(&ici_mutex);
        return 1;
    }
    pthread_mutexattr_destroy(&mutex_attr);
#endif
    return 0;
}

cfunc_t ici_thread_cfuncs[] =
{
    {CF_OBJ,    "thread",        f_thread},
    {CF_OBJ,    "wakeup",        f_wakeup},
    {CF_OBJ}
};
