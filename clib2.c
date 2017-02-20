/*
 * Process environment variable support - getenv() and putenv().
 */

#define ICI_CORE
#include "str.h"
#include "func.h"

/*
 * This struct maps environment variable names to the strings passed to
 * putenv() that set these variables. We need to do this because on
 * many systems putenv() simply uses the buffer passed to it and doesn't
 * take a copy of it and if that string changes the environment variables
 * go "funny". By keeping this struct around we ensure that there is always
 * a reference to the strings we pass to putenv() so ici doesn't go and
 * give away their space to other uses.
 */
static struct_t *ici_environ;

/*
 * Fill in the ici_environ struct from the process environment. Called
 * at initialisation time.
 */
static int
ici_init_environ(void)
{
    extern char **environ;
    char        **p;

    if ((ici_environ = new_struct()) == NULL)
        return 1;
    for (p = environ; *p != NULL; ++p)
    {
        string_t        *k;
        string_t        *v;
        char            *t;

        if ((t = strchr(*p, '=')) == NULL)
            continue; /* Ignore "broken" entries */
        if ((k = new_name(*p, t - *p)) == NULL)
            goto fail;
        if ((v = new_cname(*p)) == NULL)
        {
            decref(k);
            goto fail;
        }
        if (assign(ici_environ, k, v))
        {
            decref(v);
            decref(k);
            goto fail;
        }
        decref(v);
        decref(k);
    }
    return 0;

fail:
    decref(ici_environ);
    return 1;
}

/*
 * Return the value of an environment variable.
 */
static int
f_getenv(void)
{
    object_t    *n;

    if (ici_environ == NULL && ici_init_environ())
        return 1;
    if (ici_typecheck("o", &n))
        return 1;
    if (!isstring(n))
        return ici_argerror(0);
    if ((n = fetch(ici_environ, n)) != objof(&o_null))
    {
        char    *p;

        if ((p = strchr(stringof(n)->s_chars, '=')) == NULL)
            p = "";
        if ((n = objof(new_cname(p+1))) == NULL)
            return 1;
        decref(n); /* Because we use ici_ret_no_decref() below */
    }
    return ici_ret_no_decref(n);
}

/*
 * Set an environment variable.
 */
static int
f_putenv(void)
{
    string_t    *s;
    char        *t;
    string_t    *n;

    if (ici_environ == NULL && ici_init_environ())
        return 1;
    if (ici_typecheck("o", &s))
        return 1;
    if (!isstring(objof(s)))
        return ici_argerror(0);
    if ((t = strchr(s->s_chars, '=')) == NULL)
    {
        ici_error = "putenv() argument not in form ``name=value''";
        return 1;
    }
    if ((n = new_name(s->s_chars, t - s->s_chars)) == NULL)
        return 1;
    if (assign(ici_environ, n, s))
    {
        decref(n);
        return 1;
    }
    decref(n);
    putenv(s->s_chars);
    return null_ret();
}

cfunc_t extra_cfuncs[] =
{
    {CF_OBJ,    "getenv",       f_getenv},
    {CF_OBJ,    "putenv",       f_putenv},
    {CF_OBJ}
};
