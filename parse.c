#define ICI_CORE
#include "parse.h"
#include "func.h"
#include "str.h"
#include "struct.h"
#include "buf.h"
#include "file.h"
#include "op.h"
#include "exec.h"
#include "pc.h"

/*
 * Some commonly used strings.
 */
static char     not_by[]        = "not followed by";
static char     an_expression[] = "an expression";

/*
 * A few forward definitions...
 */
static int      compound_statement(parse_t *, struct_t *);
static int      expr(parse_t *, expr_t **, int);
static int      const_expression(parse_t *, object_t **, int);
static int      statement(parse_t *, array_t *, struct_t *, char *, int);

/*
 * In general, parseing functions return -1 on error (and set the global
 * error string), 0 if they encountered an early head symbol conflict (and
 * the parse stream has not been disturbed), and 1 if they actually got
 * what they were looking for.
 */

/*
 * 'this' is a convenience macro. Routines in this file conventionally use
 * the variable name 'p' for the pointer the current parse structure. Given
 * that, 'this' is the last token fetched. That is, the current head symbol.
 */
#define this    p->p_got.t_what

/*
 * next(p, a) and reject(p) are the basic token fetching (and rejecting)
 * functions (or macros). See lex() for the meanins of the 'a'. 'p' is a
 * pointer to the subject parse sructure.
 */
#ifndef SMALL

#define next(p, a)  (p->p_ungot.t_what != T_NONE \
                    ? (p->p_got=p->p_ungot, p->p_ungot.t_what=T_NONE, this) \
                    : lex(p, a))

#define reject(p) (p->p_ungot = p->p_got)

#else

static int
next(parse_t *p, array_t *a)
{
    if (p->p_ungot.t_what != T_NONE)
    {
        p->p_got = p->p_ungot;
        p->p_ungot.t_what = T_NONE;
        return this;
    }
    return lex(p, a);
}

static void
reject(parse_t *p)
{
    p->p_ungot = p->p_got;
}
#endif

static int
not_followed_by(char *a, char *b)
{
    sprintf(buf, "\"%s\" %s %s", a, not_by, b);
    ici_error = buf;
    return -1;
}

/*
 * Returns a non-decref array of identifiers parsed from a comma seperated
 * list, or NULL on error.  The array may be empty.
 */
static array_t *
ident_list(parse_t *p)
{
    array_t     *a;

    if ((a = ici_array_new(0)) == NULL)
        return NULL;
    for (;;)
    {
        if (next(p, NULL) != T_NAME)
        {
            reject(p);
            return a;
        }
        if (ici_stk_push_chk(a, 1))
            goto fail;
        *a->a_top = p->p_got.t_obj;
        ici_decref(*a->a_top);
        ++a->a_top;
        if (next(p, NULL) != T_COMMA)
        {
            reject(p);
            return a;
        }
    }

fail:
    ici_decref(a);
    return NULL;
}

/*
 * The parse stream may be positioned just before the on-round of the
 * argument list of a function. Try to parse the function.
 *
 * Return -1, 0 or 1, usual conventions.  On success, returns a parsed
 * non-decref function in parse.p_got.t_obj.
 */
static int
function(parse_t *p, string_t *name)
{
    array_t     *a;
    func_t      *f;
    func_t      *saved_func;
    object_t    **fp;

    a = NULL;
    f = NULL;
    if (next(p, NULL) != T_ONROUND)
    {
        reject(p);
        return 0;
    }
    if ((a = ident_list(p)) == NULL)
        return -1;
    saved_func = p->p_func;
    if (next(p, NULL) != T_OFFROUND)
    {
        not_followed_by("ident ( [args]", "\")\"");
        goto fail;
    }
    if ((f = new_func()) == NULL)
        goto fail;
    if ((f->f_autos = ici_struct_new()) == NULL)
        goto fail;
    ici_decref(f->f_autos);
    if (ici_assign(f->f_autos, SS(_func_), objof(f)))
        goto fail;
    for (fp = a->a_base; fp < a->a_top; ++fp)
    {
        if (ici_assign(f->f_autos, *fp, &o_null))
            goto fail;
    }
    f->f_autos->o_head.o_super = objwsupof(ici_vs.a_top[-1])->o_super;
    p->p_func = f;
    f->f_args = a;
    ici_decref(a);
    a = NULL;
    f->f_name = name;
    if (ici_stk_push_chk(f->f_args, 1))
        goto fail;
    switch (compound_statement(p, NULL))
    {
    case 0: not_followed_by("ident ( [args] )", "\"{\"");
    case -1: goto fail;
    }
    f->f_code = arrayof(p->p_got.t_obj);
    ici_decref(f->f_code);
    if (f->f_code->a_top[-1] == objof(&o_end))
        --f->f_code->a_top;
    if (ici_stk_push_chk(f->f_code, 3))
        goto fail;
    *f->f_code->a_top++ = objof(&o_null);
    *f->f_code->a_top++ = objof(&o_return);
    *f->f_code->a_top++ = objof(&o_end);
    f->f_args = f->f_args;
    f->f_autos = structof(ici_atom(objof(f->f_autos), 1));
    p->p_got.t_obj = ici_atom(objof(f), 1);
    p->p_func = saved_func;
    return 1;

fail:
    if (a != NULL)
        ici_decref(a);
    if (f != NULL)
        ici_decref(f);
    p->p_func = saved_func;
    return -1;
}

/*
 * ows is the struct (or whatever) the idents are going into.
 */
static int
data_def(parse_t *p, objwsup_t *ows)
{
    object_t    *o;     /* The value it is initialised with. */
    object_t    *n;     /* The name. */
    int         wasfunc;
    int         hasinit;

    n = NULL;
    o = NULL;
    wasfunc = 0;
    /*
     * Work through the list of identifiers being declared.
     */
    for (;;)
    {
        if (next(p, NULL) != T_NAME)
        {
            ici_error = "syntax error in variable definition";
            goto fail;
        }
        n = p->p_got.t_obj;

        /*
         * Gather any initialisation or function.
         */
        hasinit = 0;
        switch (next(p, NULL))
        {
        case T_EQ:
            switch (const_expression(p, &o, T_COMMA))
            {
            case 0: not_followed_by("ident =", an_expression);
            case -1: goto fail;
            }
            hasinit = 1;
            break;

        case T_ONROUND:
            reject(p);
            if (function(p, stringof(n)) < 0)
                goto fail;
            o = p->p_got.t_obj;
            wasfunc = 1;
            hasinit = 1;
            break;

        default:
            o = objof(&o_null);
            ici_incref(o);
            reject(p);
        }

        /*
         * Assign to the new variable if it doesn't appear to exist
         * or has an explicit initialisation.
         */
        if (hasinit || fetch_base(ows, n) == objof(&o_null))
        {
            if (assign_base(ows, n, o))
                goto fail;
        }
        ici_decref(n);
        n = NULL;
        ici_decref(o);
        o = NULL;

        if (wasfunc)
            return 1;

        switch (next(p, NULL))
        {
        case T_COMMA: continue;
        case T_SEMICOLON: return 1;
        }
        ici_error = "variable definition not followed by \";\" or \",\"";
        goto fail;
    }

fail:
    if (n != NULL)
        ici_decref(n);
    if (o != NULL)
        ici_decref(o);
    return -1;
}

static int
compound_statement(parse_t *p, struct_t *sw)
{
    array_t     *a;

    a = NULL;
    if (next(p, NULL) != T_ONCURLY)
    {
        reject(p);
        return 0;
    }
    if ((a = ici_array_new(0)) == NULL)
        goto fail;
    for (;;)
    {
        switch (statement(p, a, sw, NULL, 0))
        {
        case -1: goto fail;
        case 1: continue;
        }
        break;
    }
    if (next(p, a) != T_OFFCURLY)
    {
        ici_error = "badly formed statement";
        goto fail;
    }
    if (ici_stk_push_chk(a, 1))
        goto fail;
    *a->a_top++ = objof(&o_end);
    p->p_got.t_obj = objof(a);
    return 1;

fail:
    if (a != NULL)
        ici_decref(a);
    return -1;
}

/*
 * Free an exprssesion tree and decref all the objects that it references.
 */
static void
free_expr(expr_t *e)
{
    expr_t      *e1;

    while (e != NULL)
    {
        if (e->e_arg[1] != NULL)
            free_expr(e->e_arg[1]);
        if (e->e_obj != NULL)
            ici_decref(e->e_obj);
        e1 = e;
        e = e->e_arg[0];
        ici_tfree(e1, expr_t);
    }
}

static int
bracketed_expr(parse_t *p, expr_t **ep)
{
    if (next(p, NULL) != T_ONROUND)
    {
        reject(p);
        return 0;
    }
    switch (expr(p, ep, T_NONE))
    {
    case 0: not_followed_by("(", an_expression);
    case -1: return -1;
    }
    if (next(p, NULL) != T_OFFROUND)
        return not_followed_by("( expr", "\")\"");
    return 1;
}

/*
 * Parse a primaryexpression in the parse context 'p' and store the expression
 * tree of 'expr_t' type nodes under the pointer indicated by 'ep'. Usual
 * parseing return conventions (see comment near start of file). See the
 * comment on expr() for the meaning of exclude.
 */
static int
primary(parse_t *p, expr_t **ep, int exclude)
{
    expr_t      *e;
    array_t     *a;
    struct_t    *d;
    set_t       *s;
    object_t    *n;
    object_t    *o;
    char        *token_name = 0;
    int         wasfunc;
    int         is_class;

    *ep = NULL;
    if ((e = ici_talloc(expr_t)) == NULL)
        return -1;
    e->e_arg[0] = NULL;
    e->e_arg[1] = NULL;
    e->e_obj = NULL;
    switch (next(p, NULL))
    {
    case T_INT:
        e->e_what = T_INT;
        if ((e->e_obj = objof(ici_int_new(p->p_got.t_int))) == NULL)
            goto fail;
        break;

    case T_FLOAT:
        e->e_what = T_FLOAT;
        if ((e->e_obj = objof(ici_float_new(p->p_got.t_float))) == NULL)
            goto fail;
        break;

    case T_STRING:
        e->e_what = T_STRING;
        o = p->p_got.t_obj;
        while (next(p, NULL) == T_STRING)
        {
            register int        i;

            i = stringof(p->p_got.t_obj)->s_nchars;
            if (ici_chkbuf(stringof(o)->s_nchars + i + 1))
                goto fail;
            memcpy(buf, stringof(o)->s_chars, stringof(o)->s_nchars);
            memcpy
            (
                buf + stringof(o)->s_nchars,
                stringof(p->p_got.t_obj)->s_chars,
                i
            );
            i += stringof(o)->s_nchars;
            ici_decref(o);
            ici_decref(p->p_got.t_obj);
            if ((o = objof(ici_str_new(buf, i))) == NULL)
                goto fail;
            this = T_NONE;
        }
        reject(p);
        e->e_obj = o;
        break;

    case T_REGEXP:
        e->e_what = T_CONST;
        e->e_obj = p->p_got.t_obj;
        break;

    case T_NAME:
        if (p->p_got.t_obj == SSO(_NULL))
        {
            e->e_what = T_NULL;
            ici_decref(p->p_got.t_obj);
            break;
        }
        e->e_what = T_NAME;
        e->e_obj = p->p_got.t_obj;
        break;

    case T_ONROUND:
        reject(p);
        ici_tfree(e, expr_t);
        e = NULL;
        if (bracketed_expr(p, &e) < 1)
            goto fail;
        break;

    case T_ONSQUARE:
        if (next(p, NULL) != T_NAME)
        {
            not_followed_by("[", "an identifier");
            goto fail;
        }
        if (p->p_got.t_obj == SSO(array))
        {
            ici_decref(p->p_got.t_obj);
            this = T_NONE;
            if ((a = ici_array_new(0)) == NULL)
                goto fail;
            for (;;)
            {
                switch (const_expression(p, &o, T_COMMA))
                {
                case -1: goto fail;
                case 1:
                    if (ici_stk_push_chk(a, 1))
                    {
                        ici_decref(a);
                        goto fail;
                    }
                    *a->a_top++ = o;
                    ici_decref(o);
                    if (next(p, NULL) == T_COMMA)
                        continue;
                    reject(p);
                    break;
                }
                break;
            }
            if (next(p, NULL) != T_OFFSQUARE)
            {
                ici_decref(a);
                not_followed_by("[array expr, expr ...", "\",\" or \"]\"");
                goto fail;
            }
            e->e_what = T_CONST;
            e->e_obj = objof(a);
        }
        else if
        (
            (is_class = p->p_got.t_obj == SSO(class))
            ||
            p->p_got.t_obj == SSO(struct)
        )
        {
            struct_t    *super;

            ici_decref(p->p_got.t_obj);
            this = T_NONE;
            d = NULL;
            super = NULL;
            if (next(p, NULL) == T_COLON || this == T_EQ)
            {
                int     is_eq;

                is_eq = this == T_EQ;
                switch (const_expression(p, &o, T_COMMA))
                {
                case 0: not_followed_by
                        (
                           is_eq ? "[stuct =" : "[struct :",
                           an_expression
                        );
                case -1: goto fail;
                }
                if (!isstruct(o))
                {
                    ici_decref(o);
                    if (this == T_EQ) /*### Improve error messages. */
                        ici_error = "the object being extended is not a struct";
                    else
                        ici_error = "the super is not a struct";
                    goto fail;
                }
                if (is_eq)
                    d = structof(o);
                else
                    super = structof(o);
                switch (next(p, NULL))
                {
                case T_OFFSQUARE:
                    reject(p);
                case T_COMMA:
                    break;

                default:
                    if (super != NULL)
                        ici_decref(super);
                   if (d != NULL)
                        ici_decref(o);
                    not_followed_by("[struct : expr", "\",\" or \"]\"");
                    goto fail;
                }
            }
            else
                reject(p);
            if (d == NULL)
            {
                if ((d = ici_struct_new()) == NULL)
                    goto fail;
                if ((d->o_head.o_super = objwsupof(super)) != NULL)
                    ici_decref(super);
            }
            if (is_class)
            {
                struct_t        *autos;

                /*
                 * If we don't have an explicit super, use the current statics.
                 */
                if (d->o_head.o_super == NULL)
                    d->o_head.o_super = objwsupof(ici_vs.a_top[-1])->o_super;
                /*
                 * A class definition operates within the scope context of
                 * the class. Create autos with the new struct as the super.
                 */
                if ((autos = ici_struct_new()) == NULL)
                    goto fail;
                autos->o_head.o_super = objwsupof(d);
                if (ici_stk_push_chk(&ici_vs, 80)) /* ### Formalise */
                    goto fail;
                *ici_vs.a_top++ = objof(autos);
                ici_decref(autos);
            }
            for (;;)
            {
                switch (next(p, NULL))
                {
                case T_OFFSQUARE:
                    break;

                case T_ONROUND:
                    switch (const_expression(p, &o, T_NONE))
                    {
                    case 0: not_followed_by("[struct ... (", an_expression);
                    case -1: ici_decref(d); goto fail;
                    }
                    if (next(p, NULL) != T_OFFROUND)
                    {
                        not_followed_by("[struct ... (expr", "\")\"");
                        goto fail;
                    }
                    n = o;
                    goto gotkey;

                case T_NAME:
                    n = p->p_got.t_obj;
                gotkey:
                    wasfunc = 0;
                    if (next(p, NULL) == T_ONROUND)
                    {
                        reject(p);
                        if (function(p, stringof(n)) < 0)
                            goto fail;
                        o = p->p_got.t_obj;
                        wasfunc = 1;
                    }
                    else if (this == T_EQ)
                    {
                        switch (const_expression(p, &o, T_COMMA))
                        {
                        case 0: not_followed_by("[struct ... ident =", an_expression);
                        case -1: goto fail;
                        }
                    }
                    else if (this == T_COMMA || this == T_OFFSQUARE)
                    {
                        reject(p);
                        o = objof(&o_null);
                        ici_incref(o);
                    }
                    else
                    {
                        not_followed_by("[struct ... key", "\"=\", \"(\", \",\" or \"]\"");
                        ici_decref(d);
                        ici_decref(n);
                        goto fail;
                    }
                    if (assign_base(d, n, o))
                        goto fail;
                    ici_decref(n);
                    ici_decref(o);
                    switch (next(p, NULL))
                    {
                    case T_OFFSQUARE:
                        reject(p);
                    case T_COMMA:
                        continue;

                    default:
                        if (wasfunc)
                        {
                            reject(p);
                            continue;
                        }
                    }
                    not_followed_by("[struct ... key = expr", "\",\" or \"]\"");
                    ici_decref(d);
                    goto fail;
                }
                break;
            }
            if (is_class)
            {
                /*
                 * That was a class definition. Restore the scope context.
                 */
                --ici_vs.a_top;
            }
            e->e_what = T_CONST;
            e->e_obj = objof(d);
        }
        else if (p->p_got.t_obj == SSO(set))
        {
            ici_decref(p->p_got.t_obj);
            this = T_NONE;
            if ((s = ici_set_new()) == NULL)
                goto fail;
            for (;;)
            {
                switch (const_expression(p, &o, T_COMMA))
                {
                case -1: goto fail;
                case 1:
                    if (ici_assign(s, o, ici_one))
                    {
                        ici_decref(s);
                        goto fail;
                    }
                    ici_decref(o);
                    if (next(p, NULL) == T_COMMA)
                        continue;
                    reject(p);
                    break;
                }
                break;
            }
            if (next(p, NULL) != T_OFFSQUARE)
            {
                ici_decref(s);
                not_followed_by("[set expr, expr ...", "\"]\"");
                goto fail;
            }
            e->e_what = T_CONST;
            e->e_obj = objof(s);
        }
        else if (p->p_got.t_obj == SSO(func))
        {
            ici_decref(p->p_got.t_obj);
            this = T_NONE;
            switch (function(p, SS(empty_string)))
            {
            case 0: not_followed_by("[func", "function body");
            case -1:
                goto fail;
            }
            e->e_what = T_CONST;
            e->e_obj = p->p_got.t_obj;
            if (next(p, NULL) != T_OFFSQUARE)
            {
                not_followed_by("[func function-body ", "\"]\"");
                goto fail;
            }
        }
        else if (p->p_got.t_obj == SSO(module))
        {
            struct_t    *autos;

            ici_decref(p->p_got.t_obj);
            this = T_NONE;

            if ((d = ici_struct_new()) == NULL)
                goto fail;

            d->o_head.o_super = objwsupof(ici_vs.a_top[-1])->o_super;
            if ((autos = ici_struct_new()) == NULL)
                goto fail;
            autos->o_head.o_super = objwsupof(d);
            *ici_vs.a_top++ = objof(autos);
            ici_decref(autos);
            ++p->p_module_depth;
            o = ici_evaluate(objof(p), 0);
            --p->p_module_depth;
            --ici_vs.a_top;
            if (o == NULL)
                goto fail;
            ici_decref(o);
            e->e_what = T_CONST;
            e->e_obj = objof(d);
        }
        else
        {
            ici_decref(p->p_got.t_obj);
            not_followed_by("[", "\"array\", \"struct\", \"set\", \"func\" or \"module\"");
            goto fail;
        }
        break;

    default:
        reject(p);
        ici_tfree(e, expr_t);
        return 0;
    }
    *ep = e;
    e = NULL;
    for (;;)
    {
        int     oldthis;

        switch (next(p, NULL))
        {
        case T_ONSQUARE:
            if ((e = ici_talloc(expr_t)) == NULL)
                goto fail;
            e->e_what = T_ONSQUARE;
            e->e_arg[0] = *ep;
            e->e_arg[1] = NULL;
            e->e_obj = NULL;
            *ep = e;
            e = NULL;
            switch (expr(p, &(*ep)->e_arg[1], T_NONE))
            {
            case 0: not_followed_by("[", an_expression);
            case -1: goto fail;
            }
            if (next(p, NULL) != T_OFFSQUARE)
            {
                not_followed_by("[ expr", "\"]\"");
                goto fail;
            }
            break;

        case T_COLON:
        case T_COLONCARET:
        case T_PTR:
        case T_DOT:
        case T_AT:
            if (this == exclude)
            {
                reject(p);
                return 1;
            }
            if ((e = ici_talloc(expr_t)) == NULL)
                goto fail;
            if ((oldthis = this) == T_AT)
                e->e_what = T_BINAT;
            else if (oldthis == T_COLON)
                e->e_what = T_PRIMARYCOLON;
            else
                e->e_what = this;
            e->e_arg[0] = *ep;
            e->e_arg[1] = NULL;
            e->e_obj = NULL;
            *ep = e;
            e = NULL;
            switch (next(p, NULL))
            {
            case T_NAME:
                if ((e = ici_talloc(expr_t)) == NULL)
                    goto fail;
                e->e_what = T_STRING;
                e->e_arg[0] = NULL;
                e->e_arg[1] = NULL;
                e->e_obj = NULL;
                e->e_obj = p->p_got.t_obj;
                (*ep)->e_arg[1] = e;
                e = NULL;
                break;

            case T_ONROUND:
                reject(p);
                if (bracketed_expr(p, &(*ep)->e_arg[1]) < 1)
                    goto fail;
                break;

            default:
                switch (oldthis)
                {
                case T_COLON:      token_name = ":"; break;
                case T_COLONCARET: token_name = ":^"; break;
                case T_PTR:        token_name = "->"; break;
                case T_DOT:        token_name = "."; break;
                case T_AT:         token_name = "@"; break;
                default: abort();
                }
                not_followed_by(token_name, "an identifier or \"(\"");
                goto fail;
            }
            break;

        case T_ONROUND: /* Function call. */
            if ((e = ici_talloc(expr_t)) == NULL)
                goto fail;
            e->e_what = T_ONROUND;
            e->e_arg[0] = *ep;
            e->e_arg[1] = NULL;
            e->e_obj = NULL;
            *ep = e;
            e = NULL;
            for (;;)
            {
                expr_t  *e1;

                e1 = NULL;
                switch (expr(p, &e1, T_COMMA))
                {
                case -1:
                    goto fail;

                case 1:
                    if ((e = ici_talloc(expr_t)) == NULL)
                        goto fail;
                    e->e_arg[1] = (*ep)->e_arg[1];
                    (*ep)->e_arg[1] = e;
                    e->e_what = T_COMMA;
                    e->e_arg[0] = e1;
                    e->e_obj = NULL;
                    e = NULL;
                    if (next(p, NULL) == T_COMMA)
                        continue;
                    reject(p);
                    break;
                }
                break;
            }
            if (next(p, NULL) != T_OFFROUND)
            {
                ici_error = "error in function call arguments";
                goto fail;
            }
            if (next(p, NULL) == T_ONCURLY)
            {
                /*
                 * Gratuitous check to get a better error message.
                 */
                ici_error = "function definition without a storage class";
                goto fail;
            }
            reject(p);
            break;


        default:
            reject(p);
            return 1;
        }
    }

fail:
    if (e != NULL)
    {
        if (e->e_obj != NULL)
            ici_decref(e->e_obj);
        ici_tfree(e, expr_t);
    }
    free_expr(*ep);
    *ep = NULL;
    return -1;
}

/*
 * Parse a sub-expression consisting or a sequence of unary operators round
 * a primary (a factor) in the parse context 'p' and store the expression
 * tree of 'expr_t' type nodes under the pointer indicated by 'ep'. Usual
 * parseing return conventions (see comment near start of file). See the
 * comment on expr() for the meaning of exclude.
 */
static int
unary(parse_t *p, expr_t **ep, int exclude)
{
    expr_t      *e;
    int         what;

    switch (next(p, NULL))
    {
    case T_ASTERIX:
    case T_AND:
    case T_MINUS:
    case T_PLUS:
    case T_EXCLAM:
    case T_TILDE:
    case T_PLUSPLUS:
    case T_MINUSMINUS:
    case T_AT:
    case T_DOLLAR:
        what = this;
        switch (unary(p, ep, exclude))
        {
        case 0: ici_error = "badly formed expression";
        case -1: return -1;
        }
        if ((e = ici_talloc(expr_t)) == NULL)
            return -1;
        e->e_what = what;
        e->e_arg[0] = *ep;
        e->e_arg[1] = NULL;
        e->e_obj = NULL;
        *ep = e;
        break;

    default:
        reject(p);
        switch (primary(p, ep, exclude))
        {
        case 0: return 0;
        case -1: return -1;
        }
    }
    switch (next(p, NULL))
    {
    case T_PLUSPLUS:
    case T_MINUSMINUS:
        if ((e = ici_talloc(expr_t)) == NULL)
            return -1;
        e->e_what = this;
        e->e_arg[0] = NULL;
        e->e_arg[1] = *ep;
        e->e_obj = NULL;
        *ep = e;
        break;

    default:
        reject(p);
        break;
    }
    return 1;
}

/*
 * Parse an expression in the parse context 'p' and store the expression
 * tree of 'expr_t' type nodes under the pointer indicated by 'ep'. Usual
 * parseing return conventions (see comment near start of file).
 *
 * exclude              A binop token that would normally be allowed
 *                      but because of the context this expression is
 *                      being parsed in, must be excluded. This is used
 *                      to exclude commas from argument lists and colons
 *                      from case labels and such like. The support is
 *                      not general. Only what is needed for comma and
 *                      colon.
 */
static int
expr(parse_t *p, expr_t **ep, int exclude)
{
    expr_t      *e;
    expr_t      **ebase;
    expr_t      *elimit;
    int         tp;
    int         r;
    int         in_quest_colon;

    /*
     * This expression parser is neither state stack based nor recursive
     * descent. It maintains an epression tree, and re-forms it each time
     * it finds a subsequent binary operator and following factor. In
     * practice this is probably faster than either the other two methods.
     * It handles all the precedence and right/left associativity and
     * the ? : operator correctly (at least according to ICI's definition
     * of ? :).
     */

    /*
     * Get the first factor.
     */
    if ((r = unary(p, ebase = ep, exclude)) <= 0)
        return r;
    elimit = *ebase;

    /*
     * While there is a following binary operator, merge it and the
     * following factor into the expression.
     */
    while (t_type(next(p, NULL)) == T_BINOP && this != exclude)
    {
        /*
         * Cause assignments to be right associative.
         */
        if ((tp = t_prec(this)) == t_prec(T_EQ))
            --tp;

        /*
         * Slide down the right hand side of the tree to find where this
         * operator binds.
         */
        in_quest_colon = this == T_QUESTION;
        for
        (
            ep = ebase;
            (e = *ep) != elimit && tp < t_prec(e->e_what);
            ep = &e->e_arg[1]
        )
        {
            if (e->e_what == T_QUESTION)
                in_quest_colon = 1;
        }

        /*
         * Allocate a new node and rebuild this bit with the new operator
         * and the following factor.
         */
        if ((e = ici_talloc(expr_t)) == NULL)
        {
            /*ici_tfree((char *)e); ###???*/
            return -1;
        }
        e->e_what = this;
        e->e_arg[0] = *ep;
        e->e_arg[1] = NULL;
        e->e_obj = NULL;
        switch (unary(p, &e->e_arg[1], in_quest_colon ? T_COLON : exclude))
        {
        case 0:
            sprintf(buf, "\"expr %s\" %s %s",
                ici_binop_name(t_subtype(e->e_what)), not_by, an_expression);
            ici_error = buf;
        case -1:
            ici_tfree(e, expr_t);
            return -1;
        }
        *ep = e;
        elimit = e->e_arg[1];
    }
    reject(p);
    return 1;
}

static int
expression(parse_t *p, array_t *a, int why, int exclude)
{
    expr_t      *e;

    e = NULL;
    switch (expr(p, &e, exclude))
    {
    case 0: return 0;
    case -1: goto fail;
    }
    if (compile_expr(a, e, why))
        goto fail;
    free_expr(e);
    return 1;

fail:
    free_expr(e);
    return -1;
}

/*
 * Parse and evaluate an expected "constant" (that is, parse time evaluated)
 * expression and store the result through po. The excluded binop (see comment
 * on expr() above) is often used to exclude comma operators.
 *
 * Usual parseing return conventions.
 */
static int
const_expression(parse_t *p, object_t **po, int exclude)
{
    expr_t      *e;
    array_t     *a;
    int         ret;

    a = NULL;
    e = NULL;
    if ((ret = expr(p, &e, exclude)) <= 0)
        return ret;
    /*
     * If the expression is a data item that obviously requires no
     * further compilation and evaluation to arrive at its value,
     * we just use the value directly.
     */
    switch (e->e_what)
    {
    case T_NULL:
        *po = objof(&o_null);
        goto simple;

    case T_INT:
    case T_FLOAT:
    case T_STRING:
    case T_CONST:
        *po = e->e_obj;
    simple:
        ici_incref(*po);
        free_expr(e);
        return 1;
    }
    if ((a = ici_array_new(0)) == NULL)
        goto fail;
    if (compile_expr(a, e, FOR_VALUE))
        goto fail;
    if (ici_stk_push_chk(a, 1))
        goto fail;
    *a->a_top++ = objof(&o_end);
    free_expr(e);
    e = NULL;
    if ((*po = ici_evaluate(objof(a), 0)) == NULL)
        goto fail;
    ici_decref(a);
    return 1;

fail:
    if (a != NULL)
        ici_decref(a);
    free_expr(e);
    return -1;
}

static int
xx_brac_expr_brac(parse_t *p, array_t *a, char *xx)
{
    if (next(p, a) != T_ONROUND)
    {
        sprintf(buf, "\"%s\" %s a \"(\"", xx, not_by);
        goto fail;
    }
    switch (expression(p, a, FOR_VALUE, T_NONE))
    {
    case 0:
        sprintf(buf, "\"%s (\" %s %s", xx, not_by, an_expression);
        goto fail;

    case -1:
        return -1;
    }
    if (next(p, a) != T_OFFROUND)
    {
        sprintf(buf, "\"%s (expr\" %s \")\"", xx, not_by);
        goto fail;
    }
    return 1;

fail:
    ici_error = buf;
    return -1;
}

/*
 * a    Code array being appended to.
 * sw   Switch structure, else NULL.
 * m    Who needs it, else NULL.
 * endme If non-zero, put an o_end at the end of the code array before
 *      returning.
 */
static int
statement(parse_t *p, array_t *a, struct_t *sw, char *m, int endme)
{
    array_t     *a1;
    array_t     *a2;
    expr_t      *e;
    struct_t    *d;
    objwsup_t   *ows;
    object_t    *o;
    object_t    **op;
    int_t       *i;
    int         stepz;

    switch (next(p, a))
    {
    case T_ONCURLY:
        reject(p);
        if (compound_statement(p, NULL) == -1)
            return -1;
        a1 = arrayof(p->p_got.t_obj);
        /*
         * Expand the statement in-line. Just a minor optimisation.
         */
        if (ici_stk_push_chk(a, a1->a_top - a1->a_base))
            return -1;
        /*
         * Copy in-line, up to end or o_end operator.
         */
        for (op = a1->a_base; op < a1->a_top && *op != objof(&o_end); ++op)
            *a->a_top++ = *op;
        ici_decref(a1);
        break;

    case T_SEMICOLON:
        break;

    case T_OFFCURLY: /* Just to prevent unecessary expression parseing. */
    case T_EOF:
    case T_ERROR:
        reject(p);
        goto none;

    case T_NAME:
        if (p->p_got.t_obj == SSO(extern))
        {
            ici_decref(p->p_got.t_obj);
            if
            (
                (ows = objwsupof(ici_vs.a_top[-1])->o_super) == NULL
                ||
                (ows = ows->o_super) == NULL
            )
            {
                ici_error = "extern declaration, but no extern variable scope";
                return -1;
            }
            goto decl;
        }
        if (p->p_got.t_obj == SSO(static))
        {
            ici_decref(p->p_got.t_obj);
            if ((ows = objwsupof(ici_vs.a_top[-1])->o_super) == NULL)
            {
                ici_error = "static declaration, but no static variable scope";
                return -1;
            }
            goto decl;
        }
        if (p->p_got.t_obj == SSO(auto))
        {
            ici_decref(p->p_got.t_obj);
            if (p->p_func == NULL)
                ows = objwsupof(ici_vs.a_top[-1]);
            else
                ows = objwsupof(p->p_func->f_autos);
        decl:
            if (data_def(p, ows) == -1)
                return -1;
            break;
        }
        if (p->p_got.t_obj == SSO(case))
        {
            ici_decref(p->p_got.t_obj);
            if (sw == NULL)
            {
                ici_error = "\"case\" not at top level of switch body";
                return -1;
            }
            switch (const_expression(p, &o, T_COLON))
            {
            case 0: not_followed_by("case", an_expression);
            case -1: return -1;
            }
            if ((i = ici_int_new((long)(a->a_top - a->a_base))) == NULL)
            {
                ici_decref(o);
                return -1;
            }
            if (ici_assign(sw, o, i))
            {
                ici_decref(i);
                ici_decref(o);
                return -1;
            }
            ici_decref(i);
            ici_decref(o);
            if (next(p, a) != T_COLON)
                return not_followed_by("case expr", "\":\"");
            break;
        }
        if (p->p_got.t_obj == SSO(default))
        {
            ici_decref(p->p_got.t_obj);
            if (sw == NULL)
            {
                ici_error = "\"default\" not at top level of switch body";
                return -1;
            }
            if (next(p, a) != T_COLON)
                return not_followed_by("default", "\":\"");
            if ((i = ici_int_new((long)(a->a_top - a->a_base))) == NULL)
                return -1;
            if (ici_assign(sw, &o_mark, i))
            {
                ici_decref(i);
                return -1;
            }
            ici_decref(i);
            break;
        }
        if (p->p_got.t_obj == SSO(if))
        {
            ici_decref(p->p_got.t_obj);
            if (xx_brac_expr_brac(p, a, "if") != 1)
                return -1;
            if ((a1 = ici_array_new(0)) == NULL)
                return -1;
            if (statement(p, a1, NULL, "if (expr)", 1) == -1)
            {
                ici_decref(a1);
                return -1;
            }
            a2 = NULL;
            if (next(p, a) == T_NAME && p->p_got.t_obj == SSO(else))
            {
                ici_decref(p->p_got.t_obj);
                if ((a2 = ici_array_new(0)) == NULL)
                    return -1;
                if (statement(p, a2, NULL, "if (expr) stmt else", 1) == -1)
                    return -1;
            }
            else
                reject(p);
            if (ici_stk_push_chk(a, 3))
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            if (a2 != NULL)
            {
                *a->a_top++ = objof(a2);
                ici_decref(a2);
                *a->a_top++ = objof(&o_ifelse);
            }
            else
                *a->a_top++ = objof(&o_if);
            break;

        }
        if (p->p_got.t_obj == SSO(while))
        {
            ici_decref(p->p_got.t_obj);
            if ((a1 = ici_array_new(0)) == NULL)
                return -1;
            if (xx_brac_expr_brac(p, a1, "while") != 1)
            {
                ici_decref(a1);
                return -1;
            }
            if (ici_stk_push_chk(a1, 1))
            {
                ici_decref(a1);
                return -1;
            }
            *a1->a_top++ = objof(&o_ifnotbreak);
            if (statement(p, a1, NULL, "while (expr)", 0) == -1)
                return -1;
            if (ici_stk_push_chk(a1, 1))
            {
                ici_decref(a1);
                return -1;
            }
            *a1->a_top++ = objof(&o_rewind);
            if (ici_stk_push_chk(a, 2))
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            *a->a_top++ = objof(&o_loop);
            break;
        }
        if (p->p_got.t_obj == SSO(do))
        {
            ici_decref(p->p_got.t_obj);
            if ((a1 = ici_array_new(0)) == NULL)
                return -1;
            if (statement(p, a1, NULL, "do", 0) == -1)
                return -1;
            if (next(p, a1) != T_NAME || p->p_got.t_obj != SSO(while))
                return not_followed_by("do statement", "\"while\"");
            ici_decref(p->p_got.t_obj);
            if (next(p, NULL) != T_ONROUND)
                return not_followed_by("do statement while", "\"(\"");
            switch (expression(p, a1, FOR_VALUE, T_NONE))
            {
            case 0: ici_error = "syntax error";
            case -1: ici_decref(a1); return -1;
            }
            if (next(p, a1) != T_OFFROUND || next(p, NULL) != T_SEMICOLON)
            {
                ici_decref(a1);
                return not_followed_by("do statement while (expr", "\");\"");
            }
            if (ici_stk_push_chk(a1, 2))
                return -1;
            *a1->a_top++ = objof(&o_ifnotbreak);
            *a1->a_top++ = objof(&o_rewind);

            if (ici_stk_push_chk(a, 2))
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            *a->a_top++ = objof(&o_loop);
            break;
        }
        if (p->p_got.t_obj == SSO(forall))
        {
            int         rc;

            ici_decref(p->p_got.t_obj);
            if (next(p, a) != T_ONROUND)
                return not_followed_by("forall", "\"(\"");
            if ((rc = expression(p, a, FOR_LVALUE, T_COMMA)) == -1)
                return -1;
            if (rc == 0)
            {
                if (ici_stk_push_chk(a, 2))
                    return -1;
                *a->a_top++ = objof(&o_null);
                *a->a_top++ = objof(&o_null);
            }
            if (next(p, a) == T_COMMA)
            {
                if (expression(p, a, FOR_LVALUE, T_COMMA) == -1)
                    return -1;
                if (next(p, a) != T_NAME || p->p_got.t_obj != SSO(in))
                    return not_followed_by("forall (expr, expr", "\"in\"");
                ici_decref(p->p_got.t_obj);
            }
            else
            {
                if (this != T_NAME || p->p_got.t_obj != SSO(in))
                    return not_followed_by("forall (expr", "\",\" or \"in\"");
                ici_decref(p->p_got.t_obj);
                if (ici_stk_push_chk(a, 2))
                    return -1;
                *a->a_top++ = objof(&o_null);
                *a->a_top++ = objof(&o_null);
            }
            if (expression(p, a, FOR_VALUE, T_NONE) == -1)
                return -1;
            if (next(p, a) != T_OFFROUND)
                return not_followed_by("forall (expr [, expr] in expr", "\")\"");
            if ((a1 = ici_array_new(0)) == NULL)
                return -1;
            if (statement(p, a1, NULL, "forall (expr [, expr] in expr)", 1) == -1)
                return -1;
            if (ici_stk_push_chk(a, 2))
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            if ((*a->a_top = objof(new_op(ici_op_forall, 0, 0))) == NULL)
                return -1;
            ici_decref(*a->a_top);
            ++a->a_top;
            break;

        }
        if (p->p_got.t_obj == SSO(for))
        {
            ici_decref(p->p_got.t_obj);
            if (next(p, a) != T_ONROUND)
                return not_followed_by("for", "\"(\"");
            if (expression(p, a, FOR_EFFECT, T_NONE) == -1)
                return -1;
            if (next(p, a) != T_SEMICOLON)
                return not_followed_by("for (expr", "\";\"");

            /*
             * Get the condition expression, but don't generate code yet.
             */
            e = NULL;
            if (expr(p, &e, T_NONE) == -1)
                return -1;

            if (next(p, a) != T_SEMICOLON)
                return not_followed_by("for (expr; expr", "\";\"");

            /*
             * a1 is the body of the loop.  Get the step expression.
             */
            if ((a1 = ici_array_new(0)) == NULL)
                return -1;
            if (expression(p, a1, FOR_EFFECT, T_NONE) == -1)
                return -1;
            stepz = a1->a_top - a1->a_base;

            if (e != NULL)
            {
                /*
                 * Now compile in the test expression.
                 */
                if (compile_expr(a1, e, FOR_VALUE))
                {
                    free_expr(e);
                    return -1;
                }
                free_expr(e);
                if (ici_stk_push_chk(a1, 1))
                    return -1;
                *a1->a_top++ = objof(&o_ifnotbreak);
            }
            if (next(p, a1) != T_OFFROUND)
                return not_followed_by("for (expr; expr; expr", "\")\"");
            if (statement(p, a1, NULL, "for (expr; expr; expr)", 0) == -1)
                return -1;
            if (ici_stk_push_chk(a1, 1))
                return -1;
            *a1->a_top++ = objof(&o_rewind);
            if (ici_stk_push_chk(a, 2))
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            if ((*a->a_top = objof(new_op(ici_op_for, 0, stepz))) == NULL)
                return -1;
            ici_decref(*a->a_top);
            ++a->a_top;
            break;
        }
        if (p->p_got.t_obj == SSO(switch))
        {
            ici_decref(p->p_got.t_obj);
            if (xx_brac_expr_brac(p, a, "switch") != 1)
                return -1;
            if ((d = ici_struct_new()) == NULL)
                return -1;
            switch (compound_statement(p, d))
            {
            case 0: not_followed_by("switch (expr)", "a compound statement");
            case -1: return -1;
            }
            if (ici_stk_push_chk(a, 3))
                return -1;
            *a->a_top++ = p->p_got.t_obj;
            ici_decref(p->p_got.t_obj);
            *a->a_top++ = objof(d);
            *a->a_top++ = objof(&o_switch);
            ici_decref(d);
            break;
        }
        if (p->p_got.t_obj == SSO(break))
        {
            ici_decref(p->p_got.t_obj);
            if (next(p, a) != T_SEMICOLON)
                return not_followed_by("break", "\";\"");
            if (ici_stk_push_chk(a, 1))
                return -1;
            *a->a_top++ = objof(&o_break);
            break;

        }
        if (p->p_got.t_obj == SSO(continue))
        {
            ici_decref(p->p_got.t_obj);
            if (next(p, a) != T_SEMICOLON)
                return not_followed_by("continue", "\";\"");
            if (ici_stk_push_chk(a, 1))
                return -1;
            *a->a_top++ = objof(&o_continue);
            break;
        }
        if (p->p_got.t_obj == SSO(return))
        {
            ici_decref(p->p_got.t_obj);
            switch (expression(p, a, FOR_VALUE, T_NONE))
            {
            case -1: return -1;
            case 0:
                if (ici_stk_push_chk(a, 1))
                    return -1;
                if ((*a->a_top = objof(&o_null)) == NULL)
                    return -1;
                ++a->a_top;
            }
            if (next(p, a) != T_SEMICOLON)
                return not_followed_by("return [expr]", "\";\"");
            if (ici_stk_push_chk(a, 1))
                return -1;
            *a->a_top++ = objof(&o_return);
            break;
        }
        if (p->p_got.t_obj == SSO(try))
        {
            ici_decref(p->p_got.t_obj);
            if ((a1 = ici_array_new(0)) == NULL)
                return -1;
            if (statement(p, a1, NULL, "try", 1) == -1)
                return -1;
            if (next(p, NULL) != T_NAME || p->p_got.t_obj != SSO(onerror))
                return not_followed_by("try statement", "\"onerror\"");
            ici_decref(p->p_got.t_obj);
            if ((a2 = ici_array_new(0)) == NULL)
                return -1;
            if (statement(p, a2, NULL, "try statement onerror", 1) == -1)
                return -1;
            if (ici_stk_push_chk(a, 3))
                return -1;
            *a->a_top++ = objof(a1);
            *a->a_top++ = objof(a2);
            *a->a_top++ = objof(&o_onerror);
            ici_decref(a1);
            ici_decref(a2);
            break;
        }
        if (p->p_got.t_obj == SSO(critsect))
        {
            ici_decref(p->p_got.t_obj);
            /*
             * Start a critical section with a new code array (a1) as
             * its subject. Into this code array we place the statement.
             */
            if (ici_stk_push_chk(a, 2))
                return -1;
            if ((a1 = ici_array_new(1)) == NULL)
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            if (statement(p, a1, NULL, "critsect", 1) == -1)
                return -1;
            *a->a_top++ = objof(&o_critsect);
            break;
        }
        if (p->p_got.t_obj == SSO(waitfor))
        {
            ici_decref(p->p_got.t_obj);
            /*
             * Start a critical section with a new code array (a1) as
             * its subject. Into this code array we place a loop followed
             * by the statement. Thus the critical section will dominate
             * the entire waitfor statement. But the wait primitive
             * temporarily releases one level of critical section around
             * the actual wait call.
             */
            if (ici_stk_push_chk(a, 2))
                return -1;
            if ((a1 = ici_array_new(2)) == NULL)
                return -1;
            *a->a_top++ = objof(a1);
            ici_decref(a1);
            *a->a_top++ = objof(&o_critsect);
            /*
             * Start a new code array (a2) and establish it as the body of
             * a loop.
             */
            if (ici_stk_push_chk(a1, 2))
                return -1;
            if ((a2 = ici_array_new(2)) == NULL)
                return -1;
            *a1->a_top++ = objof(a2);
            ici_decref(a2);
            *a1->a_top++ = objof(&o_loop);
            /*
             * Into the new code array (a1, the body of the loop) we build:
             *     condition expression (for value)
             *     if-break operator
             *     wait object expression (for value)
             *     waitfor operator
             */
            if (next(p, a2) != T_ONROUND)
                return not_followed_by("waitfor", "\"(\"");
            switch (expression(p, a2, FOR_VALUE, T_NONE))
            {
            case 0: not_followed_by("waitfor (", an_expression);
            case -1: return -1;
            }
            if (ici_stk_push_chk(a2, 1))
                return -1;
            *a2->a_top++ = objof(&o_ifbreak);
            if (next(p, a2) != T_SEMICOLON)
                return not_followed_by("waitfor (expr", "\";\"");
            switch (expression(p, a2, FOR_VALUE, T_NONE))
            {
            case 0: not_followed_by("waitfor (expr;", an_expression);
            case -1: return -1;
            }
            if (ici_stk_push_chk(a2, 2))
                return -1;
            *a2->a_top++ = objof(&o_waitfor);
            *a2->a_top++ = objof(&o_rewind);
            /*
             * After we break out of the loop, we execute the statement,
             * but it is still on top of the critical section. After the
             * statement is executed, the execution engine will pop off
             * the critical section catcher.
             */
            if (next(p, a2) != T_OFFROUND)
                return not_followed_by("waitfor (expr; expr", "\")\"");
            if (statement(p, a1, NULL, "waitfor (expr; expr)", 1) == -1)
                return -1;
            break;
        }
    default:
        reject(p);
        switch (expression(p, a, FOR_EFFECT, T_NONE))
        {
        case 0: goto none;
        case -1: return -1;
        }
        if (next(p, a) != T_SEMICOLON)
        {
            ici_error = "badly formed expression, or missing \";\"";
            return -1;
        }
        break;
    }
    if (endme)
    {
        if (ici_stk_push_chk(a, 1))
            return -1;
        *a->a_top++ = objof(&o_end);
    }
    return 1;

none:
    if (m != NULL)
    {
        sprintf(buf, "\"%s\" %s a reasonable statement", m, not_by);
        ici_error = buf;
        return -1;
    }
    return 0;
}

/*
 * s    Scope; autos, statics, externs.
 */
int
parse_module(file_t *f, objwsup_t *s)
{
    parse_t             *p;
    object_t            *o;

    if ((p = new_parse(f)) == NULL)
        return -1;

    *ici_vs.a_top++ = objof(s);
    if ((o = ici_evaluate(objof(p), 0)) == NULL)
    {
        --ici_vs.a_top;
        ici_decref(p);
        return -1;
    }
    --ici_vs.a_top;
    ici_decref(o);
    ici_decref(p);
    return 0;
}

/*
 * Parse the given file, module file name 'mname'.  Return 0 if ok, else -1,
 * usual conventions.  It closes the file (if all goes well).
 */
int
parse_file(char *mname, char *file, ftype_t *ftype)
{
    objwsup_t           *s;     /* Statics. */
    objwsup_t           *a;     /* Autos. */
    file_t              *f;

    a = NULL;
    f = NULL;
    if ((f = new_file(file, ftype, ici_str_get_nul_term(mname))) == NULL)
        goto fail;

    if ((a = objwsupof(ici_struct_new())) == NULL)
        goto fail;
    if ((a->o_super = s = objwsupof(ici_struct_new())) == NULL)
        goto fail;
    ici_decref(s);
    s->o_super = objwsupof(ici_vs.a_top[-1])->o_super;

    if (parse_module(f, a) < 0)
        goto fail;
    f_close(f);
    ici_decref(a);
    ici_decref(f);
    return 0;

fail:
    if (f != NULL)
        ici_decref(f);
    if (a != NULL)
        ici_decref(a);
    return -1;
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_parse(object_t *o)
{
    long        mem;

    o->o_flags |= O_MARK;
    mem = sizeof(parse_t);
    if (parseof(o)->p_func != NULL)
        mem += ici_mark(parseof(o)->p_func);
    if (parseof(o)->p_file != NULL)
        mem += ici_mark(parseof(o)->p_file);
    return mem;
}

int
parse_exec(void)
{
    parse_t     *p;
    array_t     *a;

    if ((a = ici_array_new(0)) == NULL)
        return 1;

    p = parseof(ici_xs.a_top[-1]);

    for (;;)
    {
        switch (statement(p, a, NULL, NULL, 1))
        {
        case 1:
            if (a->a_top == a->a_base)
                continue;
            get_pc(a, ici_xs.a_top);
            ++ici_xs.a_top;
            ici_decref(a);
            return 0;

        case 0:
            next(p, a);
            if (p->p_module_depth > 0)
            {
                if (this != T_OFFSQUARE)
                {
                    not_followed_by("[module statements", "\"]\"");
                    goto fail;
                }
            }
            else if (this != T_EOF)
            {
                ici_error = "syntax error";
                goto fail;
            }
            --ici_xs.a_top;
            ici_decref(a);
            return 0;

        default:
        fail:
            ici_decref(a);
            if (this == T_ERROR)
                ici_error = p->p_got.t_str;
            expand_error(p->p_lineno, p->p_file->f_name);
            return 1;
        }
    }
}

parse_t *
new_parse(file_t *f)
{
    register parse_t    *p;

    if ((p = (parse_t *)ici_talloc(parse_t)) == NULL)
        return NULL;
    memset(p, 0, sizeof(parse_t));
    objof(p)->o_tcode = TC_PARSE;
    assert(ici_typeof(p) == &parse_type);
    objof(p)->o_flags = 0;
    objof(p)->o_nrefs = 1;
    rego(p);
    p->p_file = f;
    p->p_sol = 1;
    p->p_lineno = 1;
    p->p_func = NULL;
    p->p_ungot.t_what = T_NONE;
    return p;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_parse(object_t *o)
{
    ici_tfree(o, parse_t);
}

type_t  parse_type =
{
    mark_parse,
    free_parse,
    hash_unique,
    cmp_unique,
    copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "parse"
};

