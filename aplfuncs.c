#define ICI_CORE
#include "exec.h"
#include "func.h"
#include "str.h"
#include "int.h"
#include "struct.h"
#include "buf.h"
#include "null.h"
#include "op.h"
#include "array.h"

struct context
{
    object_t    **c_dlimit;     /* Addr or 1st dimension we don't use. */
    int         c_dstep;        /* Direction to step dnext by. +/-1. */
    char        c_option;       /* The option char. */
    object_t    **c_cstart;     /* First element of the content. */
    object_t    **c_climit;     /* Addr of 1st content we don't use. */
    object_t    **c_cnext;      /* Next content waiting to be used. */
    int         c_cstep;        /* Direction to step c_cnect by, +/-1. */
};


/*
 * Build a data structure according to the given dimensions and content.
 * The returned object has been ici_incref()ed.
 *
 * r is a pointer through which to store the resulting object.
 * dnext is a pointer to the dimension of interest to this call.
 * c is a pointer to a struct context containing parameters that are
 * independent of the recursion. See above.
 */
static int
buildxx(object_t **r, object_t **dnext, struct context *c)
{
    int         i;
    char        n1[30];
    char        n2[30];

    if (dnext == c->c_dlimit)
    {
        /*
         * We have run out of dimensions. Time to return an element of
         * the content. We must then step our content context in accordance
         * with the supplied option.
         */
        *r = *c->c_cnext;
        ici_incref(*r);
        c->c_cnext += c->c_cstep;
        switch (c->c_option)
        {
        case '\0':
        case 'c':
        case 'r': /* See end of function for restart case. */
            if (c->c_cnext == c->c_climit)
                c->c_cnext = c->c_cstart;
            break;

        case 'l':
            if (c->c_cnext == c->c_climit)
                c->c_cnext -= c->c_cstep;
            break;

        default:
            sprintf(buf, "option \"%c\" given to %s is not one of \"\", \"c\", \"r\" or \"l\"",
                c->c_option, objname(n1, ici_os.a_top[-1]));
            ici_error = buf;
            return 1;
        }
        return 0;
    }
    if (isint(*dnext))
    {
        array_t         *a;
        int             n;

        /*
         * We have an int dimension. We must make an array that big and
         * recursively fill it based on the next dimension or content.
         */
        n = intof(*dnext)->i_value;
        if ((a = ici_array_new(n)) == NULL)
            return 1;
        for (i = 0; i < n; ++i)
        {
            if (buildxx(a->a_top, dnext + c->c_dstep, c))
            {
                ici_decref(a);
                return 1;
            }
            ++a->a_top;
            ici_decref(a->a_top[-1]);
        }
        *r = objof(a);
    }
    else if (isarray(*dnext))
    {
        array_t         *a;
        struct_t        *s;
        object_t        **e;
        object_t        *o;

        /*
         * We have an array dimension. This means a struct with the elements
         * of the array as keys. We must recursively build the struct elememts
         * with the next dimension or content.
         */
        a = arrayof(*dnext);
        if ((s = ici_struct_new()) == NULL)
            return 1;
        for (e = ici_astart(a); e != ici_alimit(a); e = ici_anext(a, e))
        {
            if (buildxx(&o, dnext + c->c_dstep, c))
            {
                ici_decref(s);
                return 1;
            }
            if (ici_assign(s, *e, o))
            {
                ici_decref(s);
                return 1;
            }
            ici_decref(o);
        }
        *r = objof(s);
    }
    else
    {
        sprintf(buf, "%s supplied as a dimension to %s",
            objname(n1, *dnext), objname(n2, ici_os.a_top[-1]));
        ici_error = buf;
        return 1;
    }
    if (c->c_option == 'r')
        c->c_cnext = c->c_cstart;
    return 0;
}

static int
f_build()
{
    object_t            **dstart;
    int                 i;
    object_t            *r;
    object_t            *default_content;
    struct context      c;

    memset(&c, 0, sizeof c);
    dstart = &ARG(0);
    c.c_dlimit = &ARG(NARGS()); /* Assume for the moment. */
    c.c_dstep = -1;
    for (i = 0; i < NARGS(); ++i)
    {
        if (isstring(ARG(i)))
        {
            c.c_dlimit = &ARG(i); /* Revise. */
            c.c_option = stringof(ARG(i))->s_chars[0];
            if (++i < NARGS())
            {
                c.c_cstart = &ARG(i);
                c.c_climit = &ARG(NARGS());
                c.c_cstep = -1;
            }
            break;
        }
    }
    if (dstart == c.c_dlimit)
        return ici_null_ret();
    if (c.c_cstart == NULL)
    {
        default_content = objof(&o_null);
        c.c_cstart = &default_content;
        c.c_climit = c.c_cstart + 1;
        c.c_cstep = 1;
    }
    c.c_cnext = c.c_cstart;
    if (buildxx(&r, dstart, &c))
        return 1;
    return ici_ret_with_decref(r);
}

cfunc_t ici_apl_funcs[] =
{
    {CF_OBJ,    (char *)SS(build),        f_build},
    {CF_OBJ}
};
