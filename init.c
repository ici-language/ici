#define ICI_CORE
#include "func.h"
#include "buf.h"
#include "struct.h"
#include "exec.h"
#include "str.h"
#include "pcre/pcre.h"

extern cfunc_t  *funcs[];

/*
 * Perform basic interpreter setup. Return non-zero on failure, usual
 * conventions.
 *
 * After calling this the scope stack has a struct for autos on it, and
 * the super of that is for statics. That struct for statics is where
 * global definitions that are likely to be visible to all code tend
 * to get set. All the intrinsic functions for example. It forms the
 * extern scope of any files parsed at the top level.
 *
 * In systems supporting threads, on exit, the global ICI mutex has been
 * acquired (with ici_enter()).
 */
int
ici_init(void)
{
    cfunc_t             **cfp;
    struct_t            *scope;
    objwsup_t           *externs;
    exec_t              *x;
    int                 i;
    double              pi = 3.1415926535897932;

    /*
     * Just make sure our universal headers are really the size we
     * hope they are. Nothing actually assumes this. But it would
     * represent a significant inefficiency if they were padded.
     */
    assert(sizeof(object_t) == 4);
    assert(offsetof(objwsup_t, o_super) == 4);

    if (ici_chkbuf(120))
        return 1;
    if ((atoms = (object_t **)ici_nalloc(64 * sizeof(object_t *))) == NULL)
        return 1;
    atomsz = 64; /* Must be power of two. */
    memset((char *)atoms, 0, atomsz * sizeof(object_t *));
    if ((objs = (object_t **)ici_nalloc(256 * sizeof(object_t *))) == NULL)
        return 1;
    memset((char *)objs, 0, 256 * sizeof(object_t *));
    objs_limit = objs + 256;
    objs_top = objs;
    for (i = 0; i < nels(ici_small_ints); ++i)
    {
        if ((ici_small_ints[i] = ici_int_new(i)) == NULL)
            return -1;
    }
    ici_zero = ici_small_ints[0];
    ici_one = ici_small_ints[1];
    if (ici_init_sstrings())
        return 1;
    pcre_free = ici_free;
    pcre_malloc = (void *(*)(size_t))ici_alloc;
    if (ici_init_thread_stuff())
        return 1;
    if ((scope = ici_struct_new()) == NULL)
        return 1;
    if ((scope->o_head.o_super = externs = objwsupof(ici_struct_new())) == NULL)
        return 1;
    ici_decref(externs);
    if ((x = ici_new_exec()) == NULL)
        return 1;
    ici_enter(x);
    rego(&ici_os);
    rego(&ici_xs);
    rego(&ici_vs);
    if (ici_engine_stack_check())
        return 1;
    *ici_vs.a_top++ = objof(scope);
    ici_decref(scope);
    for (cfp = funcs; *cfp != NULL; ++cfp)
    {
        if (ici_assign_cfuncs(scope->o_head.o_super, *cfp))
            return 1;
    }

#ifndef NOSIGNALS
    ici_signals_init();
#endif

    if
    (
        ici_set_val(externs, SS(_stdin),  'u', stdin)
        ||
        ici_set_val(externs, SS(_stdout), 'u', stdout)
        ||
        ici_set_val(externs, SS(_stderr), 'u', stderr)
        ||
        ici_set_val(externs, SS(pi), 'f', &pi)
    )
        return 1;

#ifndef NOSTARTUPFILE
    if (ici_call("load", "o", SSO(core)) != NULL)
        return 1;
#endif
    return 0;
}
