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
    register op_t       *o;
    static op_t         proto = {OBJ(TC_OP)};

    proto.op_func = func;
    proto.op_code = code;
    proto.op_ecode = ecode;
    if ((o = opof(atom_probe(objof(&proto)))) != NULL)
    {
        ici_incref(o);
        return o;
    }
    if ((o = ici_talloc(op_t)) == NULL)
        return NULL;
    *o = proto;
    assert(ici_typeof(o) == &op_type);
    rego(o);
    objof(o)->o_leafz = sizeof(op_t);
    return opof(ici_atom(objof(o), 1));
}

type_t  op_type =
{
    mark_op,
    free_op,
    hash_op,
    cmp_op,
    copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "op"
};
