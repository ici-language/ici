#define NOCLASSPROTO

#define ICI_CORE
#include "exec.h"
#include "func.h"
#include "str.h"
#include "int.h"
#include "float.h"
#include "struct.h"
#include "buf.h"
#include "re.h"
#include "null.h"
#include "op.h"
#include "array.h"
#include "method.h"

/*
 * Return 0 if o (the subject object argument supplied to C implemented
 * methods) is present (indicating a method call was made) and is an
 * object with a super and, (if tcode != TC_NONE) has the given type
 * code. Else return 1 and set error appropriately.
 */
int
ici_method_check(object_t *o, int tcode)
{
    char        n1[30];
    char        n2[30];

    if (o == NULL)
    {
        sprintf(buf, "attempt to call method %s as a function",
            objname(n1, ici_os.a_top[-1]));
        ici_error = buf;
        return 1;
    }
    if (!hassuper(o) || (tcode != 0 && o->o_tcode != tcode))
    {
        sprintf(buf, "attempt to apply method %s to %s",
            objname(n1, ici_os.a_top[-1]),
            objname(n2, o));
        ici_error = buf;
        return 1;
    }
    return 0;
}

#ifndef NOCLASSPROTO
static int
assign_proto(struct_t *s, struct_t *d)
{
    struct_t    *proto;
    struct_t    *ss;
    slot_t      *sl;

    if (s == NULL)
        return 0;
    if (assign_proto(s->s_super, d))
        return 1;
    proto = ici_fetch_no_super_struct(s, SSO(proto));
    if (!isstruct(proto))
        return 0;
    for (sl = proto->s_slots; sl - proto->s_slots < proto->s_nslots; ++sl)
        if (sl->sl_key != NULL && assign(d, sl->sl_key, sl->sl_value))
            return 1;
    return 0;
}
#endif

/*
 * Implemantation of the ICI new method.
 */
static int
m_new(object_t *o)
{
    struct_t    *s;

    if (ici_method_check(o, 0))
        return 1;
    if ((s = new_struct()) == NULL)
        return 1;
    s->o_head.o_super = objwsupof(o);
#ifndef NOCLASSPROTO
    assign_proto(o, s);
#endif
    return ici_ret_with_decref(objof(s));
}

static int
m_isa(object_t *o)
{
    objwsup_t   *s;
    object_t    *class;

    if (ici_method_check(o, 0))
        return 1;
    if (ici_typecheck("o", &class))
        return 1;
    for (s = objwsupof(o); s != NULL; s = s->o_super)
    {
        if (objof(s) == class)
            return ici_ret_no_decref(objof(o_one));
    }
    return ici_ret_no_decref(objof(o_zero));
}

static int
m_respondsto(object_t *o)
{
    object_t    *classname;
    object_t    *v;

    if (ici_method_check(o, 0))
        return 1;
    if (ici_typecheck("o", &classname))
        return 1;
    if ((v = fetch(o, classname)) == NULL)
        return 1;
    if (ismethod(v) || isfunc(v))
        return ici_ret_no_decref(objof(o_one));
    return ici_ret_no_decref(objof(o_zero));
}

cfunc_t ici_oo_funcs[] =
{
    {CF_OBJ,    "new",          m_new},
    {CF_OBJ,    "isa",          m_isa},
    {CF_OBJ,    "respondsto",   m_respondsto},
    {CF_OBJ}
};
