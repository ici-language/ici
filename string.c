#define ICI_CORE
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
    register object_t   *o;
    size_t              az;

    az = STR_ALLOCZ(nchars);
    if ((o = (object_t *)ici_nalloc(az)) == NULL)
        return NULL;
    o->o_tcode = TC_STRING;
    assert(ici_typeof(o) == &string_type);
    o->o_flags = 0;
    o->o_nrefs = 1;
    rego(o);
    if (az <= 127)
        o->o_leafz = az;
    else
        o->o_leafz = 0;
    stringof(o)->s_nchars = nchars;
    stringof(o)->s_chars[nchars] = '\0';
    stringof(o)->s_struct = NULL;
    stringof(o)->s_slot = NULL;
    stringof(o)->s_hash = 0;
    stringof(o)->s_vsver = 0;
    return stringof(o);
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
    if ((size_t)nchars < sizeof proto.d)
    {
        proto.s.s_nchars = nchars;
        memcpy(proto.s.s_chars, p, nchars);
        proto.s.s_chars[nchars] = '\0';
        proto.s.s_hash = 0;
        if ((s = stringof(atom_probe(objof(&proto.s)))) != NULL)
        {
            ici_incref(s);
            return s;
        }
        az = STR_ALLOCZ(nchars);
        if ((s = (string_t *)ici_nalloc(az)) == NULL)
            return NULL;
        memcpy((char *)s, (char *)&proto.s, az);
        rego(s);
        objof(s)->o_leafz = az;
        return stringof(ici_atom(objof(s), 1));
    }
    if ((s = (string_t *)ici_nalloc(STR_ALLOCZ(nchars))) == NULL)
        return NULL;
    s->o_head = proto.s.o_head;
    assert(ici_typeof(s) == &string_type);
    rego(s);
    if (STR_ALLOCZ(nchars) <= 127)
        objof(s)->o_leafz = STR_ALLOCZ(nchars);
    else
        objof(s)->o_leafz = 0;
    s->s_nchars = nchars;
    s->s_struct = NULL;
    s->s_slot = NULL;
    s->s_vsver = 0;
    memcpy(s->s_chars, p, nchars);
    s->s_chars[nchars] = '\0';
    s->s_hash = 0;
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

int
need_string(string_t **p, char *s)
{
    if (*p != NULL)
        return 0;
    return (*p = ici_str_new_nul_term(s)) == NULL;
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
    unsigned char       *p;
    unsigned char       *ep;

    if (stringof(o)->s_hash)
        return stringof(o)->s_hash;
    p = stringof(o)->s_chars;
    ep = p + stringof(o)->s_nchars;
    h = STR_PRIME_0;
    while ((ep - p) & 0x3)
        h = *p++ + h * 31;
    while (p < ep)
    {
        h = *p++ + h * 17;
        h = *p++ + h * 19;
        h = *p++ + h * 23;
        h = *p++ + h * 31;
    }
    return stringof(o)->s_hash = h;
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
