#define ICI_CORE
#include "float.h"
#include "primes.h"
#include <assert.h>

/*
 * Return a float object corresponding to the given value v. Note that
 * floats are intrinsically atomic. The returned object will have had its
 * reference count inceremented.
 */
float_t *
new_float(v)
double  v;
{
    register float_t    *f;
    static float_t      proto = {OBJ(TC_FLOAT)};

    proto.f_value = v;
    if ((f = floatof(atom_probe(objof(&proto)))) != NULL)
    {
        incref(f);
        return f;
    }
    if ((f = ici_talloc(float_t)) == NULL)
        return NULL;
    *f = proto;
    rego(f);
    objof(f)->o_leafz = sizeof(float_t);
    return floatof(atom(objof(f), 1));
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_float(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(float_t);
}

/*
 * Returns 0 if these objects are eq, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_float(object_t *o1, object_t *o2)
{
    return floatof(o1)->f_value != floatof(o2)->f_value;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_float(object_t *o)
{
    ici_tfree(o, float_t);
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_float(object_t *o)
{
    unsigned long       h;
    int                 i;

    h = FLOAT_PRIME;
    /*
     * We assume that the compiler will decide this constant expression
     * at compile time and not actually make a run-time decision about
     * which bit of code to run.
     */
    if (sizeof floatof(o)->f_value == 2 * sizeof(unsigned long))
    {
        unsigned long   *p;

        p = (unsigned long *)&floatof(o)->f_value;
        h += p[0] + p[1] * 31;
    }
    else
    {
        unsigned char   *p;

        p = (unsigned char *)&floatof(o)->f_value;
        i = sizeof(floatof(o)->f_value);
        while (--i >= 0)
            h = *p++ + h * 31;
    }
    return h;
}

type_t  float_type =
{
    mark_float,
    free_float,
    hash_float,
    cmp_float,
    copy_simple,
    assign_simple,
    fetch_simple,
    "float"
};
