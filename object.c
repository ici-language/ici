#define ICI_CORE
#include "exec.h"
#include "buf.h"
#include "int.h"
#include "str.h"
#include "float.h"
#include "func.h"
#include "pc.h"
#include "primes.h"

#include <limits.h>

#define VERBOSE 0

/*
 * The global error message pointer.
 */
char            *ici_error;

/*
 * The array of known types. Initialised with the types known to the
 * core. NB: The positions of these must exactly match the TC_* defines
 * in object.h.
 */
type_t          *ici_types[ICI_MAX_TYPES] =
{
    NULL,
    &pc_type,
    &src_type,
    &parse_type,
    &op_type,
    &string_type,
    &ici_catch_type,
    &forall_type,
    &int_type,
    &float_type,
    &regexp_type,
    &ptr_type,
    &ici_array_type,
    &struct_type,
    &set_type,
    &ici_exec_type,
    &file_type,
    &ici_func_type,
    &ici_cfunc_type,
    &ici_method_type,
    &mark_type,
    &null_type,
    &ici_handle_type,
    &socket_type,
    &mem_type,
};

static int              ici_ntypes = TC_MAX_CORE + 1;

/*
 * All objects are in the objects list or completely static and
 * known to never require collection.
 */
object_t        **objs;         /* List of all objects. */
object_t        **objs_limit;   /* First element we can't use in list. */
object_t        **objs_top;     /* Next unused element in list. */

object_t        **atoms;        /* Hash table of atomic objects. */
int             atomsz;         /* Number of slots in hash table. */
static int      natoms;         /* Number of atomic objects. */

/*
 * Format a human readable version of the object in 30 chars or less.
 */
char *
objname(char *p, object_t *o)
{
    if (ici_typeof(o)->t_objname != NULL)
    {
        (*ici_typeof(o)->t_objname)(o, p);
        return p;
    }

    if (isstring(o))
    {
        if (stringof(o)->s_nchars > 10)
            sprintf(p, "\"%.10s...\"", stringof(o)->s_chars);
        else
            sprintf(p, "\"%s\"", stringof(o)->s_chars);
    }
    else if (isint(o))
        sprintf(p, "%ld", intof(o)->i_value);
    else if (isfloat(o))
        sprintf(p, "%g", floatof(o)->f_value);
    else if (strchr("aeiou", ici_typeof(o)->t_name[0]) != NULL)
        sprintf(p, "an %s", ici_typeof(o)->t_name);
    else
        sprintf(p, "a %s", ici_typeof(o)->t_name);
    return p;
}

int
ici_register_type(type_t *t)
{
    if (ici_ntypes == ICI_MAX_TYPES)
    {
        ici_error = "too many primitive types";
        return 0;
    }
    ici_types[ici_ntypes] = t;
    return ici_ntypes++;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
void
free_simple(object_t *o)
{
    ici_free(o);
}


/*
 * Return a copy of the given object, or NULL on error.
 * See the comment on t_copy() in object.h.
 */
object_t *
copy_simple(object_t *o)
{
    incref(o);
    return o;
}

/*
 * Assign to key k of the object o the value v. Return 1 on error, else 0.
 * See the comment on t_assign() in object.h.
 *
 * This is a generic function which can be used directly as the assign
 * function of a type which doesn't support assignment, or called from
 * within a custom assign function in cases where the particular assignment
 * is illegal.
 */
int
assign_simple(object_t *o, object_t *k, object_t *v)
{
    char        n1[30];
    char        n2[30];
    char        n3[30];

    sprintf(buf, "attempt to set %s keyed by %s to %s",
        objname(n1, o),
        objname(n2, k),
        objname(n3, v));
    ici_error = buf;
    return 1;
}

/*
 * Return the object at key k of the obejct o, or NULL on error.
 * See the comment on t_fetch in object.h.
 *
 * This is a generic function which can be used directly as the fetch
 * function of a type which doesn't support fetching, or called from
 * within a custom fetch function in cases where the particular fetch
 * is illegal.
 */
object_t *
fetch_simple(object_t *o, object_t *k)
{
    char        n1[30];
    char        n2[30];

    sprintf(buf, "attempt to read %s keyed by %s",
        objname(n1, o),
        objname(n2, k));
    ici_error = buf;
    return NULL;
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * For objects which can't be copied and are intrinsically unique.
 * See the comments on t_cmp() in object.h.
 */
int
cmp_unique(object_t *o1, object_t *o2)
{
    return o1 != o2;
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
unsigned long
hash_unique(object_t *o)
{
    return ((unsigned long)o >> 4) * UNIQUE_PRIME;
}

/*
 * Grow the hash table of atoms to be four times as big, plus three.
 */
static void
grow_atoms(void)
{
    register object_t   **o;
    register int        i;
    object_t            **olda;
    unsigned long       newz;
    ptrdiff_t           oldz;

    newz = atomsz * 2;
    oldz = atomsz;
    if ((o = (object_t **)ici_nalloc(newz * sizeof(object_t *))) == NULL)
        return;
    atomsz = newz;
    memset((char *)o, 0, newz * sizeof(object_t *));
    natoms = 0;
    olda = atoms;
    atoms = o;
    i = oldz;
    while (--i >= 0)
    {
        if (olda[i] != NULL)
        {
            olda[i]->o_flags &= ~O_ATOM;
            atom(olda[i], 1);
        }
    }
    ici_nfree(olda, oldz * sizeof(object_t *));
}

/*
 * Return an object equal to the one given, but possibly shared by others.
 * Never fails, at worst it just returns its argument.  If the lone flag
 * is given, the object is free'd if it isn't used.  ("lone" because the
 * caller has the lone reference to it and will replace that with what
 * atom returns anyway.)  If the lone flag is not given, and the object
 * would be used, a copy will be used.  Also note that if lone is true and
 * the object is not used, the nrefs of the passed object will be transfered
 * to the object being returned.
 */
object_t *
atom(object_t *o, int lone)
{
    object_t    **po;

    if (o->o_flags & O_ATOM)
        return o;
    for
    (
        po = &atoms[ici_atom_hash_index(hash(o))];
        *po != NULL;
        --po < atoms ? po = atoms + atomsz - 1 : NULL
    )
    {
        if (o->o_tcode == (*po)->o_tcode && cmp(o, *po) == 0)
        {
            if (lone)
            {
                (*po)->o_nrefs += o->o_nrefs;
                o->o_nrefs = 0;
            }
            return *po;
        }
    }

    /*
     * Not found.  Add this object (or a copy of it) to the atom pool.
     */
    if (!lone)
    {
        if ((*po = copy(o)) == NULL)
            return o;
        o = *po;
    }
    *po = o;
    o->o_flags |= O_ATOM;
    if (++natoms > atomsz / 2)
        grow_atoms();
    if (!lone)
        decref(o);
    return o;
}

/*
 * Probe the atom pool for an atomic form of o.  If found, return that
 * atomic form, else NULL.  Used by various new_*() routines.  These
 * routines generally set up a dummy version of the object being made
 * which is passed to this probe.  If it finds a match, that is returned,
 * thus avoiding the allocation of an object that may be thrown
 * away anyway.
 */
object_t *
atom_probe(object_t *o)
{
    object_t    **po;

    for
    (
        po = &atoms[ici_atom_hash_index(hash(o))];
        *po != NULL;
        --po < atoms ? po = atoms + atomsz - 1 : NULL
    )
    {
        if (o->o_tcode == (*po)->o_tcode && cmp(o, *po) == 0)
            return *po;
    }
    return NULL;
}

/*
 * Quick search for an int to save allocation/deallocation if it already
 * exists.
 */
int_t *
atom_int(long i)
{
    object_t    *o;
    object_t    **po;

    /*
     * NB: There is an in-line version of this code in binop.h
     */
    for
    (
        po = &atoms[ici_atom_hash_index((unsigned long)i * INT_PRIME)];
        (o = *po) != NULL;
        --po < atoms ? po = atoms + atomsz - 1 : NULL
    )
    {
        if (isint(o) && intof(o)->i_value == i)
            return intof(o);
    }
    return NULL;
}

static void
unatom(object_t *o)
{
    register object_t   **sl;
    register object_t   **ss;
    register object_t   **ws;   /* Wanted position. */

    for
    (
        ss = &atoms[ici_atom_hash_index(hash(o))];
        *ss != NULL;
        --ss < atoms ? ss = atoms + atomsz - 1 : NULL
    )
    {
        if (o == *ss)
           goto delete;
    }
    /*printf("Warning: could not find atom being deleted\n");*/
    return;

delete:
    o->o_flags &= ~O_ATOM;
    --natoms;
    sl = ss;
    /*
     * Scan "forward" bubbling up entries which would rather be at our
     * current empty slot.
     */
    for (;;)
    {
        if (--sl < atoms)
            sl = atoms + atomsz - 1;
        if (*sl == NULL)
            break;
        ws = &atoms[ici_atom_hash_index(hash(*sl))];
        if
        (
            (sl < ss && (ws >= ss || ws < sl))
            ||
            (sl > ss && (ws >= ss && ws < sl))
        )
        {
            /*
             * The value at sl, which really wants to be at ws, should go
             * into the current empty slot (ss).  Copy it to there and update
             * ss to be here (which now becomes empty).
             */
            *ss = *sl;
            ss = sl;
        }
    }
    *ss = NULL;
}

void
grow_objs(o)
object_t        *o;
{
    object_t            **newobjs;
    ptrdiff_t           newz;
    ptrdiff_t           oldz;

    oldz = objs_limit - objs;
    newz = 2 * oldz;
    if ((newobjs = (object_t **)ici_nalloc(newz * sizeof(object_t *))) == NULL)
        return;
    memcpy((char *)newobjs, (char *)objs, (char *)objs_limit - (char *)objs);
    objs_limit = newobjs + newz;
    objs_top = newobjs + (objs_top - objs);
    memset((char *)objs_top, 0, (char *)objs_limit - (char *)objs_top);
    ici_nfree(objs, oldz * sizeof(object_t *));
    objs = newobjs;
    *objs_top++ = o;
}

/*
 * Mark sweep garbage collection.  Should be safe to do any time, as new
 * objects are created without the nrefs == 0 which allows them to be
 * collected.  They must be explicitly lost before they are subject
 * to garbage collection.  But of course all code must be careful not
 * to hang on to "found" objects where they are not accessible, or they
 * will be collected.  You can incref() them if you want.  All "held" objects
 * will cause all objects referenced from them to be marked (ie, not
 * collected), as long as they are registered on either the global object
 * list or in the atom pool.  Thus statically declared objects which
 * reference other objects (very rare) must be appropriately registered.
 */
void
collect(void)
{
    register object_t   **a;
    register object_t   *o;
    register object_t   **b;
    register int        ndead_atoms;
    register long       mem;    /* Total mem tied up in refed objects. */

#if 0
    long                tc_counts[16];
    memset((char *)tc_counts, 0, sizeof tc_counts);
#endif

    /*
     * Mark all objects which are referenced (and thus what they ref).
     */
    mem = 0;
    for (a = objs; a < objs_top; ++a)
    {
        if ((*a)->o_nrefs != 0)
            mem += mark(*a);
    }

    /*
     * Count how many atoms are going to be retained and how many are
     * going to be lost so we can decide on the fastest method.
     */
    ndead_atoms = 0;
    for (a = objs; a < objs_top; ++a)
    {
#if 0
        if (((*a)->o_flags & O_MARK) != 0)
            ++tc_counts[(*a)->o_tcode];
#endif
        if (((*a)->o_flags & (O_ATOM|O_MARK)) == O_ATOM)
            ++ndead_atoms;
    }

    /*
     * Collection phase.  Discard unmarked objects, compact down marked
     * objects and fix up the atom pool.
     *
     * Deleteing an atom from the atom pool is (say) once as expensive
     * as adding one.  Use this to determine which is quicker; rebuilding
     * the atom pool or deleting dead ones.
     */
    if (1 * ndead_atoms > (natoms - ndead_atoms))
    {
        /*
         * Rebuilding the atom pool is a better idea.
         */
        memset((char *)atoms, 0, atomsz * sizeof(object_t *));
        natoms = 0;
        b = objs;
        for (a = objs; a < objs_top; ++a)
        {
            if (((o = *a)->o_flags & O_MARK) == 0)
                freeo(o);
            else
            {
                o->o_flags &= ~O_MARK;
                *b++ = o;
                if (o->o_flags & O_ATOM)
                {
                    o->o_flags &= ~O_ATOM;
                    atom(o, 1);
                }
            }
        }
        objs_top = b;
    }
    else
    {
        /*
         * Faster to delete dead atoms as we go.
         */
        b = objs;
        for (a = objs; a < objs_top; ++a)
        {
            if (((o = *a)->o_flags & O_MARK) == 0)
            {
                if (o->o_flags & O_ATOM)
                    unatom(o);
                freeo(o);
            }
            else
            {
                o->o_flags &= ~O_MARK;
                *b++ = o;
            }
        }
        objs_top = b;
    }

/*printf("mem=%ld vs. %ld, nobjects=%d, natoms=%d\n", mem, ici_mem, objs_top - objs, natoms);*/

    /*
     * Set ici_mem_limit (which is the point at which to trigger a
     * new call to us) to twice what is currently allocated, but
     * with some special cases for small and large.
     */
    if (ici_mem < 0)
        ici_mem = 0;
    if (ici_mem < 16 * 1024)
        ici_mem_limit = 32 * 1024;
    else if (ici_mem > 128 * 1024)
        ici_mem_limit = ici_mem + 128 * 1024;
    else
        ici_mem_limit = ici_mem * 2;

#if 0
    {
        int i;
        for (i = 0; i < 16; ++i)
        {
            if (tc_counts[i] != 0)
                printf("%d:%d ", i, tc_counts[i]);
        }
        printf("\n");
    }
#endif
}

/*
 * Garbage collection triggered by other than our internal mechanmism.
 * Don't do this unless you really must. It will free all memory it can
 * but will reduce subsequent performance.
 */
void
ici_reclaim(void)
{
    collect();
}

#ifdef  BUGHUNT
void
bughunt_incref(object_t *o)
{
    if ((unsigned char)objof(o)->o_nrefs == (unsigned char)0xFF)
    {
        printf("Oops: ref count overflow\n");
        abort();
    }
    if (++objof(o)->o_nrefs > 10)
    {
        printf("Warning: nrefs %d > 10\n", objof(o)->o_nrefs);
        fflush(stdout);
    }
}

void
bughunt_decref(object_t *o)
{
    if (--o->o_nrefs < 0)
    {
        printf("Oops: ref count underflow\n");
        abort();
    }
}
#endif

#ifdef  SMALL
/*
 * If SMALL is defined we use simple functions for these type vectored
 * operations. They are normally macros.
 */

unsigned long
mark(object_t *o)
{
    if (o->o_flags & O_MARK)
        return 0L;
    return (*ici_typeof(o)->t_mark)(o);
}

void
freeo(object_t *o)
{
    (*ici_typeof(o)->t_free)(o);
}

unsigned long
hash(object_t *o)
{
    return (*ici_typeof(o)->t_hash)(o);
}

int
cmp(object_t *o1, object_t *o2)
{
    return (*ici_typeof(o1)->t_cmp)(o1, o2);
}

object_t *
copy(object_t *o)
{
    return (*ici_typeof(o)->t_copy)(o);
}

object_t *
fetch(object_t *o, object_t *k)
{
    return (*ici_typeof(o)->t_fetch)(o, k);
}

int
assign(object_t *o, object_t *k, object_t *v)
{
    return (*ici_typeof(o)->t_assign)(o, k, v);
}

void
rego(object_t *o)
{
    if (objs_top < objs_limit)
        *objs_top++ = o;
    else
        grow_objs(o);
}
#endif
