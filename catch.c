#define ICI_CORE
/*
 * catch.c - implementation of the ICI catch type. This is an internal
 * type used as a marker on the execution stack. These objects should
 * never escape to user visibility. See also catch.h.
 */
#include "exec.h"
#include "pc.h"
#include "catch.h"
#include "op.h"
#include "func.h"

/*
 * Unwind the execution stack until a catcher is found.  Then unwind
 * the scope and operand stacks to the matching depth (but only if it is).
 * Returns the catcher, or NULL if there wasn't one.
 */
catch_t *
ici_unwind()
{
    object_t    **p;
    catch_t     *c;

    for (p = ici_xs.a_top - 1; p >= ici_xs.a_base; --p)
    {
        if (iscatch(*p))
        {
            c = catchof(*p);
            ici_xs.a_top = p;
            ici_os.a_top = &ici_os.a_base[c->c_odepth];
            ici_vs.a_top = &ici_vs.a_base[c->c_vdepth];
            return c;
        }
    }
    assert(0);
    return NULL;
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_catch(object_t *o)
{
    unsigned long       mem;

    o->o_flags |= O_MARK;
    mem = sizeof(catch_t);
    if (catchof(o)->c_catcher != NULL)
        mem += ici_mark(catchof(o)->c_catcher);
    return mem;
}

/*
 * Return a new catch object with the given catcher object and
 * corresponding to the operand and variable stack depths given.
 * The catcher, o, may be NULL.
 *
 * Note: catch's are special. Unlike most types this new_catch function
 * returns its object with a ref count of 0 not 1.
 */
catch_t *
new_catch(object_t *o, int odepth, int vdepth, int flags)
{
    register catch_t    *c;

    if ((c = ici_talloc(catch_t)) == NULL)
        return NULL;
    objof(c)->o_tcode = TC_CATCH;
    assert(ici_typeof(c) == &ici_catch_type);
    objof(c)->o_flags = flags;
    objof(c)->o_nrefs = 0;/* Catch's are pre-decref'ed.  Unlike other types. */
    rego(c);
    c->c_catcher = o;
    c->c_odepth = odepth;
    c->c_vdepth = vdepth;
    return c;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_catch(object_t *o)
{
    assert((o->o_flags & CF_EVAL_BASE) == 0);
    ici_tfree(o, catch_t);
}

/*
 * runner handler       => catcher (os)
 *                      => catcher pc (xs)
 *                      => catcher (vs)
 */
int
ici_op_onerror()
{
    if ((ici_xs.a_top[-1] = objof(new_catch(ici_os.a_top[-1], ici_os.a_top - ici_os.a_base - 2, ici_vs.a_top - ici_vs.a_base, 0))) == NULL)
        return 1;
    get_pc(arrayof(ici_os.a_top[-2]), ici_xs.a_top);
    ++ici_xs.a_top;
    ici_os.a_top -= 2;
    return 0;
}

type_t  ici_catch_type =
{
    mark_catch,
    free_catch,
    hash_unique,
    cmp_unique,
    copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "catch"
};

op_t    o_onerror       = {OBJ(TC_OP), ici_op_onerror};
