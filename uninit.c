#define ICI_CORE
#include "buf.h"
#include "exec.h"
#include "wrap.h"

static wrap_t           *wraps;

/*
 * Register the function func to be called at ICI interpreter shutdown
 * (i.e. ici_uninit() call).
 *
 * The caller must supply a wrap_t struct, which is usually statically
 * allocated. This structure will be linked onto an internal list and
 * be unavailable till after ici_uninit() is called.
 */
void
ici_atexit(void (*func)(void), wrap_t *w)
{
    w->w_next = wraps;
    w->w_func = func;
    wraps = w;
}

/*
 * Shut down the interpreter and clean up any allocations.
 *
 * This function is the reverse of ici_init().
 */
void
ici_uninit(void)
{
    int                 i;
    exec_t              *x;
    extern int          ici_n_allocs;

    /*
     * This catches the case where ici_uninit() is called without ici_init
     * ever being called.
     */
    assert(ici_zero != NULL);

    /*
     * Clean up anything registered by modules that are only optionally
     * compiled in.
     */
    while (wraps != NULL)
    {
        (*wraps->w_func)();
        wraps = wraps->w_next;
    }

    /* Clean up ICI variables used by various bits of ICI. */
    for (i = 0; i < nels(ici_small_ints); ++i)
    {
        ici_decref(ici_small_ints[i]);
        ici_small_ints[i] = NULL;
    }

    /* Call uninitialisation functions for compulsory bits of ICI. */
    uninit_compile();
    uninit_cfunc();

    /* Ensure that the stacks don't stop anything being collected. */
    for (x = ici_execs; x != NULL; x = x->x_next)
        ici_decref(x);
    ici_decref(&ici_os);
    ici_decref(&ici_xs);
    ici_decref(&ici_vs);
    ici_vs.a_top = ici_vs.a_base;
    ici_os.a_top = ici_os.a_base;
    ici_xs.a_top = ici_xs.a_base;

    /* The first call moves them to the fast free list and the second actually
     * cleans up and free()s their memory. */
    ici_reclaim();
    ici_reclaim();

    /* Free the general purpose buffer. */
    ici_nfree(ici_buf, ici_bufz + 1);
    ici_buf = NULL;
    ici_bufz = 0;

    /* Destroy the now empty tables. */
    ici_nfree(atoms, atomsz * sizeof(object_t *));
    atoms = NULL;
    ici_nfree(objs, (objs_limit - objs) * sizeof(object_t *));
    objs = NULL;

    ici_drop_all_small_allocations();
    /*fprintf(stderr, "ici_mem = %ld, n = %d\n", ici_mem, ici_n_allocs);*/
}
