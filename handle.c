#define ICI_CORE
#include "fwd.h"
#include "object.h"
#include "string.h"
#include "handle.h"
#include "buf.h"
#include "array.h"
#include "str.h"

/*
 * Format a human readable version of the object in less than 30 chars.
 */
static void
objname_handle(object_t *o, char p[ICI_OBJNAMEZ])
{
    if (handleof(o)->h_name == NULL)
        strcpy(p, "handle");
    else
    {
        if (stringof(o)->s_nchars > 26)
            sprintf(p, "%.26s...", stringof(o)->s_chars);
        else
            sprintf(p, "%s", stringof(o)->s_chars);
    }
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
        mem += ici_mark(objwsupof(o)->o_super);
    if (handleof(o)->h_name != NULL)
        mem += ici_mark(handleof(o)->h_name);
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
        || handleof(o1)->h_name != handleof(o2)->h_name;
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_handle(object_t *o)
{
    return ICI_PTR_HASH(handleof(o)->h_ptr)
         ^ ICI_PTR_HASH(handleof(o)->h_name);
}

/*
 * Return a handle object corresponding to the given address addr and
 * with the given super (which may be NULL). Note that handles are
 * intrinsically atomic. The returned object will have had its
 * reference count inceremented.
 */
ici_handle_t *
ici_handle_new(void *ptr, string_t *name, objwsup_t *super)
{
    ici_handle_t        *h;
    object_t            **po;
    static ici_handle_t proto = {{{TC_HANDLE, O_SUPER, 1, 0}, NULL}};

    proto.h_ptr = ptr;
    proto.h_name = name;
    proto.o_head.o_super = super;
    if ((h = handleof(atom_probe(objof(&proto), &po))) != NULL)
    {
        ici_incref(h);
        return h;
    }
    ++ici_supress_collect;
    if ((h = ici_talloc(ici_handle_t)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(h, TC_HANDLE, O_SUPER | O_ATOM, 1, 0);
    h->h_ptr = ptr;
    h->h_name = name;
    h->o_head.o_super = super;
    ici_rego(h);
    --ici_supress_collect;
    ICI_STORE_ATOM_AND_COUNT(po, h);
    return h;
}

/*
 * Return the object at key k of the obejct o, or NULL on error.
 * See the comment on t_fetch in object.h.
 */
object_t *
fetch_handle(object_t *o, object_t *k)
{
    if (!hassuper(o))
        return ici_fetch_fail(o, k);
    if (handleof(o)->o_head.o_super == NULL)
        return ici_fetch_fail(o, k);
    return ici_fetch(handleof(o)->o_head.o_super, k);
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
    if (!hassuper(o))
    {
        ici_fetch_fail(o, k);
        return 1;
    }
    if (handleof(o)->o_head.o_super == NULL)
        return 0;
    return fetch_super(handleof(o)->o_head.o_super, k, v, b);
}

static object_t *
fetch_base_handle(object_t *o, object_t *k)
{
    if (!hassuper(o))
        return ici_fetch_fail(o, k);
    if ((o->o_flags & H_HAS_PRIV_STRUCT) == 0)
        return objof(&o_null);
    return fetch_base(handleof(o)->o_head.o_super, k);
}

/*
 * Assign a value into a key of object o, but ignore the super chain.
 * That is, always assign into the lowest level. Usual error coventions.
 */
static int
assign_base_handle(object_t *o, object_t *k, object_t *v)
{
    if (!hassuper(o))
        return ici_assign_fail(o, k, v);
    if ((o->o_flags & H_HAS_PRIV_STRUCT) == 0)
    {
        objwsup_t       *s;

        /*
         * We don't yet have a private struct to hold our values.
         * Give ourselves one.
         *
         * This operation disturbs the struct-lookup lookaside mechanism.
         * We invalidate all existing entries by incrementing ici_vsver.
         */
        if ((s = objwsupof(ici_struct_new())) == NULL)
            return 1;
        s->o_super = objwsupof(o)->o_super;
        objwsupof(o)->o_super = s;
        ++ici_vsver;
        o->o_flags |= H_HAS_PRIV_STRUCT;
    }
    return assign_base(objwsupof(o)->o_super, k, v);
}

/*
 * Assign to key k of the object o the value v. Return 1 on error, else 0.
 * See the comment on t_assign() in object.h.
 */
static int
assign_handle(object_t *o, object_t *k, object_t *v)
{
    if (!hassuper(o))
        return ici_assign_fail(o, k, v);
    if (o->o_flags & H_HAS_PRIV_STRUCT)
        return ici_assign(handleof(o)->o_head.o_super, k, v);
    /*
     * We don't have a base struct of our own yet. Try the super.
     */
    if (handleof(o)->o_head.o_super != NULL)
    {
        switch (assign_super(handleof(o)->o_head.o_super, k, v, NULL))
        {
        case -1: return 1;
        case 1:  return 0;
        }
    }
    /*
     * We failed to assign the value to a super, and we haven't yet got
     * a private struct. Assign it to the base. This will create our
     * private struct.
     */
    return assign_base_handle(o, k, v);
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
    if (!hassuper(o))
        return ici_assign_fail(o, k, v);
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
    if (handleof(o)->h_pre_free != NULL)
        (*handleof(o)->h_pre_free)(handleof(o));
    ici_tfree(o, ici_handle_t);
}

/*
 * Verify that a method on a handle has been invoked correctly. In particular,
 * that inst is not NULL and is a handle with the given name. If OK, if
 * h is non-NULL, the handle is stored through it. If p is non-NULL, the
 * associted pointer (h_ptr) is stored through it. Return 1 in error and
 * sets ici_error, else 0.
 */
int
ici_handle_method_check(object_t *inst, string_t *name, ici_handle_t **h, void **p)
{
    char                n1[30];
    char                n2[30];

    if (ici_method_check(inst, TC_HANDLE))
        return 1;
    if (handleof(inst)->h_name != name)
    {
        sprintf(buf, "attempt to apply method %s to %s",
            ici_objname(n1, ici_os.a_top[-1]),
            ici_objname(n2, inst));
        ici_error = buf;
        return 1;
    }
    if (h != NULL)
        *h = handleof(inst);
    if (p != NULL)
        *p = handleof(inst)->h_ptr;
    return 0;
}


type_t  ici_handle_type =
{
    mark_handle,
    free_handle,
    hash_handle,
    cmp_handle,
    ici_copy_simple,
    assign_handle,
    fetch_handle,
    "handle",
    objname_handle,
    NULL,
    NULL,
    assign_super_handle,
    fetch_super_handle,
    assign_base_handle,
    fetch_base_handle
};
