#define ICI_CORE
#include "buf.h"
#include "exec.h"
#include "wrap.h"


extern void     drop_fast_free_lists(void);


/*
 * Shut down the interpreter and clean up any allocations.
 *
 * This function is the reverse of ici_init(), and a superset of wrapup().
 */
void
ici_uninit(void)
{
    /* This catches the case where ici_uninit() is called without ici_init
     * ever being called. */
    if (o_zero == NULL)
        return;

    /* Clean up anything registered by modules that are only optionally
     * compiled in. */
    wrapup();

    /* Clean up ICI variables used by various bits of ICI. */
    decref(o_zero);
    o_zero = NULL;
    decref(o_one);
    o_one = NULL;

    /* Call uninitialisation functions for compulsory bits of ICI. */
    uninit_compile();
    uninit_cfunc();

    /* Ensure that the stacks don't stop anything being collected. */
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
}
