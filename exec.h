#ifndef ICI_EXEC_H
#define ICI_EXEC_H

#ifndef ICI_INT_H
#include "array.h"
#endif

#ifndef ICI_INT_H
#include "int.h"
#endif

#ifndef ICI_FLOAT_H
#include "float.h"
#endif

#ifdef ICI_USE_WIN32_THREADS
#include <windows.h>
#endif

#ifdef ICI_USE_POSIX_THREADS
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#endif /* ICI_USE_POSIX_THREADS */

union ostemp
{
    int_t       i;
    float_t     f;
};

struct exec
{
    object_t    o_head;
    array_t     *x_xs;
    array_t     *x_os;
    array_t     *x_vs;
    int         x_count;
    int         x_yield_count;
    array_t     *x_pc_closet;           /* See below. */
    array_t     *x_os_temp_cache;       /* See below. */
    exec_t      *x_next;
    int         x_critsect;
    object_t    *x_waitfor;
    int         x_state;
    object_t    *x_result;
#ifdef ICI_USE_WIN32_THREADS
    HANDLE      x_semaphore;
    HANDLE      x_thread_handle;
#endif
#ifdef ICI_USE_POSIX_THREADS
    sem_t   x_semaphore;
    pthread_t   x_thread_handle;
#endif
};
#define execof(o)        ((exec_t *)(o))
#define isexec(o)        (objof(o)->o_tcode == TC_EXEC)
/*
 * x_xs                 The ICI interpreter execution stack. This contains
 *                      objects being executed, which includes 'pc' objects
 *                      (never seen at the language level) that are special
 *                      pointers into code arrays. Entering blocks and
 *                      functions calls cause this stack to grow deeper.
 *
 * x_os                 The ICI interpreter operand stack. This is where
 *                      operands in expressions are stacked during expression
 *                      evaluation (which includes function call argument
 *                      preparation).
 *
 * x_vs                 The ICI interpreter 'scope' or 'variable' stack. The
 *                      top element of this stack is always a struct that
 *                      defines the current context for the lookup of variable
 *                      names. Function calls cause this to grow deeper as
 *                      the new scope of the function being entered is pushed
 *                      on the stack.
 *
 * x_pc_closet          An array that shadows the execution stack. pc objects
 *                      exist only in a one-to-one relationship with a fixed
 *                      (for their life) position on the execution stack. This
 *                      cache holds pc objects that are used whenever we need
 *                      a pc at that slot in the execution stack.
 *
 * x_os_temp_cache      An array of pseudo int/float objects that shadows the
 *                      operand stack. The objects in this array (apart
 *                      from the NULLs) are unions of int and float objects
 *                      that can be used as intermediate results in specific
 *                      circumstances as flaged by the compiler. Specifically,
 *                      they are known to be immediately consumed by some
 *                      operator that is only sensitive to the value, not the
 *                      address, of the object. See binop.h.
 *
 * x_next               Link to the next execution context on the list of
 *                      all existing execution contexts.
 *
 * x_waitfor            If this thread is sleeping, an aggragate object that it
 *                      is waiting to be signaled. NULL if it is not sleeping.
 */

/*
 * Possible values of for x_state.
 */
enum
{
    XS_ACTIVE,          /* Thread has not exited yet. */
    XS_RETURNED,        /* Function returned and thread exited normally. */
    XS_FAILED,          /* Function failed and thread exitied. */
};


/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
#ifndef NODEBUGGING

/*
 * ICI debug interface.  exec() calls the functions in this interface
 * if debugging is enabled.  A default interface, with stubbed
 * implementations of these functions, is provided in the ICI core
 * (in idb2.c).  Debuggers will normally override the default with
 * a more useful set of functions.
 */
struct debug
{
    void    (*idbg_error)(char *, src_t *);
    void    (*idbg_fncall)(object_t *, object_t **, int);
    void    (*idbg_fnresult)(object_t *);
    void    (*idbg_src)(src_t *);
    void    (*idbg_watch)(object_t *, object_t *, object_t *);
};

#endif
/*
 * End of ici.h export. --ici.h-end--
 */

#ifdef  NOTDEF
#define get_pc(code, xs) \
    (*(xs) = ici_exec->x_pc_closet->a_base[(xs) - ici_xs.a_base], \
    pcof(*(xs))->pc_code = code, \
    pcof(*(xs))->pc_next = pcof(*(xs))->pc_code->a_base)
#endif

#endif /* ICI_EXEC_H */
