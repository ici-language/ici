#define ICI_CORE
#include "exec.h"
#include "ptr.h"
#include "struct.h"
#include "int.h"
#include "op.h"
#include "buf.h"
#include "primes.h"
#include "cfunc.h"

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_ptr(ici_obj_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(ici_ptr_t) + ici_mark(ptrof(o)->p_aggr) + ici_mark(ptrof(o)->p_key);
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_ptr(ici_obj_t *o1, ici_obj_t *o2)
{
    return ptrof(o1)->p_aggr != ptrof(o2)->p_aggr
        || ptrof(o1)->p_key != ptrof(o2)->p_key;
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_ptr(ici_obj_t *o)
{
    return (unsigned long)ptrof(o)->p_aggr * PTR_PRIME_0
        + (unsigned long)ptrof(o)->p_key * PTR_PRIME_1;
}

/*
 * Return the object at key k of the obejct o, or NULL on error.
 * See the comment on t_fetch in object.h.
 *
 * Fetching a "sub element" of a pointer is the operation of pointer indexing.
 * It works for any pointer whoes key into its aggregate is an integer, and
 * of course the key must be an integer too.  The final key is the sum of the
 * two keys.  But if the key is zero, just do *ptr.
 *
 * Note that it was previously common for ptrof(o)->p_aggr to itself be a
 * pointer, so fetch_ptr() was recursive.  That should now be rare, since
 * ici_op_mkptr() checks for construction of a pointer with another pointer
 * as p_aggr and bypasses the intermediate pointer if possible.
 */
static ici_obj_t *
fetch_ptr(ici_obj_t *o, ici_obj_t *k)
{
    if (k == objof(ici_zero))
        return ici_fetch(ptrof(o)->p_aggr, ptrof(o)->p_key);
    if (!isint(k) || !isint(ptrof(o)->p_key))
        return ici_fetch_fail(o, k);
    if (ptrof(o)->p_key == objof(ici_zero))
        ici_incref(k);
    else if ((k = objof(ici_int_new(intof(k)->i_value + intof(ptrof(o)->p_key)->i_value)))
            == NULL)
        return NULL;
    o = ici_fetch(ptrof(o)->p_aggr, k);
    ici_decref(k);
    return o;
}

/*
 * Assign to key k of the object o the value v. Return 1 on error, else 0.
 * See the comment on t_assign() in object.h.
 *
 * See above comment.
 */
static int
assign_ptr(ici_obj_t *o, ici_obj_t *k, ici_obj_t *v)
{
    if (k == objof(ici_zero))
        return ici_assign(ptrof(o)->p_aggr, ptrof(o)->p_key, v);
    if (!isint(k) || !isint(ptrof(o)->p_key))
        return ici_assign_fail(o, k, v);
    if (ptrof(o)->p_key == objof(ici_zero))
        ici_incref(k);
    else if ((k = objof(ici_int_new(intof(k)->i_value + intof(ptrof(o)->p_key)->i_value)))
            == NULL)
        return 1;
    if (ici_assign(ptrof(o)->p_aggr, k, v))
    {
        ici_decref(k);
        return 1;
    }
    ici_decref(k);
    return 0;
}

static int
call_ptr(ici_obj_t *o, ici_obj_t *subject)
{
    ici_obj_t   *f;

    if ((f = ici_fetch(ptrof(o)->p_aggr, ptrof(o)->p_key)) == NULL)
        return 1;
    if (ici_typeof(f)->t_call == NULL)
    {
        char    n1[30];

        sprintf(buf, "attempt to call a ptr pointing to %s", ici_objname(n1, o));
        ici_error = buf;
        return 1;
    }
    /*
     * Replace ourselves on the operand stack with 'self' (our aggr) and
     * push on the new object being called.
     */
    if ((ici_os.a_top[-1] = objof(ici_int_new(NARGS() + 1))) == NULL)
        return 1;
    ici_decref(ici_os.a_top[-1]);
    ici_os.a_top[-2] = ptrof(o)->p_aggr;
    if (ici_stk_push_chk(&ici_os, 1))
        return 1;
    *ici_os.a_top++ = f;
    ici_xs.a_top[-1] = objof(&o_call);
    /*
     * Then behave as if the target had been called. Should this do the
     * debug hooks? Assume not for now.
     */
    return (*ici_typeof(f)->t_call)(f, NULL);
}

/*
 * Return a new ICI pointer object. The pointer will point to the element
 * keyed by 'k' in the object 'a'.
 *
 * The returned object has had it' reference count incremented.
 *
 * Returns NULL on error, usual conventions.
 *
 * This --func-- forms part of the --ici-api--.
 */
ici_ptr_t *
ici_ptr_new(ici_obj_t *a, ici_obj_t *k)
{
    register ici_ptr_t  *p;

    if ((p = ici_talloc(ici_ptr_t)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(p, TC_PTR, 0, 1, 0);
    p->p_aggr = a;
    p->p_key = k;
    ici_rego(p);
    return p;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_ptr(ici_obj_t *o)
{
    ici_tfree(o, ici_ptr_t);
}

/*
 * aggr key => ptr
 */
int
ici_op_mkptr()
{
    ici_obj_t   *o;
    ici_obj_t   *a = ici_os.a_top[-2];
    ici_obj_t   *k = ici_os.a_top[-1];

    /*
     * The topmost 2 elements on the stack, 'a' and 'k', are used as the
     * aggregate and key respectively of a new pointer.
     * Suppose we have the ici code:
     *   p = &"ABCDE"[1];
     *   q = &p[2];
     * This sets p = ici_op_mkptr("ABCDE", 1) and q = ici_op_mkptr(p, 2); that
     * is, q is a ptr with another ptr as p_aggr.  *q gives "D" as expected,
     * because fetch_ptr() is recursive and adds up the indices.  However, ptr
     * addition/subtraction/comparison only use the top level indices; thus (q
     * - p) is (q.p_key - p.p_key), i.e. 1, not 2!!  This can really cause
     * chaos in pointer arithmetic, so we don't really want ptrs with another
     * ptr as p_aggr.
     *
     * So, if 'a' is a ptr and 'k' is an int, bypass 'a' by adding the indices
     * and use a->p_aggr as the new aggr.  (We assume that we won't need to go
     * more than one level down, since this same mechanism should have acted
     * on the ptr passed as 'a' as well).  Ideally it would be an error if 'a'
     * is a ptr and 'k' is not an int, but someone may want to create a ptr to
     * one of the special names by which another ptr can be indexed: "key" and
     * "aggr".  So let's allow it for now.
     *
     * Note that pointers to pointers are a totally different situation:
     * that's where p_aggr[p_key] is another ptr.  We're talking about the
     * case where p_aggr itself is a ptr.  Also note that ptrs cannot be
     * changed once created; pointer arithmetic actually creates a new ptr
     * object with the new index.  So "p += 3;" will have no effect on q even
     * if q.p_aggr is p; q still refers to the ptr originally referred to by
     * p.  So there's really no reason to allow ptrs with another ptr as
     * p_aggr.
     */
    if (isint(k) && isptr(a) && isint(ptrof(a)->p_key))
    {
        int     i;
        
        i = intof(k)->i_value + intof(ptrof(a)->p_key)->i_value;
        if ((k = objof(ici_int_new(i))) == NULL)
            return 1;
        a = ptrof(a)->p_aggr;
        o = objof(ici_ptr_new(a, k));
        ici_decref(k);
    }
    else
    {
        o = objof(ici_ptr_new(a, k));
    }
    if (o == NULL)
        return 1;
    ici_os.a_top[-2] = o;
    ici_decref(o);
    --ici_os.a_top;
    --ici_xs.a_top;
    return 0;
}

/*
 * ptr => aggr key
 */
int
ici_op_openptr()
{
    register ici_ptr_t  *p;
    char                n[30];

    if (!isptr(objof(p = ptrof(ici_os.a_top[-1]))))
    {
        sprintf(buf, "pointer required, but %s given", ici_objname(n, ici_os.a_top[-1]));
        ici_error = buf;
        return 1;
    }
    ici_os.a_top[-1] = p->p_aggr;
    *ici_os.a_top++ = p->p_key;
    --ici_xs.a_top;
    return 0;
}

/*
 * ptr => obj
 */
int
ici_op_fetch()
{
    register ici_ptr_t  *p;
    register ici_obj_t  *o;
    char                n[30];

    if (!isptr(objof(p = ptrof(ici_os.a_top[-1]))))
    {
        sprintf(buf, "pointer required, but %s given", ici_objname(n, ici_os.a_top[-1]));
        ici_error = buf;
        return 1;
    }
    if ((o = ici_fetch(p->p_aggr, p->p_key)) == NULL)
        return 1;
    ici_os.a_top[-1] = o;
    --ici_xs.a_top;
    return 0;
}

ici_type_t  ptr_type =
{
    mark_ptr,
    free_ptr,
    hash_ptr,
    cmp_ptr,
    ici_copy_simple,
    assign_ptr,
    fetch_ptr,
    "ptr",
    NULL,
    call_ptr,
};

ici_op_t    o_mkptr         = {OBJ(TC_OP), ici_op_mkptr};
ici_op_t    o_openptr       = {OBJ(TC_OP), ici_op_openptr};
ici_op_t    o_fetch         = {OBJ(TC_OP), ici_op_fetch};
