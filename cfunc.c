/*
 * cfunc.c
 *
 * Implementations of many (not all) of the core language intrinsic functions.
 * For historical reasons this is *NOT* the code associated with the cfunc_t
 * type. That's in cfunco.c
 *
 * This is public domain source. Please do not add any copyrighted material.
 */
#define ICI_CORE
#include "exec.h"
#include "func.h"
#include "str.h"
#include "int.h"
#include "float.h"
#include "struct.h"
#include "set.h"
#include "op.h"
#include "ptr.h"
#include "buf.h"
#include "file.h"
#include "re.h"
#include "null.h"
#include "parse.h"
#include "mem.h"
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#ifdef  _WIN32
#include <windows.h>
#endif

#ifndef NOWAITFOR
/*
 * For select() for waitfor().
 */
#include <sys/types.h>

#ifdef  BSD
#include <sys/time.h>
#else
#ifdef mips
#include <bsd/sys/types.h>
#include <bsd/sys/time.h>
#else
#ifdef hpux
#include <time.h>
#else
#ifndef _R3000
#include <sys/select.h>
#include <sys/times.h>
#endif
#endif
#endif
#endif
#ifndef bzero
#define bzero(p,n)      (memset((p), 0, (n)))
#endif
#endif

#if !defined(_WIN32) || (defined(sun) && (defined(__GNUC__) || !__STDC__))
extern int      fgetc();
#endif

/* Cached references to objects. */
static func_t   *sort_cmp_func;

/*
 * Cleans up data structures allocated/referenced in this module.
 * Required for a clean shutdown.
 */
void
uninit_cfunc(void)
{
    if (sort_cmp_func != NULL)
    {
        decref(sort_cmp_func);
        sort_cmp_func = NULL;
    }
}

/*
 * ici_typecheck(argspec, &arg1, &arg2...)
 *
 * Check ICI/C function argument types and translate into normal C data types.
 * The argspec is a character string.  Each character corresponds to
 * an actual argument to the ICI function which will (may) be assigned
 * through the corresponding pointer taken from the subsequent arguments.
 * Any detected type mismatches result in a non-zero return.  If all types
 * match, all assignments will be made and zero will be returned.
 *
 * The argspec key letters and their meaning are:
 *
 * o    Any ICI object is required in the actuals, the corresponding pointer
 *      must be a pointer to a (object_t *); which will be set to the actual
 *      argument.
 * p    An ICI ptr object is required in the actuals, then as for o.
 * d    An ICI struct object is required in the actuals, then as for o.
 * a    An ICI array object is required in the actuals, then as for o.
 * u    An ICI file object is required in the actuals, then as for o.
 * r    An ICI regexp object is required in the actuals, then as for o.
 * m    An ICI mem object is required in the actuals, then as for o.
 * i    An ICI int object is required in the actuals, the value of this int
 *      will be stored through the corresponding pointer which must be
 *      a (long *).
 * f    An ICI float object is required in the actuals, the value of this float
 *      will be stored through the corresponding pointer which must be
 *      a (double *).
 * n    An ICI float or int object is required in the actuals, the value of
 *      this float or int will be stored through the corresponding pointer
 *      which must be a (double *).
 * s    An ICI string object is required in the actuals, the corresponding
 *      pointer must be a (char **).  A pointer to the raw characters of
 *      the string will be stored through this (this will be '\0' terminated
 *      by virtue of all ICI strings having a gratuitous '\0' just past
 *      their real end).
 * -    The acutal parameter at this position is skipped, but it must be
 *      present.
 * *    All remaining actual parametes are ignored (even if there aren't any).
 *
 * The capitalisation of any of the alphabetic key letters above changes
 * their meaning.  The acutal must be an ICI ptr type.  The value this
 * pointer points to is taken to be the value which the above descriptions
 * concern themselves with (i.e. in place of the raw actual parameter).
 *
 * There must be exactly as many actual arguments as key letters unless
 * the last key letter is a *.
 *
 * Error returns have the usual ICI error conventions.
 */
int
ici_typecheck(char *types, ...)
{
    va_list             va;
    register object_t   **ap;   /* Argument pointer. */
    register int        nargs;
    register int        i;
    char                *ptr;   /* Subsequent things from va_alist. */
    register int        tcode;
    register object_t   *o;

    va_start(va, types);
    nargs = NARGS();
    ap = ARGS();
    for (i = 0; types[i] != '\0'; ++i, --ap)
    {
        if (types[i] == '*')
        {
            va_end(va);
            return 0;
        }

        if (i == nargs)
        {
            va_end(va);
            return ici_argcount(strlen(types));
        }

        if ((tcode = types[i]) == '-')
            continue;

        ptr = va_arg(va, char *);
        if (tcode >= 'A' && tcode <= 'Z')
        {
            if (!isptr(*ap))
                goto fail;
            if ((o = fetch(*ap, objof(o_zero))) == NULL)
                goto fail;
            tcode += 'a' - 'A';
        }
        else
        {
            o = *ap;
        }

        switch (tcode)
        {
        case 'o': /* Any object. */
            *(object_t **)ptr = o;
            break;

        case 'p': /* Any pointer. */
            if (!isptr(o))
                goto fail;
            *(ptr_t **)ptr = ptrof(o);
            break;

        case 'i': /* An int -> long. */
            if (!isint(o))
                goto fail;
            *(long *)ptr = intof(o)->i_value;
            break;

        case 's': /* A string -> (char *). */
            if (!isstring(o))
                goto fail;
            *(char **)ptr = stringof(o)->s_chars;
            break;

        case 'f': /* A float -> double. */
            if (!isfloat(o))
                goto fail;
            *(double *)ptr = floatof(o)->f_value;
            break;

        case 'n': /* A number, int or float -> double. */
            if (isint(o))
                *(double *)ptr = intof(o)->i_value;
            else if (isfloat(o))
                *(double *)ptr = floatof(o)->f_value;
            else
                goto fail;
            break;

        case 'd': /* A struct ("dict") -> (struct_t *). */
            if (!isstruct(o))
                goto fail;
            *(struct_t **)ptr = structof(o);
            break;

        case 'a': /* An array -> (array_t *). */
            if (!isarray(o))
                goto fail;
            *(array_t **)ptr = arrayof(o);
            break;

        case 'u': /* A file -> (file_t *). */
            if (!isfile(o))
                goto fail;
            *(file_t **)ptr = fileof(o);
            break;

        case 'r': /* A regular expression -> (regexpr_t *). */
            if (!isregexp(o))
                goto fail;
            *(regexp_t **)ptr = regexpof(o);
            break;

        case 'm': /* A mem -> (mem_t *). */
            if (!ismem(o))
                goto fail;
            *(mem_t **)ptr = memof(o);
            break;
        }
    }
    va_end(va);
    if (i != nargs)
        return ici_argcount(i);
    return 0;

fail:
    return ici_argerror(i);
}

/*
 * ici_retcheck(retspec, &arg1, &arg2...)
 *
 * Perform storage of values through pointers in the actual arguments to
 * an ICI/C function.
 *
 * The retspec is a character string consisting of key letters which
 * correspond to actual arguments of the current ICI/C function.
 * Each of the characters in the retspec has the following meaning.
 *
 * o    The actual argument must be a ptr, the corresponding pointer is
 *      assumed to be an (object_t **).  The location indicated by the
 *      ptr object is updated with the (object_t *).
 * d
 * a
 * u    Likwise for types as per ici_typecheck() above.
 * ...
 * -    The acutal argument is skipped.
 * *    ...
 */
int
ici_retcheck(char *types, ...)
{
    va_list             va;
    register int        i;
    register int        nargs;
    register object_t   **ap;
    char                *ptr;
    register int        tcode;
    register object_t   *o;
    register object_t   *s;

    va_start(va, types);
    nargs = NARGS();
    ap = ARGS();
    for (i = 0; types[i] != '\0'; ++i, --ap)
    {
        if ((tcode = types[i]) == '*')
        {
            va_end(va);
            return 0;
        }

        if (i == nargs)
        {
            va_end(va);
            return ici_argcount(strlen(types));
        }

        if (tcode == '-')
            continue;

        o = *ap;
        if (!isptr(o))
            goto fail;

        ptr = va_arg(va, char *);

        switch (tcode)
        {
        case 'o': /* Any object. */
            *(object_t **)ptr = o;
            break;

        case 'p': /* Any pointer. */
            if (!isptr(o))
                goto fail;
            *(ptr_t **)ptr = ptrof(o);
            break;

        case 'i':
            if ((s = objof(new_int(*(long *)ptr))) == NULL)
                goto ret1;
            if (assign(o, o_zero, s))
                goto ret1;
            decref(s);
            break;

        case 's':
            if ((s = objof(new_cname(*(char **)ptr))) == NULL)
                goto ret1;
            if (assign(o, o_zero, s))
                goto ret1;
            decref(s);
            break;

        case 'f':
            if ((s = objof(new_float(*(double *)ptr))) == NULL)
                goto ret1;
            if (assign(o, o_zero, s))
                goto ret1;
            decref(s);
            break;

        case 'd':
            if (!isstruct(o))
                goto fail;
            *(struct_t **)ptr = structof(o);
            break;

        case 'a':
            if (!isarray(o))
                goto fail;
            *(array_t **)ptr = arrayof(o);
            break;

        case 'u':
            if (!isfile(o))
                goto fail;
            *(file_t **)ptr = fileof(o);
            break;

        case '*':
            return 0;

        }
    }
    va_end(va);
    if (i != nargs)
        return ici_argcount(i);
    return 0;

ret1:
    va_end(va);
    return 1;

fail:
    va_end(va);
    return ici_argerror(i);
}

/*
 * Generate a generic error message to indicate that argument i of the
 * current intrinsic function is bad. Despite being generic, this message
 * is generally pretty informative and useful.
 *
 * The argument number is base 0. I.e. ici_argerror(0) indicates the
 * 1st argument is bad.
 *
 * Returns 1, and is suitable for using in a direct return from an
 * intrinsic function, as in:
 *
 *      return ici_argerror(2);
 */
int
ici_argerror(int i)
{
    char        n1[30];
    char        n2[30];

    sprintf(buf, "argument %d of %s incorrectly supplied as %s",
        i + 1,
        objname(n1, ici_os.a_top[-1]),
        objname(n2, ARG(i)));
    ici_error = buf;
    return 1;
}

/*
 * Generate a generic error message to indicate that the wrong number of
 * arguments have been supplied to this intrinsic function, and that
 * it really (or normally) takes n.
 *
 * Returns 1, and is suitable for using in a direct return from an
 * intrinsic function, as in:
 *
 *      return ici_argcount(2);
 */
int
ici_argcount(int n)
{
    char        n1[30];

    sprintf(buf, "%d arguments given to %s, but it takes %d",
        NARGS(), objname(n1, ici_os.a_top[-1]), n);
    ici_error = buf;
    return 1;
}

/*
 * General way out of an intrinsic function returning the object o, but
 * the given object has a reference count which must be decref'ed on the
 * way out. Return 0 unless the given o in NULL, in which case it returns
 * 1 with no other action.
 *
 * This is suitable for using as a return from an intrinsic function
 * as say:
 *
 *      return ici_ret_with_decref(objof(new_int(2)));
 *
 * (Although see int_ret() below.) If the object you wish to return does
 * not have an extra reference, use ici_ret_no_decref() (see below).
 */
int
ici_ret_with_decref(object_t *o)
{
    if (o == NULL)
        return 1;
    ici_os.a_top -= NARGS() + 1;
    ici_os.a_top[-1] = o;
    decref(o);
    --ici_xs.a_top;
    return 0;
}

/*
 * General way out of an intrinsic function returning the object o where
 * the given object has no extra refernce count. Returns 0 indicating no
 * error.
 *
 * This is suitable for using as a return from an intrinsic function
 * as say:
 *
 *      return ici_ret_no_decref(o);
 *
 * If the object you are returning has an extra reference which must be
 * decremented as part of the return, use ici_ret_with_decref() (above).
 */
int
ici_ret_no_decref(object_t *o)
{
    ici_os.a_top -= NARGS() + 1;
    ici_os.a_top[-1] = o;
    --ici_xs.a_top;
    return 0;
}

/*
 * Use 'return int_ret(n);' to return an integer from an intrinsic fuction.
 */
int
int_ret(long ret)
{
    return ici_ret_with_decref(objof(new_int(ret)));
}

/*
 * Use 'return float_ret(n);' to return a float from an intrinsic fuction.
 */
int
float_ret(double ret)
{
    return ici_ret_with_decref(objof(new_float(ret)));
}

/*
 * Use 'return str_ret(n);' to return a string from an intrinsic fuction.
 */
int
str_ret(char *str)
{
    return ici_ret_with_decref(objof(new_cname(str)));
}

file_t *
need_stdin()
{
    file_t              *f;

    f = fileof(fetch(ici_vs.a_top[-1], SSO(_stdin)));
    if (!isfile(f))
    {
        ici_error = "stdin is not a file";
        return NULL;
    }
    return f;
}

file_t *
need_stdout()
{
    file_t              *f;

    f = fileof(fetch(ici_vs.a_top[-1], SSO(_stdout)));
    if (!isfile(f))
    {
        ici_error = "stdout is not a file";
        return NULL;
    }
    return f;
}

#ifndef NOMATH

#if !__STDC__
# ifdef sun
/*
 * Math exception handler for SVID compliant systems. We just set errno
 * to the appropriate value and return non-zero. This stops the output
 * of a message to stderr and allows normal error handling to control
 * the behaviour.
 */
int
matherr(exc)
struct exception *exc;
{
    switch (exc->type)
    {
    case DOMAIN:
    case SING:
        errno = EDOM;
        break;
    case OVERFLOW:
    case UNDERFLOW:
        errno = ERANGE;
        break;
    }
    return 1;
}
# endif
#endif

/*
 * For any C functions that return a double and take 0, 1, or 2 doubles as
 * arguments.
 */
int
f_math()
{
    double      av[2];
    double      r;

    if (ici_typecheck(CF_ARG2() + 2, &av[0], &av[1]))
        return 1;
    errno = 0;
    r = (*(double (*)())CF_ARG1())(av[0], av[1]);
    if (errno != 0)
    {
        ici_error = syserr();
        return 1;
    }
    return float_ret(r);
}
#endif

static int
f_struct()
{
    object_t            **o;
    int                 nargs;
    struct_t            *s;
    objwsup_t           *super;

    nargs = NARGS();
    o = ARGS();
    super = NULL;
    if (nargs & 1)
    {
        super = objwsupof(*o);
        if (!hassuper(super) && !isnull(objof(super)))
            return ici_argerror(0);
        if (isnull(objof(super)))
            super = NULL;
        --nargs;
        --o;
    }
    if ((s = new_struct()) == NULL)
        return 1;
    for (; nargs >= 2; nargs -= 2, o -= 2)
    {
        if (assign(s, o[0], o[-1]))
        {
            decref(s);
            return 1;
        }
    }
    s->o_head.o_super = super;
    return ici_ret_with_decref(objof(s));
}

static int
f_set()
{
    register int        nargs;
    register set_t      *s;
    register object_t   **o;

    if ((s = new_set()) == NULL)
        return 1;
    for (nargs = NARGS(), o = ARGS(); nargs > 0; --nargs, --o)
    {
        if (assign(s, *o, o_one))
        {
            decref(s);
            return 1;
        }
    }
    return ici_ret_with_decref(objof(s));
}

static int
f_array()
{
    register int        nargs;
    register array_t    *a;
    register object_t   **o;

    nargs = NARGS();
    if ((a = new_array(nargs)) == NULL)
        return 1;
    for (o = ARGS(); nargs > 0; --nargs)
        *a->a_top++ = *o--;
    return ici_ret_with_decref(objof(a));
}

static int
f_keys()
{
    struct_t            *s;
    register array_t    *k;
    register slot_t     *sl;

    if (ici_typecheck("d", &s))
        return 1;
    if ((k = new_array(s->s_nels)) == NULL)
        return 1;
    for (sl = s->s_slots; sl < s->s_slots + s->s_nslots; ++sl)
    {
        if (sl->sl_key != NULL)
            *k->a_top++ = sl->sl_key;
    }
    return ici_ret_with_decref(objof(k));
}

static int
f_copy(object_t *o)
{
    if (o != NULL)
        return ici_ret_with_decref(copy(o));
    if (NARGS() != 1)
        return ici_argcount(1);
    return ici_ret_with_decref(copy(ARG(0)));
}

static int
f_typeof()
{
    if (NARGS() != 1)
        return ici_argcount(1);
    if (ici_typeof(ARG(0))->t_ici_name == NULL)
        ici_typeof(ARG(0))->t_ici_name = new_cname(ici_typeof(ARG(0))->t_name);
    return ici_ret_no_decref(objof(ici_typeof(ARG(0))->t_ici_name));
}

static int
f_nels()
{
    register object_t   *o;
    long                size;

    if (NARGS() != 1)
        return ici_argcount(1);
    o = ARG(0);
    if (isstring(o))
        size = stringof(o)->s_nchars;
    else if (isarray(o))
        size = ici_array_nels(arrayof(o));
    else if (isstruct(o))
        size = structof(o)->s_nels;
    else if (isset(o))
        size = setof(o)->s_nels;
    else if (ismem(o))
        size = memof(o)->m_length;
    else
        size = 1;
    return int_ret(size);
}

static int
f_int()
{
    register object_t   *o;
    register long       v;

    if (NARGS() != 1)
        return ici_argcount(1);
    o = ARG(0);
    if (isint(o))
        return ici_ret_no_decref(o);
    else if (isstring(o))
        v = ici_strtol(stringof(o)->s_chars, NULL, 0);
    else if (isfloat(o))
        v = (long)floatof(o)->f_value;
    else
        v = 0;
    return int_ret(v);
}

static int
f_float()
{
    register object_t   *o;
    register double     v;
    extern double       strtod();

    if (NARGS() != 1)
        return ici_argcount(1);
    o = ARG(0);
    if (isfloat(o))
        return ici_ret_no_decref(o);
    else if (isstring(o))
        v = strtod(stringof(o)->s_chars, NULL);
    else if (isint(o))
        v = (double)intof(o)->i_value;
    else
        v = 0;
    return float_ret(v);
}

static int
f_num()
{
    register object_t   *o;
    register double     f;
    register long       i;
    char                *s;
    extern double       strtod();
    char                n[30];

    if (NARGS() != 1)
        return ici_argcount(1);
    o = ARG(0);
    if (isfloat(o) || isint(o))
        return ici_ret_no_decref(o);
    else if (isstring(o))
    {
        i = ici_strtol(stringof(o)->s_chars, &s, 0);
        if (*s == '\0')
            return int_ret(i);
        f = strtod(stringof(o)->s_chars, &s);
        if (*s == '\0')
            return float_ret(f);
    }
    sprintf(buf, "%s is not a number", objname(n, o));
    ici_error = buf;
    return 1;
}

static int
f_string()
{
    register object_t   *o;

    if (NARGS() != 1)
        return ici_argcount(1);
    o = ARG(0);
    if (isstring(o))
        return ici_ret_no_decref(o);
    if (isint(o))
        sprintf(buf, "%ld", intof(o)->i_value);
    else if (isfloat(o))
        sprintf(buf, "%g", floatof(o)->f_value);
    else if (isregexp(o))
        return ici_ret_no_decref(objof(regexpof(o)->r_pat));
    else
        sprintf(buf, "<%s>", ici_typeof(o)->t_name);
    return str_ret(buf);
}

static int
f_eq()
{
    object_t    *o1;
    object_t    *o2;

    if (ici_typecheck("oo", &o1, &o2))
        return 1;
    if (o1 == o2)
        return ici_ret_no_decref(objof(o_one));
    return ici_ret_no_decref(objof(o_zero));
}

static int
f_push()
{
    array_t     *a;
    object_t    *o;

    if (ici_typecheck("ao", &a, &o))
        return 1;
    if (ici_array_push(a, o))
        return 1;
    return ici_ret_no_decref(o);
}

static int
f_rpush()
{
    array_t     *a;
    object_t    *o;

    if (ici_typecheck("ao", &a, &o))
        return 1;
    if (ici_array_rpush(a, o))
        return 1;
    return ici_ret_no_decref(o);
}

static int
f_pop()
{
    array_t     *a;
    object_t    *o;

    if (ici_typecheck("a", &a))
        return 1;
    if ((o = ici_array_pop(a)) == NULL)
        return 1;
    return ici_ret_no_decref(o);
}

static int
f_rpop()
{
    array_t     *a;
    object_t    *o;

    if (ici_typecheck("a", &a))
        return 1;
    if ((o = ici_array_rpop(a)) == NULL)
        return 1;
    return ici_ret_no_decref(o);
}

static int
f_top()
{
    array_t     *a;
    long        n = 0;

    switch (NARGS())
    {
    case 1:
        if (ici_typecheck("a", &a))
            return 1;
        break;

    default:
        if (ici_typecheck("ai", &a, &n))
            return 1;
    }
    n += ici_array_nels(a) - 1;
    return ici_ret_no_decref(ici_array_get(a, n));
}

static int
f_parse()
{
    object_t    *o;
    file_t      *f;
    struct_t    *s;     /* Statics. */
    struct_t    *a;     /* Autos. */

    switch (NARGS())
    {
    case 1:
        if (ici_typecheck("o", &o))
            return 1;
        if ((a = new_struct()) == NULL)
            return 1;
        if ((a->o_head.o_super = objwsupof(s = new_struct())) == NULL)
        {
            decref(a);
            return 1;
        }
        decref(s);
        s->o_head.o_super = objwsupof(ici_vs.a_top[-1])->o_super;
        break;

    default:
        if (ici_typecheck("od", &o, &a))
            return 1;
        incref(a);
        break;
    }

    if (isstring(o))
    {
        if ((f = ici_sopen(stringof(o)->s_chars, stringof(o)->s_nchars)) == NULL)
        {
            decref(a);
            return 1;
        }
        f->f_name = SS(empty_string);
    }
    else if (isfile(o))
        f = fileof(o);
    else
    {
        decref(a);
        return ici_argerror(0);
    }

    if (parse_module(f, objwsupof(a)) < 0)
        goto fail;

    if (isstring(o))
        decref(f);
    return ici_ret_with_decref(objof(a));

fail:
    if (isstring(o))
        decref(f);
    decref(a);
    return 1;
}

static int
f_include()
{
    string_t    *filename;
    struct_t    *a;
    int         rc;
    file_t      *f;

    switch (NARGS())
    {
    case 1:
        if (ici_typecheck("o", &filename))
            return 1;
        a = structof(ici_vs.a_top[-1]);
        break;

    case 2:
        if (ici_typecheck("od", &filename, &a))
            return 1;
        break;

    default:
        return ici_argcount(2);
    }
    if (!isstring(objof(filename)))
        return ici_argerror(0);
#ifndef NODEBUGGING
    ici_debug_ignore_errors();
#endif
    if ((ici_error = ici_call("fopen", "o=o", &f, filename)) != NULL)
    {
    char    fname[1024];

    strncpy(fname, filename->s_chars, 1023);
    if (!ici_find_on_path(ici_get_dll_path(), fname, NULL))
    {
        ici_error = "can't find include file";
        return 1;
    }
        if ((ici_error = ici_call("fopen", "o=s", &f, fname)) != NULL)
    {
#ifndef NODEBUGGING
        ici_debug_respect_errors();
#endif
            return 1;
    }
    }
#ifndef NODEBUGGING
    ici_debug_respect_errors();
#endif
    rc = parse_module(f, objwsupof(a));
    ici_call("close", "o", f);
    decref(f);
    return rc < 0 ? 1 : ici_ret_no_decref(objof(a));
}

static int
f_call(void)
{
    array_t     *aa;        /* The array with extra arguments, or NULL. */
    int         nargs;      /* Number of args to target function. */
    int         naargs;     /* Number of args comming from the array. */
    object_t    **base;
    object_t    **e;
    object_t    *opcall;
    int         i;
    int_t       *nargso;
    object_t    *func;

    if (NARGS() < 2)
        return ici_argcount(2);
    func = ARG(0);
    incref(func);
    nargso = NULL;
    base = &ARG(NARGS() - 1);
    if (isarray(*base))
        aa = arrayof(*base);
    else if (isnull(*base))
        aa = NULL;
    else
        return ici_argerror(NARGS() - 1);
    if (aa == NULL)
        naargs = 0;
    else
        naargs = ici_array_nels(aa);
    nargs = naargs + NARGS() - 2;
    /*
     * On the operand stack, we have...
     *    [aa] [argn]...[arg2] [arg1] [func] [nargs] [us] [    ]
     *      ^                                               ^
     *      +-base                                          +-ici_os.a_top
     *
     * We want...
     *    [aa[n]]...[aa[1]] [aa[0]] [argn]...[arg2] [arg1] [nargs] [func] [    ]
     *      ^                                                               ^
     *      +-base                                                          +-ici_os.a_top
     *
     * Do everything that can get an error first, before we start playing with
     * the stack.
     *
     * We include an extra 80 in our ici_stk_push_chk, see start of
     * ici_evaluate().
     */
    if (ici_stk_push_chk(&ici_os, naargs + 80))
        goto fail;
    if ((nargso = new_int(nargs)) == NULL)
        goto fail;
    /*
     * First move the arguments that we want to keep up to the stack
     * to their new position (all except the func and the array).
     */
    memmove(base + naargs, base + 1, (NARGS() - 2) * sizeof(object_t *));
    ici_os.a_top += naargs - 2;
    if (naargs > 0)
    {
        i = naargs;
        for (e = ici_astart(aa); e < ici_alimit(aa); e = ici_anext(aa, e))
            base[--i] = *e;
    }
    /*
     * Push the count of actual args and the target function.
     */
    ici_os.a_top[-2] = objof(nargso);
    decref(nargso);
    ici_os.a_top[-1] = func;
    decref(func);
    ici_xs.a_top[-1] = objof(&o_call);
    /*
     * Very special return. Drops back into the execution loop with
     * the call on the execution stack.
     */
    return 0;

fail:
    decref(func);
    if (nargso != NULL)
        decref(nargso);
    return 1;
}

static int
f_fail()
{
    char        *s;

    if (ici_typecheck("s", &s))
        return 1;
    if (ici_chkbuf(strlen(s)))
        return 1;
    strcpy(buf, s);
    ici_error = buf;
    return 1;
}

static int
f_exit()
{
    object_t    *rc;
    long        status;

    switch (NARGS())
    {
    case 0:
        rc = objof(&o_null);
        break;
    case 1:
        if (ici_typecheck("o", &rc))
        return 1;
        break;
    default:
        return ici_argcount(1);
    }
    if (isint(rc))
        status = (int)intof(rc)->i_value;
    else if (rc == objof(&o_null))
        status = 0;
    else if (isstring(rc))
    {
        if (stringof(rc)->s_nchars == 0)
            status = 0;
        else
        {
            if (strchr(stringof(rc)->s_chars, ':') != NULL)
                fprintf(stderr, "%s\n", stringof(rc)->s_chars);
            else
                fprintf(stderr, "exit: %s\n", stringof(rc)->s_chars);
            status = 1;
        }
    }
    else
    {
        return ici_argerror(0);
    }
    wrapup();
    exit((int)status);
    /*NOTREACHED*/
}

static int
f_vstack()
{
    return ici_ret_with_decref(copy(objof(&ici_vs)));
}

static int
f_tochar()
{
    long        i;

    if (ici_typecheck("i", &i))
        return 1;
    buf[0] = (unsigned char)i;
    return ici_ret_with_decref(objof(new_name(buf, 1)));
}

static int
f_toint()
{
    char        *s;

    if (ici_typecheck("s", &s))
        return 1;
    return int_ret((long)(s[0] & 0xFF));
}

static int
f_rand()
{
    static long seed    = 1;

    if (NARGS() >= 1)
    {
        if (ici_typecheck("i", &seed))
            return 1;
        srand(seed);
    }
#ifdef ICI_RAND_IS_C_RAND
    return int_ret(rand());
#else
    seed = seed * 1103515245 + 12345;
    return int_ret((seed >> 16) & 0x7FFF);
#endif
}

static int
f_interval()
{
    object_t            *o;
    long                start;
    long                length;
    long                nel;
    register string_t   *s = 0; /* init to shut up compiler */
    register array_t    *a = 0; /* init to shut up compiler */
    register array_t    *a1;


    if (ici_typecheck("oi*", &o, &start))
        return 1;
    length = -1;
    if (NARGS() > 2)
    {
        if (!isint(ARG(2)))
            return ici_argerror(2);
        if ((length = intof(ARG(2))->i_value) < 0)
            ici_argerror(2);
    }
    switch (o->o_tcode)
    {
    case TC_STRING:
        s = stringof(o);
        nel = s->s_nchars;
        break;

    case TC_ARRAY:
        a = arrayof(o);
        nel = a->a_top - a->a_base;
        break;

    default:
        return ici_argerror(0);
    }

    if (start < 0)
    {
        if ((start += nel) < 0)
            start = 0;
    }
    else if (start > nel)
        start = nel;
    if (length < 0)
        length = nel;
    if (start + length > nel)
        length = nel - start;

    if (o->o_tcode == TC_STRING)
    {
        return ici_ret_with_decref(objof(new_name(s->s_chars + start, (int)length)));
    }
    else
    {
        if ((a1 = new_array(length)) == NULL)
            return 1;
        ici_array_gather(a1->a_base, a, start, length);
        a1->a_top += length;
        return ici_ret_with_decref(objof(a1));
    }
}

static int
f_explode()
{
    register int        i;
    char                *s;
    array_t             *x;

    if (ici_typecheck("s", &s))
        return 1;
    i = stringof(ARG(0))->s_nchars;
    if ((x = new_array(i)) == NULL)
        return 1;
    while (--i >= 0)
    {
        if ((*x->a_top = objof(new_int(*s++ & 0xFFL))) == NULL)
        {
            decref(x);
            return 1;
        }
        decref(*x->a_top);
        ++x->a_top;
    }
    return ici_ret_with_decref(objof(x));
}

static int
f_implode()
{
    array_t             *a;
    int                 i;
    object_t            **o;
    string_t            *s;
    char                *p;

    if (ici_typecheck("a", &a))
        return 1;
    i = 0;
    for (o = ici_astart(a); o < ici_alimit(a); o = ici_anext(a, o))
    {
        switch ((*o)->o_tcode)
        {
        case TC_INT:
            ++i;
            break;

        case TC_STRING:
            i += stringof(*o)->s_nchars;
            break;
        }
    }
    if ((s = new_string(i)) == NULL)
        return 1;
    p = s->s_chars;
    for (o = ici_astart(a); o < ici_alimit(a); o = ici_anext(a, o))
    {
        switch ((*o)->o_tcode)
        {
        case TC_INT:
            *p++ = (char)intof(*o)->i_value;
            break;

        case TC_STRING:
            memcpy(p, stringof(*o)->s_chars, stringof(*o)->s_nchars);
            p += stringof(*o)->s_nchars;
            break;
        }
    }
    if ((s = stringof(atom(objof(s), 1))) == NULL)
        return 1;
    return ici_ret_with_decref(objof(s));
}

static int
f_sopen()
{
    file_t      *f;
    char        *str;
    char        *mode;

    mode = "r";
    if (ici_typecheck(NARGS() > 1 ? "ss" : "s", &str, &mode))
        return 1;
    if (strcmp(mode, "r") != 0 && strcmp(mode, "rb") != 0)
    {
        ici_chkbuf(strlen(mode) + 50);
        sprintf(buf, "attempt to use mode \"%s\" in sopen()", mode);
        ici_error = buf;
        return 1;
    }
    if ((f = ici_sopen(str, stringof(ARG(0))->s_nchars)) == NULL)
        return 1;
    f->f_name = SS(empty_string);
    return ici_ret_with_decref(objof(f));
}

static int
f_mopen()
{
    mem_t       *mem;
    file_t      *f;
    char        *mode;

    if (ici_typecheck("os", &mem, &mode))
    {
        if (ici_typecheck("o", &mem))
            return 1;
        mode = "r";
    }
    if (!ismem(objof(mem)))
        return ici_argerror(0);
    if (strcmp(mode, "r") && strcmp(mode, "rb"))
    {
        ici_error = "bad open mode for mopen";
        return 1;
    }
    if (mem->m_accessz != 1)
    {
        ici_error = "memory object must have access size of 1 to be opened";
        return 1;
    }
    if ((f = ici_sopen(mem->m_base, (int)mem->m_length)) == NULL)
        return 1;
    f->f_name = SS(empty_string);
    return ici_ret_with_decref(objof(f));
}

int
f_sprintf()
{
    char                *fmt;
    register char       *p;
    register int        i;              /* Where we are up to in buf. */
    register int        j;
    int                 which;
    int                 nargs;
    char                subfmt[40];     /* %...? portion of string. */
    int                 stars[2];       /* Precision and field widths. */
    int                 nstars;
    int                 gotl;           /* Have a long int flag. */
    long                ivalue;
    double              fvalue;
    char                *svalue;
    object_t            **o;            /* Argument pointer. */
    file_t              *file;
#ifdef  BAD_PRINTF_RETVAL
#define IPLUSEQ
#else
#define IPLUSEQ         i +=
#endif

    which = (int)CF_ARG1(); /* sprintf, printf, fprintf */
    if (which != 0 && NARGS() > 0 && isfile(ARG(0)))
    {
        which = 2;
        if (ici_typecheck("us*", &file, &fmt))
            return 1;
        o = ARGS() - 2;
        nargs = NARGS() - 2;
    }
    else
    {
        if (ici_typecheck("s*", &fmt))
            return 1;
        o = ARGS() - 1;
        nargs = NARGS() - 1;
    }

    p = fmt;
    i = 0;
    while (*p != '\0')
    {
        if (*p != '%')
        {
            if (ici_chkbuf(i))
                return 1;
            buf[i++] = *p++;
            continue;
        }

        nstars = 0;
        gotl = 0;
        subfmt[0] = *p++;
        j = 1;
        while (*p != '\0' && strchr("diouxXfeEgGcs%", *p) == NULL)
        {
            if (*p == '*')
                ++nstars;
            else if (*p == 'l')
                gotl = 1;
            subfmt[j++] = *p++;
        }
        if (gotl == 0 && strchr("diouxX", *p) != NULL)
            subfmt[j++] = 'l';
        subfmt[j++] = *p;
        subfmt[j++] = '\0';
        if (nstars > 2)
            nstars = 2;
        stars[0] = 0;
        stars[1] = 0;
        for (j = 0; j < nstars; ++j)
        {
            if (nargs <= 0)
                goto lacking;
            if (!isint(*o))
                goto type;
            stars[j] = (int)intof(*o)->i_value;
            --o;
            --nargs;
        }
        switch (*p++)
        {
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
            if (nargs <= 0)
                goto lacking;
            if (isint(*o))
                ivalue = intof(*o)->i_value;
            else if (isfloat(*o))
                ivalue = (long)floatof(*o)->f_value;
            else
                goto type;
            if (ici_chkbuf(i + 30)) /* Worst case. */
                return 1;
            switch (nstars)
            {
            case 0:
                IPLUSEQ sprintf(&buf[i], subfmt, ivalue);
                break;

            case 1:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], ivalue);
                break;

            case 2:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], stars[1], ivalue);
                break;
            }
            --o;
            --nargs;
            break;

        case 'c':
            if (nargs <= 0)
                goto lacking;
            if (isint(*o))
                ivalue = intof(*o)->i_value;
            else if (isfloat(*o))
                ivalue = (long)floatof(*o)->f_value;
            else
                goto type;
            if (ici_chkbuf(i + 30)) /* Worst case. */
                return 1;
            switch (nstars)
            {
            case 0:
                IPLUSEQ sprintf(&buf[i], subfmt, (int)ivalue);
                break;

            case 1:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], (int)ivalue);
                break;

            case 2:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], stars[1], (int)ivalue);
                break;
            }
            --o;
            --nargs;
            break;

        case 's':
            if (nargs <= 0)
                goto lacking;
            if (!isstring(*o))
                goto type;
            svalue = stringof(*o)->s_chars;
            if (ici_chkbuf(i + stringof(*o)->s_nchars + stars[0] + stars[1]))
                return 1;
            switch (nstars)
            {
            case 0:
                IPLUSEQ sprintf(&buf[i], subfmt, svalue);
                break;

            case 1:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], svalue);
                break;

            case 2:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], stars[1], svalue);
                break;
            }
            --o;
            --nargs;
            break;

        case 'f':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            if (nargs <= 0)
                goto lacking;
            if (isint(*o))
                fvalue = intof(*o)->i_value;
            else if (isfloat(*o))
                fvalue = floatof(*o)->f_value;
            else
                goto type;
            if (ici_chkbuf(i + 40)) /* Worst case. */
                return 1;
            switch (nstars)
            {
            case 0:
                IPLUSEQ sprintf(&buf[i], subfmt, fvalue);
                break;

            case 1:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], fvalue);
                break;

            case 2:
                IPLUSEQ sprintf(&buf[i], subfmt, stars[0], stars[1], fvalue);
                break;
            }
            --o;
            --nargs;
            break;

        case '%':
            if (ici_chkbuf(i))
                return 1;
            buf[i++] = '%';
            continue;
        }
#ifdef  BAD_PRINTF_RETVAL
        i = strlen(buf); /* BSD sprintf doesn't return usual value. */
#endif
    }
    buf[i] = '\0';
    switch (which)
    {
    case 1: /* printf */
        if ((file = need_stdout()) == NULL)
            return 1;
    case 2: /* fprintf */
        if (objof(file)->o_flags & F_CLOSED)
        {
            ici_error = "write to closed file";
            return 1;
        }
        {
            char        small_buf[128];
            char        *out_buf;
            exec_t      *x;

            if (i <= sizeof small_buf)
            {
                out_buf = small_buf;
            }
            else
            {
                if ((out_buf = ici_nalloc(i)) == NULL)
                    return 1;
            }
            memcpy(out_buf, buf, i);
            x = ici_leave();
            (*file->f_type->ft_write)(out_buf, i, file->f_file);
            ici_enter(x);
            if (out_buf != small_buf)
                ici_nfree(out_buf, i);
        }
        return int_ret((long)i);

    default: /* sprintf */
        return ici_ret_with_decref(objof(new_name(buf, i)));
    }

type:
    sprintf(buf, "attempt to use a %s with a \"%s\" format in sprintf",
        ici_typeof(*o)->t_name, subfmt);
    ici_error = buf;
    return 1;

lacking:
    ici_error = "not enoughs args to sprintf";
    return 1;
}

static int
f_currentfile()
{
    object_t    **o;

    for (o = ici_xs.a_top - 1; o >= ici_xs.a_base; --o)
    {
        if (isparse(*o))
            return ici_ret_no_decref(objof(parseof(*o)->p_file));
    }
    return null_ret();
}

static int
f_del()
{
    object_t    *s;
    object_t    *o;

    if (ici_typecheck("oo", &s, &o))
        return 1;
    if (isstruct(s))
    {
        unassign_struct(structof(s), o);
    }
    else if (isset(s))
    {
        unassign_set(setof(s), o);
    }
    else if (isarray(s))
    {
        array_t         *a;
        object_t        **e;
        object_t        **prev_e;

        a = arrayof(s);
        for (e = ici_astart(a); e < ici_alimit(a); e = ici_anext(a, e))
        {
            if (*e == o)
            {
                prev_e = e;
                for (e = ici_anext(a, e); e < ici_alimit(a); e = ici_anext(a, e))
                {
                    *prev_e = *e;
                    prev_e = e;
                }
                ici_array_pop(a);
                break;
            }
        }
    }
    else
    {
        return ici_argerror(1);
    }
    return null_ret();
}

/*
 * super_loop()
 *
 * Return 1 and set error if the super chain of the given struct has
 * a loop. Else return 0.
 */
static int
super_loop(objwsup_t *base)
{
    objwsup_t           *s;

    /*
     * Scan up the super chain setting the O_MARK flag as we go. If we hit
     * a marked struct, we must have looped. Note that the O_MARK flag
     * is a strictly transitory flag that can only be used in local
     * non-allocating areas such as this. It must be left cleared at all
     * times. The garbage collector assumes it is cleared on all objects
     * when it runs.
     */
    for (s = base; s != NULL; s = s->o_super)
    {
        if (objof(s)->o_flags & O_MARK)
        {
            /*
             * A loop. Clear all the O_MARK flags we set and set error.
             */
            for (s = base; objof(s)->o_flags & O_MARK; s = s->o_super)
                objof(s)->o_flags &= ~O_MARK;
            ici_error = "cycle in struct super chain";
            return 1;
        }
        objof(s)->o_flags |= O_MARK;
    }
    /*
     * No loop. Clear all the O_MARK flags we set.
     */
    for (s = base; s != NULL; s = s->o_super)
        objof(s)->o_flags &= ~O_MARK;
    return 0;
}

static int
f_super()
{
    objwsup_t           *o;
    objwsup_t           *newsuper;
    objwsup_t           *oldsuper;

    if (ici_typecheck("o*", &o))
        return 1;
    if (!hassuper(o))
        return ici_argerror(0);
    newsuper = oldsuper = o->o_super;
    if (NARGS() >= 2)
    {
        if (objof(o)->o_flags & O_ATOM)
        {
            ici_error = "attempt to set super of an atomic struct";
            return 1;
        }
        if (isnull(ARG(1)))
            newsuper = NULL;
        else if (hassuper(ARG(1)))
            newsuper = objwsupof(ARG(1));
        else
            return ici_argerror(1);
        ++ici_vsver;
    }
    o->o_super = newsuper;
    if (super_loop(o))
    {
        o->o_super = oldsuper;
        return 1;
    }
    if (oldsuper == NULL)
        return null_ret();
    return ici_ret_no_decref(objof(oldsuper));
}

static int
f_scope()
{
    struct_t    *s;

    s = structof(ici_vs.a_top[-1]);
    if (NARGS() > 0)
    {
        if (ici_typecheck("d", &ici_vs.a_top[-1]))
            return 1;
    }
    return ici_ret_no_decref(objof(s));
}

static int
f_isatom()
{
    object_t    *o;

    if (ici_typecheck("o", &o))
        return 1;
    if (o->o_flags & O_ATOM)
        return ici_ret_no_decref(objof(o_one));
    else
        return ici_ret_no_decref(objof(o_zero));
}

static int
f_alloc()
{
    long        length;
    int         accessz;
    char        *p;

    if (ici_typecheck("i*", &length))
        return 1;
    if (length < 0)
    {
        ici_error = "attempt to allocate negative amount";
        return 1;
    }
    if (NARGS() >= 2)
    {
        if
        (
            !isint(ARG(1))
            ||
            (
                (accessz = (int)intof(ARG(1))->i_value) != 1
                &&
                accessz != 2
                &&
                accessz != 4
            )
        )
            return ici_argerror(1);
    }
    else
        accessz = 1;
    if ((p = ici_alloc((size_t)length * accessz)) == NULL)
        return 1;
    memset(p, 0, (size_t)length * accessz);
    return ici_ret_with_decref(objof(new_mem(p, (unsigned long)length, accessz, ici_free)));
}

static int
f_mem()
{
    long        base;
    long        length;
    int         accessz;

    if (ici_typecheck("ii*", &base, &length))
        return 1;
    if (NARGS() >= 3)
    {
        if
        (
            !isint(ARG(2))
            ||
            (
                (accessz = (int)intof(ARG(2))->i_value) != 1
                &&
                accessz != 2
                &&
                accessz != 4
            )
        )
            return ici_argerror(2);
    }
    else
        accessz = 1;
    return ici_ret_with_decref(objof(new_mem((char *)base, (unsigned long)length, accessz, NULL)));
}

static int
f_assign()
{
    object_t    *s;
    object_t    *k;
    object_t    *v;

    switch (NARGS())
    {
    case 2:
        if (ici_typecheck("oo", &s, &k))
            return 1;
        v = objof(&o_null);
        break;

    case 3:
        if (ici_typecheck("ooo", &s, &k, &v))
            return 1;
        break;

    default:
        return ici_argcount(2);
    }
    if (hassuper(s))
    {
        if (assign_base(s, k, v))
            return 1;
    }
    else
    {
        if (assign(s, k, v))
            return 1;
    }
    return ici_ret_no_decref(v);
}

static int
f_fetch()
{
    struct_t    *s;
    object_t    *k;

    if (ici_typecheck("oo", &s, &k))
        return 1;
    if (hassuper(s))
        return ici_ret_no_decref(fetch_base(s, k));
    return ici_ret_no_decref(fetch(s, k));
}

#ifndef NOWAITFOR
static int
f_waitfor()
{
    register object_t   **e;
    int                 nargs;
    fd_set              readfds;
    struct timeval      timeval;
    struct timeval      *tv;
    double              to;
    int                 nfds;
    int                 i;
#ifndef fileno
    extern int          fileno();
#endif

    if (NARGS() == 0)
        return ici_ret_no_decref(objof(o_zero));
    tv = NULL;
    nfds = 0;
    FD_ZERO(&readfds);
    to = 0.0; /* Stops warnings, not required. */
    for (nargs = NARGS(), e = ARGS(); nargs > 0; --nargs, --e)
    {
        if (isfile(*e))
        {
            /*
             * If the ft_getch routine of the file is the real stdio fgetc,
             * we can assume the file is a real stdio stream file, then
             * we also assume we can use fileno on it.
             */
            if (fileof(*e)->f_type->ft_getch == fgetc)
            {
                setvbuf((FILE *)fileof(*e)->f_file, NULL, _IONBF, 0);
                i = fileno((FILE *)fileof(*e)->f_file);
                FD_SET(i, &readfds);
                if (i >= nfds)
                    nfds = i + 1;
            }
            else
                return ici_ret_no_decref(*e);
        }
        else if (isint(*e))
        {
            if (tv == NULL || to > intof(*e)->i_value / 1000.0)
            {
                to = intof(*e)->i_value / 1000.0;
                tv = &timeval;
            }
        }
        else if (isfloat(*e))
        {
            if (tv == NULL || to > floatof(*e)->f_value)
            {
                to = floatof(*e)->f_value;
                tv = &timeval;
            }
        }
        else
            return ici_argerror(ARGS() - e);
    }
    if (tv != NULL)
    {
        tv->tv_sec = to;
        tv->tv_usec = (to - tv->tv_sec) * 1000000.0;
    }
    ici_signals_blocking_syscall(1);
    switch (select(nfds, &readfds, NULL, NULL, tv))
    {
    case -1:
    ici_signals_blocking_syscall(0);
        ici_error = "could not select";
        return 1;

    case 0:
    ici_signals_blocking_syscall(0);
        return ici_ret_no_decref(objof(o_zero));
    }
    ici_signals_blocking_syscall(0);
    for (nargs = NARGS(), e = ARGS(); nargs > 0; --nargs, --e)
    {
        if (!isfile(*e))
            continue;
        if (fileof(*e)->f_type->ft_getch == fgetc)
        {
            i = fileno((FILE *)fileof(*e)->f_file);
            if (FD_ISSET(i, &readfds))
                return ici_ret_no_decref(*e);
        }
    }
    ici_error = "no file selected";
    return 1;
}
#endif /* ndef NOWAITFOR */

static int
f_gettoken()
{
    file_t              *f;
    string_t            *s;
    unsigned char       *seps;
    int                 nseps;
    char                *file;
    int                 (*get)();
    int                 c;
    int                 i;
    int                 j;

    seps = (unsigned char *) " \t\n";
    nseps = 3;
    switch (NARGS())
    {
    case 0:
        if ((f = need_stdin()) == NULL)
            return 1;
        break;

    case 1:
        if (ici_typecheck("o", &f))
            return 1;
        if (isstring(objof(f)))
        {
            if ((f = ici_sopen(stringof(f)->s_chars, stringof(f)->s_nchars)) == NULL)
                return 1;
            decref(f);
        }
        else if (!isfile(objof(f)))
            return ici_argerror(0);
        break;

    default:
        if (ici_typecheck("oo", &f, &s))
            return 1;
        if (isstring(objof(f)))
        {
            if ((f = ici_sopen(stringof(f)->s_chars, stringof(f)->s_nchars)) == NULL)
                return 1;
            decref(f);
        }
        else if (!isfile(objof(f)))
            return ici_argerror(0);
        if (!isstring(objof(s)))
            return ici_argerror(1);
        seps = (unsigned char *)s->s_chars;
        nseps = s->s_nchars;
        break;
    }
    get = f->f_type->ft_getch;
    file = f->f_file;
    do
    {
        c = (*get)(file);
        if (c == EOF)
            return null_ret();
        for (i = 0; i < nseps; ++i)
        {
            if (c == seps[i])
                break;
        }

    } while (i != nseps);

    j = 0;
    do
    {
        ici_chkbuf(j);
        buf[j++] = c;
        c = (*get)(file);
        if (c == EOF)
            break;
        for (i = 0; i < nseps; ++i)
        {
            if (c == seps[i])
            {
                (*f->f_type->ft_ungetch)(c, file);
                break;
            }
        }

    } while (i == nseps);

    if ((s = new_name(buf, j)) == NULL)
        return 1;
    return ici_ret_with_decref(objof(s));
}

/*
 * Fast (relatively) version for gettokens() if argument is not file
 */
static array_t *
fast_gettokens(char *str, char *delims)
{
    array_t     *a;
    int         k       = 0;
    char        *cp     = str;

    if ((a = new_array(0)) == NULL)
        return NULL;
    while (*cp)
    {
        while (*cp && strchr(delims, *cp))
            cp++;
        if ((k = strcspn(cp, delims)))
        {
            if
            (
                ici_stk_push_chk(a, 1)
                ||
                (*a->a_top = objof(new_name(cp, k))) == NULL
            )
            {
                decref(a);
                return NULL;
            }
            ++a->a_top;
            if (*(cp += k))
                cp++;
            continue;
        }
    }
    for (k = a->a_top - a->a_base - 1; k >= 0; k--)
        decref(a->a_base[k]);
    return(a);
}

static int
f_gettokens()
{
    file_t              *f;
    string_t            *s;
    unsigned char       *terms;
    int                 nterms;
    unsigned char       *seps;
    int                 nseps;
    unsigned char       *delims = NULL; /* init to shut up compiler */
    int                 ndelims;
    int                 hardsep;
    unsigned char       sep;
    char                *file;
    array_t             *a;
    int                 (*get)();
    int                 c;
    int                 i;
    int                 j = 0; /* init to shut up compiler */
    int                 state;
    int                 what;
    int                 loose_it = 0;

    seps = (unsigned char *)" \t";
    nseps = 2;
    hardsep = 0;
    terms = (unsigned char *)"\n";
    nterms = 1;
    ndelims = 0;
    switch (NARGS())
    {
    case 0:
        if ((f = need_stdin()) == NULL)
            return 1;
        break;

    case 1:
        if (ici_typecheck("o", &f))
            return 1;
        if (isstring(objof(f)))
        {
            array_t     *a;
            a = fast_gettokens(stringof(f)->s_chars, " \t");
            return a ? ici_ret_with_decref(objof(a)) : 1;
        }
        else if (!isfile(objof(f)))
            return ici_argerror(0);
        break;

    case 2:
    case 3:
    case 4:
        if (ici_typecheck("oo*", &f, &s))
            return 1;
        if (NARGS() == 2 && isstring(objof(f)) && isstring(objof(s)))
        {
            array_t     *a;
            a = fast_gettokens(stringof(f)->s_chars, stringof(s)->s_chars);
            return a ? ici_ret_with_decref(objof(a)) : 1;
        }
        if (isstring(objof(f)))
        {
            if ((f = ici_sopen(stringof(f)->s_chars, stringof(f)->s_nchars)) == NULL)
                return 1;
            loose_it = 1;
        }
        else if (!isfile(objof(f)))
            return ici_argerror(0);
        if (isint(objof(s)))
        {
            sep = (unsigned char)intof(objof(s))->i_value;
            hardsep = 1;
            seps = (unsigned char *)&sep;
            nseps = 1;
        }
        else if (isstring(objof(s)))
        {
            seps = (unsigned char *)s->s_chars;
            nseps = s->s_nchars;
        }
        else
        {
            if (loose_it)
                decref(f);
            return ici_argerror(1);
        }
        if (NARGS() > 2)
        {
            if (!isstring(ARG(2)))
            {
                if (loose_it)
                    decref(f);
                return ici_argerror(2);
            }
            terms = (unsigned char *)stringof(ARG(2))->s_chars;
            nterms = stringof(ARG(2))->s_nchars;
            if (NARGS() > 3)
            {
                if (!isstring(ARG(3)))
                {
                    if (loose_it)
                        decref(f);
                    return ici_argerror(3);
                }
                delims = (unsigned char *)stringof(ARG(3))->s_chars;
                ndelims = stringof(ARG(3))->s_nchars;
            }
        }
        break;

    default:
        return ici_argcount(3);
    }
    get = f->f_type->ft_getch;
    file = f->f_file;

#define S_IDLE  0
#define S_INTOK 1

#define W_EOF   0
#define W_SEP   1
#define W_TERM  2
#define W_TOK   3
#define W_DELIM 4

    state = S_IDLE;
    if ((a = new_array(0)) == NULL)
        goto fail;
    for (;;)
    {
        /*
         * Get the next character and classify it.
         */
        if ((c = (*get)(file)) == EOF)
        {
            what = W_EOF;
            goto got_what;
        }
        for (i = 0; i < nseps; ++i)
        {
            if (c == seps[i])
            {
                what = W_SEP;
                goto got_what;
            }
        }
        for (i = 0; i < nterms; ++i)
        {
            if (c == terms[i])
            {
                what = W_TERM;
                goto got_what;
            }
        }
        for (i = 0; i < ndelims; ++i)
        {
            if (c == delims[i])
            {
                what = W_DELIM;
                goto got_what;
            }
        }
        what = W_TOK;
    got_what:

        /*
         * Act on state and current character classification.
         */
        switch ((state << 8) + what)
        {
        case (S_IDLE << 8) + W_EOF:
            if (loose_it)
                decref(f);
            if (a->a_top == a->a_base)
            {
                decref(a);
                return null_ret();
            }
            return ici_ret_with_decref(objof(a));

        case (S_IDLE << 8) + W_TERM:
            if (!hardsep)
            {
                if (loose_it)
                    decref(f);
                return ici_ret_with_decref(objof(a));
            }
            j = 0;
        case (S_INTOK << 8) + W_EOF:
        case (S_INTOK << 8) + W_TERM:
            if (ici_stk_push_chk(a, 1))
                goto fail;
            if ((s = new_name(buf, j)) == NULL)
                goto fail;
            *a->a_top++ = objof(s);
            if (loose_it)
                decref(f);
            decref(s);
            return ici_ret_with_decref(objof(a));

        case (S_IDLE << 8) + W_SEP:
            if (!hardsep)
                break;
            j = 0;
        case (S_INTOK << 8) + W_SEP:
            if (ici_stk_push_chk(a, 1))
                goto fail;
            if ((s = new_name(buf, j)) == NULL)
                goto fail;
            *a->a_top++ = objof(s);
            decref(s);
            if (hardsep)
            {
                j = 0;
                state = S_INTOK;
            }
            else
                state = S_IDLE;
            break;

        case (S_INTOK << 8) + W_DELIM:
            if (ici_stk_push_chk(a, 1))
                goto fail;
            if ((s = new_name(buf, j)) == NULL)
                goto fail;
            *a->a_top++ = objof(s);
            decref(s);
        case (S_IDLE << 8) + W_DELIM:
            if (ici_stk_push_chk(a, 1))
                goto fail;
            buf[0] = c;
            if ((s = new_name(buf, 1)) == NULL)
                goto fail;
            *a->a_top++ = objof(s);
            decref(s);
            j = 0;
            state = S_IDLE;
            break;

        case (S_IDLE << 8) + W_TOK:
            j = 0;
            state = S_INTOK;
        case (S_INTOK << 8) + W_TOK:
            if (ici_chkbuf(j))
                goto fail;
            buf[j++] = c;
        }
    }

fail:
    if (loose_it)
        decref(f);
    if (a != NULL)
        decref(a);
    return 1;
}

/*
 * sort(array, cmp)
 */
static int
f_sort()
{
    array_t     *a;
    object_t    **base;
    long        n;
    object_t    *f;
    long        cmp;
    long        k;                              /* element added or removed */
    long        p;                              /* place in heap */
    long        q;                              /* place in heap */
    long        l;                              /* left child */
    long        r;                              /* right child */
    object_t    *o;                             /* object used for swapping */
    object_t    *uarg;                          /* user argument to cmp func */

/*
 * Relations within heap.
 */
#define PARENT(i)       (((i) - 1) >> 1)
#define LEFT(i)         ((i) + (i) + 1)
#define RIGHT(i)        ((i) + (i) + 2)
/*
 * Macro for swapping elements.
 */
#define SWAP(a, b)      {o = base[a]; base[a] = base[b]; base[b] = o;}
#define CMP(rp, a, b)   ici_func(f, "i=ooo", rp, base[a], base[b], uarg)

    uarg = objof(&o_null);
    switch (NARGS())
    {
    case 3:
        if (ici_typecheck("aoo", &a, &f, &uarg))
            return 1;
        if (ici_typeof(f)->t_call == NULL)
            return ici_argerror(1);
        break;

    case 2:
        if (ici_typecheck("ao", &a, &f))
            return 1;
        if (ici_typeof(f)->t_call == NULL)
            return ici_argerror(1);
        break;

    case 1:
        if (ici_typecheck("a", &a))
            return 1;
        if (sort_cmp_func == NULL)
        {
            static char         *code =
                "auto _(a, b)"
                "{"
                " return a < b ? -1 : a > b;"
                "}";
            file_t              *file;

            if ((file = ici_sopen(code, strlen(code))) == NULL)
                return 1;
            file->f_name = SS(empty_string);
            if (parse_module(file, objwsupof(ici_vs.a_top[-1])) < 0)
            {
                decref(file);
                return 1;
            }
            decref(file);
            sort_cmp_func = funcof(fetch(structof(ici_vs.a_top[-1]), SS(_)));
            if (objof(sort_cmp_func) == objof(&o_null))
            {
                ici_error = "unable to define default sort compare function";
                return 1;
            }
            incref(sort_cmp_func);
        }
        f = objof(sort_cmp_func);
        break;

    default:
        return ici_argcount(2);
    }
    if (objof(a)->o_flags & O_ATOM)
    {
        ici_error = "attempt to sort an atomic array";
        return 1;
    }

    n = ici_array_nels(a);
    if (a->a_bot > a->a_top)
    {
        ptrdiff_t       m;
        object_t        **e;

        /*
         * Can't sort in-place because the array has wrapped. Force the
         * array to be contiguous. ### Maybe this should be a function
         * in array.c.
         */
        m = a->a_limit - a->a_base;
        if ((e = ici_nalloc(m * sizeof(object_t *))) == NULL)
            goto fail;
        ici_array_gather(e, a, 0, n);
        ici_nfree(a->a_base, m * sizeof(object_t *));
        a->a_base = e;
        a->a_bot = e;
        a->a_top = e + n;
        a->a_limit = e + m;
    }
    base = a->a_bot;

    /*
     * Shuffle heap.
     */
    for (k = 1; k < n; ++k)
    {
        p = k;
        while (p != 0)
        {
            q = PARENT(p);
            if (CMP(&cmp, p, q) != NULL)
                goto fail;
            if (cmp <= 0)
                break;
            SWAP(p, q);
            p = q;
        }
    }

    /*
     * Keep taking elements off heap and re-shuffling.
     */
    for (k = n - 1; k > 0; --k)
    {
        SWAP(0, k);
        p = 0;
        while (1)
        {
            l = LEFT(p);
            if (l >= k)
                break;
            r = RIGHT(p);
            if (r >= k)
            {
                if (CMP(&cmp, l, p) != NULL)
                    goto fail;
                if (cmp <= 0)
                    break;
                SWAP(l, p);
                p = l;
            }
            else
            {
                if (CMP(&cmp, l, p) != NULL)
                    goto fail;
                if (cmp <= 0)
                {
                    if (CMP(&cmp, r, p) != NULL)
                        goto fail;
                    if (cmp <= 0)
                        break;
                    SWAP(r, p);
                    p = r;
                }
                else
                {
                    if (CMP(&cmp, r, l) != NULL)
                        goto fail;
                    if (cmp <= 0)
                    {
                        SWAP(l, p);
                        p = l;
                    }
                    else
                    {
                        SWAP(r, p);
                        p = r;
                    }
                }
            }
        }
    }
    return ici_ret_no_decref(objof(a));

fail:
    return 1;

#undef  PARENT
#undef  LEFT
#undef  RIGHT
#undef  SWAP
}

#ifdef WHOALLOC
static int
f_reclaim()
{
    ici_reclaim();
    return null_ret();
}
#endif

static int
f_abs()
{
    if (isint(ARG(0)))
    {
        if (intof(ARG(0))->i_value >= 0)
            return ici_ret_no_decref(ARG(0));
        return int_ret(-intof(ARG(0))->i_value);
    }
    else if (isfloat(ARG(0)))
    {
        if (floatof(ARG(0))->f_value >= 0)
            return ici_ret_no_decref(ARG(0));
        return float_ret(-floatof(ARG(0))->f_value);
    }
    return ici_argerror(0);
}

static int              got_epoch_time;
static time_t           epoch_time;

static void
get_epoch_time()
{
    struct tm   tm;

    memset(&tm, 0, sizeof tm);
    tm.tm_year = 100; /* 2000 is 100 years since 1900. */
    tm.tm_mday = 1;    /* First day of month is 1 */
    epoch_time = mktime(&tm);
    got_epoch_time = 1;
}

static int
f_now()
{
    if (!got_epoch_time)
        get_epoch_time();
    return float_ret(difftime(time(NULL), epoch_time));
}

static int
f_calendar()
{
    objwsup_t           *s;
    double              d;
    long                l;

    s = NULL;
    if (!got_epoch_time)
        get_epoch_time();
    if (NARGS() != 1)
        return ici_argcount(1);
    if (isfloat(ARG(0)))
    {
        time_t          t;
        struct tm       *tm;

        /*
         * This is really bizarre. ANSI C doesn't define what the units
         * of a time_t are, just that it is an arithmetic type. difftime()
         * can be used to get seconds from time_t values, but their is no
         * inverse operation. So without discovering a conversion factor
         * from calls to mktime(), one can't usefully combine a time in,
         * say seconds, with a time_t. But we all know that time_t is
         * really in seconds. So I'll just assume that.
         */
        t = epoch_time + (time_t)floatof(ARG(0))->f_value;
        tm = localtime(&t);
        if ((s = objwsupof(new_struct())) == NULL)
            return 1;
        if
        (
               ici_set_val(s, SS(second), 'f', (d = tm->tm_sec, &d))
            || ici_set_val(s, SS(minute), 'i', (l = tm->tm_min, &l))
            || ici_set_val(s, SS(hour), 'i', (l = tm->tm_hour, &l))
            || ici_set_val(s, SS(day), 'i', (l = tm->tm_mday, &l))
            || ici_set_val(s, SS(month), 'i', (l = tm->tm_mon, &l))
            || ici_set_val(s, SS(year), 'i', (l = tm->tm_year + 1900, &l))
            || ici_set_val(s, SS(wday), 'i', (l = tm->tm_wday, &l))
            || ici_set_val(s, SS(yday), 'i', (l = tm->tm_yday, &l))
            || ici_set_val(s, SS(isdst), 'i', (l = tm->tm_isdst, &l))
        )
        {
            decref(s);
            return 1;
        }
        return ici_ret_with_decref(objof(s));
    }
    else if (isstruct(ARG(0)))
    {
        time_t          t;
        struct tm       tm;

        memset(&tm, 0, sizeof tm);
        s = objwsupof(ARG(0));
        if (ici_fetch_num(objof(s), SSO(second), &d))
            return 1;
        tm.tm_sec = (int)d;
        if (ici_fetch_int(objof(s), SSO(minute), &l))
            return 1;
        tm.tm_min = l;
        if (ici_fetch_int(objof(s), SSO(hour), &l))
            return 1;
        tm.tm_hour = l;
        if (ici_fetch_int(objof(s), SSO(day), &l))
            return 1;
        tm.tm_mday = l;
        if (ici_fetch_int(objof(s), SSO(month), &l))
            return 1;
        tm.tm_mon = l;
        if (ici_fetch_int(objof(s), SSO(year), &l))
            return 1;
        tm.tm_year = l - 1900;
        if (ici_fetch_int(objof(s), SSO(isdst), &l))
            tm.tm_isdst = -1;
        else
            tm.tm_isdst = l;
        t = mktime(&tm);
        if (t == (time_t)-1)
        {
            ici_error = "unsuitable calendar time";
            return 1;
        }
        return float_ret(difftime(t, epoch_time));
    }
    return ici_argerror(0);
}

/*
 * ICI: alarm(when)
 */
static int
f_sleep()
{
    double              how_long;
    exec_t              *x;

    if (ici_typecheck("n", &how_long))
        return 1;

#ifdef _WIN32
    {
        long            t;

        how_long *= 1000; /* Convert to milliseconds. */
        if (how_long > LONG_MAX)
            t = LONG_MAX;
        else if ((t = how_long) < 1)
            t = 1;
        x = ici_leave();
        Sleep(t);
        ici_enter(x);
    }
#else
    {
        long            t;

        if (how_long > LONG_MAX)
            t = LONG_MAX;
        else if ((t = how_long) < 1)
            t = 1;
        x = ici_leave();
        sleep(t);
        ici_enter(x);
    }
#endif
    return null_ret();
}

/*
 * Return the accumulated cpu time in seconds as a float. The precision
 * is system dependent. If a float argument is provided, this forms a new
 * base cpu time from which future cputime values are relative to. Thus
 * if 0.0 is passed, future (but not this one) returns measure new CPU
 * time accumulated since this call.
 */
static int
f_cputime()
{
    static double       base;
    double              t;

#ifdef  _WIN32
    FILETIME            c;
    FILETIME            e;
    FILETIME            k;
    FILETIME            user;

    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &user))
        return ici_get_last_win32_error();
    t = (user.dwLowDateTime + user.dwHighDateTime * 4294967296.0) / 1e7;

#else /* _WIN32 */
    ici_error = "cputime function not available on this platform";
    return 1;
#endif
    t -= base;
    if (NARGS() > 0 && isfloat(ARG(0)))
        base = floatof(ARG(0))->f_value + t;
    return float_ret(t);
}

static int
f_version()
{
    static string_t     *ver;

    if (ver == NULL && (ver = new_cname(ici_version_string)) == NULL)
        return 1;
    return ici_ret_no_decref(objof(ver));
}

cfunc_t std_cfuncs[] =
{
    {CF_OBJ,    "array",        f_array},
    {CF_OBJ,    "copy",         f_copy},
    {CF_OBJ,    "exit",         f_exit},
    {CF_OBJ,    "fail",         f_fail},
    {CF_OBJ,    "float",        f_float},
    {CF_OBJ,    "int",          f_int},
    {CF_OBJ,    "eq",           f_eq},
    {CF_OBJ,    "parse",        f_parse},
    {CF_OBJ,    "string",       f_string},
    {CF_OBJ,    "struct",       f_struct},
    {CF_OBJ,    "set",          f_set},
    {CF_OBJ,    "typeof",       f_typeof},
    {CF_OBJ,    "push",         f_push},
    {CF_OBJ,    "pop",          f_pop},
    {CF_OBJ,    "rpush",        f_rpush},
    {CF_OBJ,    "rpop",         f_rpop},
    {CF_OBJ,    "call",         f_call},
    {CF_OBJ,    "keys",         f_keys},
    {CF_OBJ,    "vstack",       f_vstack},
    {CF_OBJ,    "tochar",       f_tochar},
    {CF_OBJ,    "toint",        f_toint},
    {CF_OBJ,    "rand",         f_rand},
    {CF_OBJ,    "interval",     f_interval},
    {CF_OBJ,    "explode",      f_explode},
    {CF_OBJ,    "implode",      f_implode},
    {CF_OBJ,    "sopen",        f_sopen},
    {CF_OBJ,    "mopen",        f_mopen},
    {CF_OBJ,    "sprintf",      f_sprintf},
    {CF_OBJ,    "currentfile",  f_currentfile},
    {CF_OBJ,    "del",          f_del},
    {CF_OBJ,    "alloc",        f_alloc},
    {CF_OBJ,    "mem",          f_mem},
    {CF_OBJ,    "nels",         f_nels},
    {CF_OBJ,    "super",        f_super},
    {CF_OBJ,    "scope",        f_scope},
    {CF_OBJ,    "isatom",       f_isatom},
    {CF_OBJ,    "gettoken",     f_gettoken},
    {CF_OBJ,    "gettokens",    f_gettokens},
    {CF_OBJ,    "num",          f_num},
    {CF_OBJ,    "assign",       f_assign},
    {CF_OBJ,    "fetch",        f_fetch},
    {CF_OBJ,    "abs",          f_abs},
#ifndef NOMATH
    {CF_OBJ,    "sin",          f_math, (int (*)())sin,         "f=n"},
    {CF_OBJ,    "cos",          f_math, (int (*)())cos,         "f=n"},
    {CF_OBJ,    "tan",          f_math, (int (*)())tan,         "f=n"},
    {CF_OBJ,    "asin",         f_math, (int (*)())asin,        "f=n"},
    {CF_OBJ,    "acos",         f_math, (int (*)())acos,        "f=n"},
    {CF_OBJ,    "atan",         f_math, (int (*)())atan,        "f=n"},
    {CF_OBJ,    "atan2",        f_math, (int (*)())atan2,       "f=nn"},
    {CF_OBJ,    "exp",          f_math, (int (*)())exp,         "f=n"},
    {CF_OBJ,    "log",          f_math, (int (*)())log,         "f=n"},
    {CF_OBJ,    "log10",        f_math, (int (*)())log10,       "f=n"},
    {CF_OBJ,    "pow",          f_math, (int (*)())pow,         "f=nn"},
    {CF_OBJ,    "sqrt",         f_math, (int (*)())sqrt,        "f=n"},
    {CF_OBJ,    "floor",        f_math, (int (*)())floor,       "f=n"},
    {CF_OBJ,    "ceil",         f_math, (int (*)())ceil,        "f=n"},
    {CF_OBJ,    "fmod",         f_math, (int (*)())fmod,        "f=nn"},
#endif
#ifndef NOWAITFOR
    {CF_OBJ,    "waitfor",      f_waitfor},
#endif
    {CF_OBJ,    "top",          f_top},
    {CF_OBJ,    "include",      f_include},
    {CF_OBJ,    "sort",         f_sort},
#ifdef  WHOALLOC
    {CF_OBJ,    "reclaim",      f_reclaim},
#endif
    {CF_OBJ,    "now",          f_now},
    {CF_OBJ,    "calendar",     f_calendar},
    {CF_OBJ,    "version",      f_version},
    {CF_OBJ,    "cputime",      f_cputime},
    {CF_OBJ,    "sleep",        f_sleep},
    {CF_OBJ}
};
