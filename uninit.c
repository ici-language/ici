#define ICI_CORE
#include "buf.h"
#include "exec.h"
#include "wrap.h"

static ici_wrap_t       *wraps;

/*
 * Register the function 'func' to be called at ICI interpreter shutdown
 * (i.e. 'ici_uninit()' call).
 *
 * The caller must supply a 'ici_wrap_t' struct, which is usually statically
 * allocated. This structure will be linked onto an internal list and
 * be unavailable till after 'ici_uninit()' is called.
 *
 * This --func-- forms part of the --ici-api--.
 */
void
ici_atexit(void (*func)(void), ici_wrap_t *w)
{
    w->w_next = wraps;
    w->w_func = func;
    wraps = w;
}

/*
 * Shut down the interpreter and clean up any allocations.  This function is
 * the reverse of 'ici_init()'.  It's first action is to call any wrap-up
 * functions registered through 'ici_atexit()'.
 *
 * ici_uninit() should be able to be called whether ici_init() fully completed,
 * partially completed, or was not called at all.
 *
 * Calling 'ici_init()' again after calling this hasn't been adequately
 * tested.
 *
 * This routine currently does not handle shutdown of other threads,
 * either gracefully or ungracefully. They are all left blocked on the
 * global ICI mutex without any help of recovery.
 *
 * This --func-- forms part of the --ici-api--.
 */
void
ici_uninit(void)
{
    int                 i;
    ici_exec_t          *x;
    extern ici_str_t    *ici_ver_cache;
    extern ici_regexp_t *ici_smash_default_re;

    //printf("[start ici_uninit]\n");
    
    /*
     * Clean up anything registered by modules that are only optionally
     * compiled in, or loaded modules that register wrap-up functions.
     */
    while (wraps != NULL)
    {
        (*wraps->w_func)();
        wraps = wraps->w_next;
    }

    /*
     * Clean up ICI variables used by various bits of ICI.
     */
    for (i = 0; i < nels(ici_small_ints); ++i)
    {
        if (ici_small_ints[i] != NULL)
            ici_decref(ici_small_ints[i]);
        ici_small_ints[i] = NULL;
    }
    if (ici_ver_cache != NULL)
        ici_decref(ici_ver_cache);
    ici_ver_cache = NULL;
    if (ici_smash_default_re != NULL)
        ici_decref(ici_smash_default_re);
    ici_smash_default_re = NULL;

    /* Call uninitialisation functions for compulsory bits of ICI. */
    uninit_compile();
    uninit_cfunc();

    /*
     * Do a GC to free things that might require reference to the
     * exec state before we discard it.
     */
    ici_reclaim();

    /*
     * Active threads, including the main one, will count reference counts
     * for their exec structs. But finished ones will have none. We don't
     * really care. Just zap them all and let the garbage collector sort
     * them out. This routine doesn't really handle shutdown with outstanding
     * threads running (not that they can actually be running -- we have the
     * mutex).
     */
    for (x = ici_execs; x != NULL; x = x->x_next)
        x->o_head.o_nrefs = 0;

    /*
     * We don't decref the static cached copies of our stacks, because if we
     * did the garbage collector would try to free them (they are static
     * objects, so that would be bad).  However we do empty the stacks.
     */
    ici_vs.a_top = ici_vs.a_base;
    ici_os.a_top = ici_os.a_base;
    ici_xs.a_top = ici_xs.a_base;

    /*
     * OK, so do one final garbage collect to free all this stuff that should
     * now be unreferenced.
     */
    ici_reclaim();
    
    ici_exec = NULL;
    ici_execs = NULL;

    /*
     * Now free the allocated part of our three special static stacks.
     * We should be resilient against multiple calls with no intervening calls to
     * ici_init().  So assume nothing.
     */
    {
        ici_array_t *a[3] = { &ici_vs, &ici_os, &ici_xs }; int i;
        for (i = 0; i < 3; i++)
        {
            if (objof(a[i])->o_nrefs)
            {
                ici_decref(a[i]);
                ici_nfree(a[i]->a_base, (a[i]->a_limit - a[i]->a_base) * sizeof(ici_obj_t *));
            }
        }
#if 1 && !defined(NDEBUG)
        {
            extern void ici_dump_refs(void);

            ici_dump_refs();
        }
#endif
    }

    /*
     * Prevent freeing them again in the event of a failed future ici_init()
     */
    memset(&ici_vs, 0, sizeof(ici_vs));
    memset(&ici_os, 0, sizeof(ici_vs));
    memset(&ici_xs, 0, sizeof(ici_vs));

    ici_uninit_thread();

    /* Free the general purpose buffer. ### Hmm... If we do this we can't
    return ici_error from ici_main.*/
    if (ici_buf != NULL)
    {
        if (ici_error == ici_buf)
        {
            static char ici_static_error[1024];
            strncpy(ici_static_error, ici_error, 1023);
            ici_static_error[1023] = 0;
            ici_error = ici_static_error;
        }
        ici_nfree(ici_buf, ici_bufz + 1);
        ici_buf = NULL;
        ici_bufz = 0;
    }
    /*
     * Destroy the now empty atom pool and list of registered objects.
     */
    ici_uninit_object();
    
    ici_uninit_alloc();
    ici_uninit_sstrings();
    /*fprintf(stderr, "ici_mem = %ld, n = %d\n", ici_mem_used, ici_n_allocs);*/

#if 1 && defined(_WIN32) && !defined(NDEBUG)
    _CrtDumpMemoryLeaks();
#endif
/*    {
        extern long attempt, miss;
        printf("%d/%d %f\n", miss, attempt, (float)miss/attempt);
    }*/
    //printf("[end ici_uninit]\n");
}
