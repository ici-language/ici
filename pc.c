#define ICI_CORE
#include "exec.h"
#include "pc.h"

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_pc(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(pc_t) + mark(pcof(o)->pc_code);
}

/*
 * Get a new (hopefully recycled) pc and store it at through the given
 * pointer to a slot on the execution stack.
 *
 * pc objects exist only in a one-to-one relationship with a fixed (for their
 * life) position on the execution stack. We take advantage of this by keeping
 * a cache of pc objects shadowing the execution stack. For this reson we
 * require that the caller of this function tell us where on the execution
 * stack (relative to current top of stack) this pc will be placed.
 *
 * NB: pc's come back ready-decref'ed.
 */
int
new_pc(array_t *c, object_t **xs)
{
    pc_t                *pc;
    int                 n;

    assert(c->a_top[-1] == objof(&o_end) || c->a_top[-1] == objof(&o_rewind));
    n = xs - ici_xs.a_base;
    if
    (
        ici_exec->x_xs_pc_cache->a_top - ici_exec->x_xs_pc_cache->a_base < n + 1
        ||
        (pc = pcof(ici_exec->x_xs_pc_cache->a_base[n])) == pcof(&o_null)
    )
    {
        if (ici_stk_probe(ici_exec->x_xs_pc_cache, n))
            return 1;
        if ((pc = pcof(ici_talloc(pc_t))) == NULL)
            return 1;
        objof(pc)->o_tcode = TC_PC;
        assert(ici_typeof(pc) == &pc_type);
        objof(pc)->o_flags = 0;
        objof(pc)->o_nrefs = 0;
        rego(pc);
        ici_exec->x_xs_pc_cache->a_base[n] = objof(pc);
    }
    pc->pc_code = c;
    pc->pc_next = c->a_base;
    pc->pc_limit = c->a_top;
    *xs = objof(pc);
    return 0;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_pc(object_t *o)
{
    ici_tfree(o, pc_t);
}

type_t  pc_type =
{
    mark_pc,
    free_pc,
    hash_unique,
    cmp_unique,
    copy_simple,
    assign_simple,
    fetch_simple,
    "pc"
};
