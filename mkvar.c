#define ICI_CORE
#include "exec.h"
#include "struct.h"
#include "int.h"
#include "float.h"
#include "buf.h"

/*
 * ici_set_val(scope, name, typespec, value)
 *
 * Set the value of the given name in given struct to the given raw C value,
 * which is first converted to an ICI value on the assumption that it is of the
 * type specified by the given typespec. Somewhat unusually, this function
 * always sets the value in the base struct. It avoids writing in super-structs.
 * Note that the C value is generally passed as a pointer to the value.
 *
 * Type specification is similar to other internal functions (ici_func,
 * ici_typecheck etc..) and uses the following key-letters
 *
 * Type Letter  ICI type        C type (of value)
 * ================================================
 *      i       int             long *
 *      f       float           double *
 *      s       string          char *
 *      u       file            FILE *
 *      o       any             object_t *
 *
 * Returns 0 on succcess, else non-zerro, usual conventions.
 */
int
ici_set_val(objwsup_t *s, string_t *name, int type, void *vp)
{
    object_t    *o;
    int         i;

    switch (type)
    {
    case 'i':
        o = objof(new_int(*(long *)vp));
        break;

    case 'f':
        o = objof(new_float(*(double *)vp));
        break;

    case 's':
        o = objof(new_cname((char *)vp));
        break;

    case 'u':
        o = objof(new_file((char *)vp, &stdio_ftype, name));
        break;

    case 'o':
        o = (object_t *)vp;
        incref(o); /* so can decref(o) below */
        break;

    default:
        ici_error = "illegal type key-letter given to ici_set_val";
        return 1;
    }

    if (o == NULL)
        return 1;
    i = assign_base(s, objof(name), o);
    decref(o);
    return i;
}

/*
 * Utility function to form an error message when a fetch which expects to
 * find a particular value or type of value fails to do so. Returns 1 so
 * it can be used directly in typical error returns.
 */
int
ici_fetch_mismatch(object_t *o, object_t *k, object_t *v, char *expected)
{
    char        n1[30];
    char        n2[30];
    char        n3[30];

    if (ici_chkbuf(200))
        return 1;
    sprintf(buf, "read %s from %s keyed by %s, but expected %s",
        objname(n1, v),
        objname(n2, o),
        objname(n3, k),
        expected);
    ici_error = buf;
    return 1;
}

int
ici_assign_float(object_t *o, object_t *k, double v)
{
    float_t     *f;

    if ((f = new_float(v)) == NULL)
        return 1;
    if (assign(o, k, objof(f)))
        return 1;
    return 0;
}

/*
 * Fetch a float or int from the given object keyed by the given key.
 * The result is stored as a double through the given pointer. Returns
 * non-zero on error, usual conventions.
 */
int
ici_fetch_num(object_t *o, object_t *k, double *vp)
{
    object_t    *v;

    if ((v = fetch(o, k)) == NULL)
        return 1;
    if (isint(v))
        *vp = intof(v)->i_value;
    else if (isfloat(v))
        *vp = floatof(v)->f_value;
    else
        return ici_fetch_mismatch(o, k, v, "a number");
    return 0;
}

/*
 * Fetch an int (a C long) from the given object keyed by the given key.
 * The result is stored as a long through the given pointer. Returns
 * non-zero on error, usual conventions.
 */
int
ici_fetch_int(object_t *o, object_t *k, long *vp)
{
    object_t    *v;

    if ((v = fetch(o, k)) == NULL)
        return 1;
    if (!isint(v))
        return ici_fetch_mismatch(o, k, v, "an int");
    *vp = intof(v)->i_value;
    return 0;
}

/*
 * ici_cmkvar(scope, name, typespec, value)
 *
 * This function is a simple way to define variables (to the ICI level).
 * It takes a scope structure, name for the variable, a type specification
 * (see below) and the variable's initial value and creates a variable in
 * the specified scope.
 *
 * Type specification is similar to other internal functions (ici_func,
 * ici_typecheck etc..) and uses the following key-letters
 *
 * Type Letter  ICI type        C type (of value)
 * ================================================
 *      i       int             long *
 *      f       float           double *
 *      s       string          char *
 *      u       file            FILE *
 *      o       any             object_t *
 */
int
ici_cmkvar(objwsup_t *scope, char *name, int type, void *vp)
{
    string_t    *s;
    int         i;

    if ((s = new_cname(name)) == NULL)
        return 1;
    i = ici_set_val(scope, s, type, vp);
    decref(s);
    return i;
}
