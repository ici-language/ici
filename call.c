#define ICI_CORE
#include "buf.h"
#include "exec.h"
#include "func.h"
#include "int.h"
#include "float.h"
#include "str.h"
#include "null.h"
#include "op.h"
#include "catch.h"

/*
 * The common code used by ici_func() and ici_call() below. Also a
 * varient of ici_func() taking a variable argument list. See comment
 * on ici_func() below for details.
 */
char *
ici_funcv(object_t *func_obj, char *types, va_list va)
{
    int                 nargs;
    int                 arg;
    object_t            *member_obj;
    object_t            *ret_obj;
    char                ret_type;
    char                *ret_ptr;
    ptrdiff_t           os_depth;

    if (types[0] != '\0' && types[1] == '@')
    {
        member_obj = va_arg(va, object_t *);
        types += 2;
    }
    else
    {
        member_obj = NULL;
    }

    if (types[0] != '\0' && types[1] == '=')
    {
        ret_type = types[0];
        ret_ptr = va_arg(va, char *);
        types += 2;
    }
    else
    {
        ret_type = '\0';
        ret_ptr = NULL;
    }

    os_depth = ici_os.a_top - ici_os.a_base;
    /*
     * We include an extra 80 in our ici_stk_push_chk, see start of evaluate().
     */
    nargs = strlen(types);
    if (ici_stk_push_chk(&ici_os, nargs + 80))
        return ici_error;
    for (arg = 0; arg < nargs; ++arg)
        *ici_os.a_top++ = objof(&o_null);
    for (arg = -1; arg >= -nargs; --arg)
    {
        switch (*types++)
        {
        case 'o':
            ici_os.a_top[arg] = va_arg(va, object_t *);
            break;

        case 'i':
            if ((ici_os.a_top[arg] = objof(ici_int_new(va_arg(va, long)))) == NULL)
                goto fail;
            ici_decref(ici_os.a_top[arg]);
            break;

        case 'q':
            ici_os.a_top[arg] = objof(&o_quote);
            --nargs;
            break;

        case 's':
            if ((ici_os.a_top[arg] = objof(ici_str_new_nul_term(va_arg(va, char *)))) == NULL)
                goto fail;
            ici_decref(ici_os.a_top[arg]);
            break;

        case 'f':
            if ((ici_os.a_top[arg] = objof(ici_float_new(va_arg(va, double)))) == NULL)
                goto fail;
            ici_decref(ici_os.a_top[arg]);
            break;

        default:
            ici_error = "error in function call";
            goto fail;
        }
    }
    if (member_obj != NULL)
    {
        *ici_os.a_top++ = member_obj;
        nargs++;
    }
    /*
     * Push the number of actual args, followed by the function
     * itself onto the operand stack.
     */
    if ((*ici_os.a_top = objof(ici_int_new(nargs))) == NULL)
        goto fail;
    ici_decref(*ici_os.a_top);
    ++ici_os.a_top;
    *ici_os.a_top++ = func_obj;

    os_depth = (ici_os.a_top - ici_os.a_base) - os_depth;
    if ((ret_obj = ici_evaluate(objof(&o_call), os_depth)) == NULL)
        goto fail;

    switch (ret_type)
    {
    case '\0':
        ici_decref(ret_obj);
        break;

    case 'o':
        *(object_t **)ret_ptr = ret_obj;
        break;

    case 'i':
        if (!isint(ret_obj))
            goto typeclash;
        *(long *)ret_ptr = intof(ret_obj)->i_value;
        ici_decref(ret_obj);
        break;

    case 'f':
        if (!isfloat(ret_obj))
            goto typeclash;
        *(double *)ret_ptr = floatof(ret_obj)->f_value;
        ici_decref(ret_obj);
        break;

    case 's':
        if (!isstring(ret_obj))
            goto typeclash;
        *(char **)ret_ptr = stringof(ret_obj)->s_chars;
        ici_decref(ret_obj);
        break;

    default:
    typeclash:
        ici_decref(ret_obj);
        ici_error = "incorrect return type";
        goto fail;
    }
    return NULL;

fail:
    return ici_error;
}

/*
 * Varient of ici_call() (see below) taking a variable argument list.
 */
char *
ici_callv(char *func_name, char *types, va_list va)
{
    object_t            *name_obj;
    object_t            *func_obj;
    object_t            *member_obj;
    char                *result;

    name_obj = NULL;
    func_obj = NULL;

    if ((name_obj = objof(ici_str_new_nul_term(func_name))) == NULL)
        return ici_error;
    if (types[0] != '\0' && types[1] == '@')
    {
        va_list tmp;
        tmp = va;
        member_obj = va_arg(tmp, object_t *);
        if ((func_obj = ici_fetch(member_obj, name_obj)) == objof(&o_null))
        {
            sprintf(buf, "\"%s\" undefined in object", func_name);
            ici_error = buf;
            ici_decref(name_obj);
            return ici_error;
        }
        va_end(tmp);
    }
    else if ((func_obj = ici_fetch(ici_vs.a_top[-1], name_obj)) == objof(&o_null))
    {
        sprintf(buf, "\"%s\" undefined", func_name);
        ici_error = buf;
        ici_decref(name_obj);
        return ici_error;
    }
    ici_decref(name_obj);
    name_obj = NULL;
    result = ici_funcv(func_obj, types, va);
    return result;
}

/*
 * ici_func(func, types, args...)
 *
 * Call an ICI function from C with simple argument types and return value.
 *
 * Types can be of the forms ".=..." or "...".  In the first case the 1st
 * extra arg is used as a pointer to store the return value through.
 *
 * Type key letters are:
 *      i       a long
 *      f       a double
 *      s       a '\0' terminated string
 *      o       an ici object
 *
 * When a string is returned it is a pointer to the character data of an
 * internal ICI string object. It will only remain valid until the next
 * call to any ICI function.  When an object is returned it has been ici_incref'ed
 * (i.e. it is held against garbage collection).
 */
char *
ici_func(object_t *func_obj, char *types, ...)
{
    va_list     va;
    char        *result;

    va_start(va, types);
    result = ici_funcv(func_obj, types, va);
    va_end(va);
    return result;
}

/*
 * ici_call(name, types, args...)
 *
 * Call an ici function by name from C with simple argument types and
 * return value.  The named is looked up in the current scope or in
 * a supplied struct.
 *
 * Types can be of the forms ".=...", ".@.=...", ".@..." or "...".
 * In the first case the 1st extra arg is used as a pointer to store the
 * return value through. In the second and third cases the first extra
 * arg is a struct from which the function is fetched; this object
 * will be passed as the first parameter to the function. In the second case
 * the second extra arg will be used for the return value.
 *
 * Type key letters are:
 *      i       a long
 *      f       a double
 *      s       a '\0' terminated string
 *      o       an ici object
 *
 * When a string is returned it is a pointer to the character data of an
 * internal ICI string object. It will only remain valid until the next
 * call to any ICI function.  When an object is returned it has been ici_incref'ed
 * (i.e. it is held against garbage collection).
 */
char *
ici_call(char *func_name, char *types, ...)
{
    object_t            *name_obj;
    object_t            *func_obj;
    object_t            *member_obj;
    va_list             va;
    char                *result;

    name_obj = NULL;
    func_obj = NULL;

    if ((name_obj = objof(ici_str_new_nul_term(func_name))) == NULL)
        return ici_error;
    if (types[0] != '\0' && types[1] == '@')
    {
        va_start(va, types);
        member_obj = va_arg(va, object_t *);
        if ((func_obj = ici_fetch(member_obj, name_obj)) == objof(&o_null))
        {
            sprintf(buf, "\"%s\" undefined in object", func_name);
            ici_error = buf;
            ici_decref(name_obj);
            return ici_error;
        }
    }
    else if ((func_obj = ici_fetch(ici_vs.a_top[-1], name_obj)) == objof(&o_null))
    {
        sprintf(buf, "\"%s\" undefined", func_name);
        ici_error = buf;
        ici_decref(name_obj);
        return ici_error;
    }
    va_start(va, types);
    ici_decref(name_obj);
    name_obj = NULL;
    result = ici_funcv(func_obj, types, va);
    va_end(va);
    return result;
}
