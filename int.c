#define ICI_CORE
#include "int.h"
#include "primes.h"

int_t                       *ici_small_ints[32];

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_int(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(int_t);
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_int(object_t *o1, object_t *o2)
{
    return intof(o1)->i_value != intof(o2)->i_value;
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_int(object_t *o)
{
    /*
     * There are in-line versions of this in object.c and binop.h.
     */
    return (unsigned long)intof(o)->i_value * INT_PRIME;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_int(object_t *o)
{
    ici_tfree(o, int_t);
}


/*
 * Return the int object with the value v. The returned object has had
 * its ref count incremented.
 */
int_t *
ici_int_new(long v)
{
    register int_t      *i;

    /*
     * There is an in-line copy of this near the end of binop.h
     */
    if ((v & ~ICI_SMALL_INT_MASK) == 0 && (i = ici_small_ints[v]) != NULL)
    {
        ici_incref(i);
        return i;
    }
    if ((i = atom_int(v)) != NULL)
    {
        ici_incref(i);
        return i;
    }
    if ((i = ici_talloc(int_t)) == NULL)
        return NULL;
    objof(i)->o_tcode = TC_INT;
    assert(ici_typeof(i) == &int_type);
    objof(i)->o_flags = 0;
    objof(i)->o_nrefs = 1;
    rego(i);
    objof(i)->o_leafz = sizeof(int_t);
    i->i_value = v;
    return intof(ici_atom(objof(i), 1));
}

type_t  int_type =
{
    mark_int,
    free_int,
    hash_int,
    cmp_int,
    copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "int"
};
