#define ICI_CORE
#include "fwd.h"
#include "object.h"
#include "string.h"
#include "handle.h"


/*
 * Format a human readable version of the object in 30 chars or less.
 */
static void
objname_handle(object_t *o, char p[ICI_OBJNAMEZ])
{
    if (handleof(o)->h_name == NULL)
        strcpy(p, "handle");
    else
        objname(p, objof(handleof(o)->h_name));
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_handle(object_t *o)
{
    unsigned long       mem;

    o->o_flags |= O_MARK;
    mem = sizeof(ici_handle_t);
    if (objwsupof(o)->o_super != NULL)
        mem += mark(objwsupof(o)->o_super);
    if (handleof(o)->h_name != NULL)
        mem += mark(handleof(o)->h_name);
    return mem;
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_handle(object_t *o1, object_t *o2)
{
    return handleof(o1)->h_ptr != handleof(o2)->h_ptr
        || handleof(o1)->h_name != handleof(o2)->h_name
        || handleof(o1)->o_head.o_super != handleof(o2)->o_head.o_super;
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_handle(object_t *o)
{
    return ICI_PTR_HASH_BITS(handleof(o)->h_ptr)
        ^ (ICI_PTR_HASH_BITS(handleof(o)->h_name) >> 1)
        ^ (ICI_PTR_HASH_BITS(handleof(o)->o_head.o_super) >> 2);
}

/*
 * Return a handle object corresponding to the given address addr and
 * with the given super (which may be NULL). Note that handles are
 * intrinsically atomic. The returned object will have had its
 * reference count inceremented.
 */
ici_handle_t *
ici_new_handle(void *ptr, string_t *name, objwsup_t *super)
{
    ici_handle_t         *h;
    static ici_handle_t  proto = {{OBJ(TC_HANDLE)}};

    proto.h_ptr = ptr;
    proto.h_name = name;
    proto.o_head.o_super = super;
    if ((h = handleof(atom_probe(objof(&proto)))) != NULL)
    {
        incref(h);
        return h;
    }
    if ((h = ici_talloc(ici_handle_t)) == NULL)
        return NULL;
    *h = proto;
    rego(h);
    if (super == NULL && name == NULL)
        objof(h)->o_leafz = sizeof(ici_handle_t);
    return handleof(atom(objof(h), 1));
}

/*
 * Return the object at key k of the obejct o, or NULL on error.
 * See the comment on t_fetch in object.h.
 */
object_t *
fetch_handle(object_t *o, object_t *k)
{
    if (handleof(o)->o_head.o_super == NULL)
        return fetch_simple(o, k);
    return fetch(handleof(o)->o_head.o_super, k);
}

/*
 * Do a fetch where we are the super of some other object that is
 * trying to satisfy a fetch. Don't regard the item k as being present
 * unless it really is. Return -1 on error, 0 if it was not found,
 * and 1 if was found. If found, the value is stored in *v.
 *
 * If not NULL, b is a struct that was the base element of this
 * assignment. This is used to mantain the lookup lookaside mechanism.
 */
static int
fetch_super_handle(object_t *o, object_t *k, object_t **v, struct_t *b)
{
    if (handleof(o)->o_head.o_super == NULL)
        return 0;
    return fetch_super(handleof(o)->o_head.o_super, k, v, b);
}

/*
 * Assign to key k of the object o the value v. Return 1 on error, else 0.
 * See the comment on t_assign() in object.h.
 */
static int
assign_handle(object_t *o, object_t *k, object_t *v)
{
    if (handleof(o)->o_head.o_super == NULL)
        return assign_simple(o, k, v);
    return assign(handleof(o)->o_head.o_super, k, v);
}

/*
 * Do an assignment where we are the super of some other object that
 * is trying to satisfy an assign. Don't regard the item k as being
 * present unless it really is. Return -1 on error, 0 if not found
 * and 1 if the assignment was completed.
 *
 * If 0 is returned, nothing has been modified during the
 * operation of this function.
 *
 * If not NULL, b is a struct that was the base element of this
 * assignment. This is used to mantain the lookup lookaside mechanism.
 */
static int
assign_super_handle(object_t *o, object_t *k, object_t *v, struct_t *b)
{
    if (handleof(o)->o_head.o_super == NULL)
        return 0;
    return assign_super(handleof(o)->o_head.o_super, k, v, b);
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_handle(object_t *o)
{
    ici_tfree(o, ici_handle_t);
}

type_t  ici_handle_type =
{
    mark_handle,
    free_handle,
    hash_handle,
    cmp_handle,
    copy_simple,
    assign_handle,
    fetch_handle,
    "handle",
    objname_handle,
    NULL,
    NULL,
    assign_super_handle,
    fetch_super_handle,
    assign_simple,
    fetch_simple
};
