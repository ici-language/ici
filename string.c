#define ICI_CORE
#include "fwd.h"
#include "str.h"
#include "struct.h"
#include "exec.h"
#include "int.h"
#include "primes.h"

/*
 * How many bytes of memory we need for a string of n chars.
 */
#define STR_ALLOCZ(n)   (offsetof(string_t, s_chars) + (n) + 1)

string_t *
new_string(int nchars)
{
    register string_t   *s;
    size_t              az;

    az = STR_ALLOCZ(nchars);
    if ((s = (string_t *)ici_nalloc(az)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(s, TC_STRING, 0, 1, az <= 127 ? az : 0);
    s->s_nchars = nchars;
    s->s_chars[nchars] = '\0';
    s->s_struct = NULL;
    s->s_slot = NULL;
#   if KEEP_STRING_HASH
        s->s_hash = 0;
#   endif
    s->s_vsver = 0;
    ici_rego(s);
    return s;
}

/*
 * Note that the memory allocated to a string is always at least one byte
 * larger than the listed size and the extra byte contains a '\0'.  For
 * when a C string is needed.
 */
string_t *
ici_str_new(char *p, int nchars)
{
    string_t            *s;
    size_t              az;
    static struct
    {
        string_t        s;
        char            d[40];
    }
        proto   = {{OBJ(TC_STRING)}};

    assert(nchars >= 0);
    az = STR_ALLOCZ(nchars);
    if ((size_t)nchars < sizeof proto.d)
    {
        object_t        **po;

        proto.s.s_nchars = nchars;
        memcpy(proto.s.s_chars, p, nchars);
        proto.s.s_chars[nchars] = '\0';
#       if KEEP_STRING_HASH
            proto.s.s_hash = 0;
#       endif
        if ((s = stringof(atom_probe(objof(&proto.s), &po))) != NULL)
        {
            ici_incref(s);
            return s;
        }
        ++ici_supress_collect;
        az = STR_ALLOCZ(nchars);
        if ((s = (string_t *)ici_nalloc(az)) == NULL)
            return NULL;
        memcpy((char *)s, (char *)&proto.s, az);
        ICI_OBJ_SET_TFNZ(s, TC_STRING, O_ATOM, 1, az);
        ici_rego(s);
        --ici_supress_collect;
        ICI_STORE_ATOM_AND_COUNT(po, s);
        return s;
    }
    if ((s = (string_t *)ici_nalloc(az)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(s, TC_STRING, 0, 1, az <= 127 ? az : 0);
    s->s_nchars = nchars;
    s->s_struct = NULL;
    s->s_slot = NULL;
    s->s_vsver = 0;
    memcpy(s->s_chars, p, nchars);
    s->s_chars[nchars] = '\0';
#   if KEEP_STRING_HASH
        s->s_hash = 0;
#   endif
    ici_rego(s);
    return stringof(ici_atom(objof(s), 1));
}

string_t *
ici_str_new_nul_term(char *p)
{
    register string_t   *s;

    if ((s = ici_str_new(p, strlen(p))) == NULL)
        return NULL;
    return s;
}

/*
 * Same as ici_str_new_nul_term(), except the result is decref.
 */
string_t *
ici_str_get_nul_term(char *p)
{
    string_t    *s;

    if ((s = ici_str_new(p, strlen(p))) == NULL)
        return NULL;
    ici_decref(s);
    return s;
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_string(object_t *o)
{
    o->o_flags |= O_MARK;
    return STR_ALLOCZ(stringof(o)->s_nchars);
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_string(object_t *o1, object_t *o2)
{
    if (stringof(o1)->s_nchars != stringof(o2)->s_nchars)
        return 1;
    return memcmp
    (
        stringof(o1)->s_chars,
        stringof(o2)->s_chars,
        stringof(o1)->s_nchars
    );
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_string(object_t *o)
{
    ici_nfree(o, STR_ALLOCZ(stringof(o)->s_nchars));
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
unsigned long
ici_hash_string(object_t *o)
{
    unsigned long       h;

#   if KEEP_STRING_HASH
        if (stringof(o)->s_hash != 0)
            return stringof(o)->s_hash;
#   endif
    h = ici_crc(STR_PRIME_0, stringof(o)->s_chars, stringof(o)->s_nchars);
//printf("%8X %s\n", h, stringof(o)->s_chars);
#   if KEEP_STRING_HASH
        stringof(o)->s_hash = h;
#   endif
    return h;
}

/*
 * Return the object at key k of the obejct o, or NULL on error.
 * See the comment on t_fetch in object.h.
 */
static object_t *
fetch_string(object_t *o, object_t *k)
{
    register int        i;

    if (!isint(k))
        return ici_fetch_fail(o, k);
    if ((i = (int)intof(k)->i_value) < 0 || i >= stringof(o)->s_nchars)
        k = objof(ici_str_new("", 0));
    else
        k = objof(ici_str_new(&stringof(o)->s_chars[i], 1));
    if (k != NULL)
        ici_decref(k);
    return k;
}

type_t  string_type =
{
    mark_string,
    free_string,
    ici_hash_string,
    cmp_string,
    copy_simple,
    ici_assign_fail,
    fetch_string,
    "string"
};
