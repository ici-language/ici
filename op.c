#define ICI_CORE
#include "op.h"
#include "exec.h"
#include "primes.h"

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_op(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(op_t);
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_op(object_t *o1, object_t *o2)
{
    return opof(o1)->op_func != opof(o2)->op_func
        || opof(o1)->op_code != opof(o2)->op_code
        || opof(o1)->op_ecode != opof(o2)->op_ecode;
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_op(object_t *o)
{
    return OP_PRIME * ((unsigned long)opof(o)->op_func
        + opof(o)->op_code
        + opof(o)->op_ecode);
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_op(object_t *o)
{
    ici_tfree(o, op_t);
}

op_t *
new_op(int (*func)(), int ecode, int code)
{
    op_t                *o;
    object_t            **po;
    static op_t         proto = {OBJ(TC_OP)};

    proto.op_func = func;
    proto.op_code = code;
    proto.op_ecode = ecode;
    if ((o = opof(atom_probe(objof(&proto), &po))) != NULL)
    {
        ici_incref(o);
        return o;
    }
    ++ici_supress_collect;
    if ((o = ici_talloc(op_t)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(o, TC_OP, O_ATOM, 1, sizeof(op_t));
    o->op_code = code;
    o->op_ecode = ecode;
    o->op_func = func;
    ici_rego(o);
    --ici_supress_collect;
    ICI_STORE_ATOM_AND_COUNT(po, o);
    return o;
}

type_t  op_type =
{
    mark_op,
    free_op,
    hash_op,
    cmp_op,
    ici_copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "op"
};
