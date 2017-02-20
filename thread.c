#define ICI_CORE
#include "fwd.h"
#include "exec.h"
#include "array.h"
#include "cfunc.h"
#include "op.h"
#include "catch.h"

#ifdef _WIN32
HANDLE                  ici_mutex;
#endif

long                    ici_n_active_threads;

/*
 * Leave code that uses ICI data. ICI data referrs to *any* ICI objects
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
#ifdef _WIN32
        InterlockedDecrement(&ici_n_active_threads);
        ReleaseMutex(ici_mutex);
#else
        /*
         * It is ok to do ici_leave in implementations with
         * no thread support.
         */
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
#ifdef _WIN32
        InterlockedIncrement(&ici_n_active_threads);
        WaitForSingleObject(ici_mutex, INFINITE);
#else
        /*
         * It is ok to do ici_enter in implementations with
         * no thread support.
         */
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
#ifdef _WIN32
        ReleaseMutex(ici_mutex);
        WaitForSingleObject(ici_mutex, INFINITE);
#else
        /*
         * It is ok to do ici_yield in implementations with
         * no thread support.
         */
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
#ifdef _WIN32
    /*
     * At this point an ici_wakeup() may happen even before we get
     * into the wait. But it's OK because it doesn't matter if the
     * semaphore is incremented before or after we wait, it will
     * still fall out.
     */
    if (WaitForSingleObject(x->x_semaphore, INFINITE) == WAIT_FAILED)
        e = "wait failed";
#else
    e = "attempt to wait in waitfor, but no thread support";
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
#ifdef _WIN32
            ReleaseSemaphore(x->x_semaphore, 1, NULL);
#else
            /*
             * It is ok to do wakeup calls in implementations
             * with no thread support.
             */
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
static long
#ifdef _WIN32
WINAPI /* Ensure correct Win32 calling convention. */
#endif
ici_thread_base(exec_t *x)
{
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
        x->x_os->a_top[NARGS() - i] = ARG(i);
    x->x_os->a_top += NARGS() - 1;
    /*
     * Now push the number of actuals and the object to call on the
     * new operand stack.
     */
    if ((x->x_os->a_top[-1] = objof(new_int(NARGS() - 1))) == NULL)
        goto fail;
    decref(x->x_os->a_top[-1]);
    ++x->x_os->a_top;
    *x->x_os->a_top = ARG(0);
    /*
     * Create the native machine thread. We incref x to give the new thread
     * it's own reference.
     */
    incref(x);
#ifdef _WIN32
    {
        HANDLE          thread_h;
        unsigned long   thread_id;

        thread_h = CreateThread(NULL, 1024, ici_thread_base, x, 0, &thread_id);
        if (thread_h == NULL)
        {
            ici_get_last_win32_error();
            decref(x); /* The ref the thread was goint to own. */
            goto fail;
        }
        x->x_thread_handle = thread_h;
    }
#else
    ici_error = "this implementation does not support thread creation";
    goto fail;
#endif
    return ici_ret_with_decref(objof(x));

fail:
    decref(x);
    return 1;
}

static int
f_wakeup()
{
    object_t            *o;

    if (ici_typecheck("o", &o))
        return 1;
    if (ici_wakeup(o))
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
#ifdef _WIN32
    if ((ici_mutex = CreateMutex(NULL, 0, NULL)) == NULL)
        return ici_get_last_win32_error();
#endif
    return 0;
}

cfunc_t ici_thread_cfuncs[] =
{
    {CF_OBJ,    "thread",        f_thread},
    {CF_OBJ,    "wakeup",        f_wakeup},
    {CF_OBJ}
};