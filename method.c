#define ICI_CORE
#include "method.h"
#include "exec.h"
#include "buf.h"
#include "primes.h"
#include "str.h"
#ifndef NOPROFILE
#include "profile.h"
#endif

static void
objname_method(object_t *o, char p[ICI_OBJNAMEZ])
{
    char    n1[ICI_OBJNAMEZ];
    char    n2[ICI_OBJNAMEZ];

    objname(n1, methodof(o)->m_subject);
    objname(n2, methodof(o)->m_callable);
    sprintf(p, "(%.13s:%.13s)", n1, n2);
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_method(object_t *o)
{
    o->o_flags |= O_MARK;
    return mark(methodof(o)->m_subject)
        + mark(methodof(o)->m_callable);
}

method_t *
ici_new_method(object_t *subject, object_t *callable)
{
    register method_t   *m;

    if ((m = ici_talloc(method_t)) == NULL)
        return NULL;
    objof(m)->o_tcode = TC_MEM;
    assert(ici_typeof(m) == &ici_method_type);
    objof(m)->o_tcode = 0;
    objof(m)->o_nrefs = 1;
    objof(m)->o_flags = 0;
    rego(m);
    m->m_subject = subject;
    m->m_callable = callable;
    return m;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_method(object_t *o)
{
    ici_tfree(o, method_t);
}

/*
 * Return the object which at key k of the obejct o, or NULL on error.
 * See the comment on t_fetch in object.h.
 */
static object_t *
fetch_method(object_t *o, object_t *k)
{
    method_t            *m;

    m = methodof(o);
    if (k == SSO(subject))
        return m->m_subject;
    if (k == SSO(callable))
        return m->m_callable;
    return objof(&o_null);
}

static int
call_method(object_t *o, object_t *subject, string_t *method)
{
    method_t            *m;

    m = methodof(o);
    if (ici_typeof(m->m_callable)->t_call == NULL)
    {
        char    n1[ICI_OBJNAMEZ];
        char    n2[ICI_OBJNAMEZ];

        sprintf(buf, "attempt to call %s:%s",
            objname(n1, m->m_subject),
            objname(n2, m->m_callable));
        ici_error = buf;
        return 1;
    }
    return (*ici_typeof(m->m_callable)->t_call)
    (
        m->m_callable,
        m->m_subject
    );
}

type_t  ici_method_type =
{
    mark_method,
    free_method,
    hash_unique,
    cmp_unique,
    copy_simple,
    assign_simple,
    fetch_method,
    "method",
    objname_method,
    call_method
};
