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
    if (pcof(o)->pc_code != NULL)
        return sizeof(pc_t) + ici_mark(pcof(o)->pc_code);
    return sizeof(pc_t);
}

/*
 * Return a new pc object.
 *
 * NOTE: pc's come back ready-decref'ed.
 */
pc_t *
new_pc(void)
{
    pc_t                *pc;

    if ((pc = pcof(ici_talloc(pc_t))) == NULL)
        return NULL;
    objof(pc)->o_tcode = TC_PC;
    assert(ici_typeof(pc) == &pc_type);
    objof(pc)->o_flags = 0;
    objof(pc)->o_nrefs = 0;
    pc->pc_code = NULL;
    rego(pc);
    return pc;
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
    ici_assign_fail,
    ici_fetch_fail,
    "pc"
};
