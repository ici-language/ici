#define ICI_CORE
#include "str.h"
#include "re.h"
#include "exec.h"
#include "op.h"
#include "buf.h"
#include "primes.h"

#ifndef ICI
#define ICI /* Cause PCRE's internal.h to add special stuff for ICI */
#endif
#include "pcre/internal.h"

/*
 * Called when the above regexp routines detect an error.
 * Space for the offsets to matched sub-expressions in regexps and a global
 * counter for the number of matches. PCRE says the number of offsets must be
 * a multiple of three. See pcre.3 for an explanation.
 */

int     re_bra[(NSUBEXP + 1) * 3];
int     re_nbra;

regexp_t *
ici_regexp_new(string_t *s, int flags)
{
    regexp_t    *r;
    pcre        *re;
    pcre_extra  *rex = NULL;
    int         errofs;

    /* Special test for possible failure of ici_str_new_nul_term() in lex.c */
    if (s == NULL)
        return NULL;
    re = pcre_compile(s->s_chars, flags, (const char **)&ici_error, &errofs, NULL);
    if (re == NULL)
        return NULL;
    if (pcre_info(re, NULL, NULL) > NSUBEXP)
    {
        sprintf(buf, "too many subexpressions in regexp, limit is %d", NSUBEXP);
        ici_error = buf;
        goto fail;
    }
    rex = pcre_study(re, 0, (const char **)&ici_error);
    if (ici_error != NULL)
        goto fail;
    /* Note rex can be NULL if no extra info required */
    if ((r = (regexp_t *)ici_talloc(regexp_t)) == NULL)
        goto fail;
    objof(r)->o_tcode = TC_REGEXP;
    assert(ici_typeof(r) == &regexp_type);
    objof(r)->o_flags = 0;
    objof(r)->o_nrefs = 1;
    r->r_re  = re;
    r->r_rex = rex;
    if ((r->r_pat = stringof(copy(s))) == NULL)
    {
        ici_tfree(r, regexp_t);    /* Okay, prior to rego() */
        goto fail;
    }
    ici_decref(r->r_pat);
    rego(r);
    return regexpof(ici_atom(objof(r), 1));

fail:
    if (rex != NULL)
        ici_free(rex);
    if (re != NULL)
        ici_free(re);
    return NULL;
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_regexp(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(regexp_t) + ici_mark(regexpof(o)->r_pat) + ((real_pcre *)regexpof(o)->r_re)->size;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_regexp(object_t *o)
{
    if (regexpof(o)->r_rex != NULL)
        ici_free(regexpof(o)->r_rex);
    ici_free(regexpof(o)->r_re);
    ici_tfree(o, regexp_t);
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_regexp(object_t *o)
{
    /* static unsigned long     primes[] = {0xBF8D, 0x9A4F, 0x1C81, 0x6DDB}; */
    return (unsigned long)regexpof(o)->r_pat * 0x9A4F;
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_regexp(object_t *o1, object_t *o2)
{
    return cmp(regexpof(o1)->r_pat, regexpof(o2)->r_pat);
}

static object_t *
fetch_regexp(object_t *o, object_t *k)
{
    if (k == SSO(pattern))
        return objof(regexpof(o)->r_pat);
    if (k == SSO(options))
    {
        int     options;
        pcre_info(regexpof(o)->r_re, &options, NULL);
        return objof(ici_int_new(options));
    }
    return ici_fetch_fail(o, k);
}

/*
 * This function is just a wrapper round pcre_exec so that external modules
 * don't need to drag in the whole definition of pcre's include files.
 */
int
ici_pcre(regexp_t *r,
    const char *subject, int length, int start_offset,
    int options, int *offsets, int offsetcount)
{
    return pcre_exec
    (
        r->r_re,
        r->r_rex,
        subject,
        length,
        start_offset,
        options,
        offsets,
        offsetcount
    );
}

type_t  regexp_type =
{
    mark_regexp,
    free_regexp,
    hash_regexp,
    cmp_regexp,
    copy_simple,
    ici_assign_fail,
    fetch_regexp,
    "regexp"
};
