#ifndef ICI_OBJECT_H
#define ICI_OBJECT_H

#ifndef ICI_FWD_H
#include "fwd.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

#define ICI_MAX_TYPES   64

extern DLI type_t       *ici_types[ICI_MAX_TYPES];

#define ICI_OBJNAMEZ    30

/*
 * Every object has a header. In the header the o_tcode (type code) field
 * can be used to index the ici_types[] array to discover the obejct's
 * type structure. This is the type structure. See detailed comments below.
 */
struct type
{
    unsigned long (*t_mark)(object_t *);
    void        (*t_free)(object_t *);
    unsigned long (*t_hash)(object_t *);
    int         (*t_cmp)(object_t *, object_t *);
    object_t    *(*t_copy)(object_t *);
    int         (*t_assign)(object_t *, object_t *, object_t *);
    object_t    *(*t_fetch)(object_t *, object_t *);
    char        *t_name;
    void        (*t_objname)(object_t *, char [ICI_OBJNAMEZ]);
    int         (*t_call)(object_t *, object_t *);
    string_t    *t_ici_name;
    int         (*t_assign_super)(object_t *, object_t *, object_t *, struct_t *);
    int         (*t_fetch_super)(object_t *, object_t *, object_t **, struct_t *);
    int         (*t_assign_base)(object_t *, object_t *, object_t *);
    object_t    *(*t_fetch_base)(object_t *, object_t *);
    void        *t_reserved1;   /* MBZ. Probably forall generalisation. */
    void        *t_reserved2;   /* Must be zero. */
    void        *t_reserved3;   /* Must be zero. */
    void        *t_reserved4;   /* Must be zero. */
};
/*
 * t_mark(o)    Sets the O_MARK flag in o->o_flags of this object and
 *              all objects referenced by this one which don't already
 *              have O_MARK set. Returns the approximate memory cost
 *              of this and all other objects it sets the O_MARK of.
 *              Typically recurses on all referenced objects which
 *              don't already have O_MARK set (this recursion is a
 *              potential problem due to the uncontrolled stack depth
 *              it can create). This is used in the marking phase of
 *              garbage collection.
 *
 *              The macro ici_mark() calls the t_mark function of the object
 *              (based on object type) if the O_MARK flag of the object
 *              is clear, else it returns 0. This is the usual interface
 *              to an object's mark function.
 *
 *              The mark function implemantation of objects can assume
 *              the O_MARK flag of the object they are being invoked
 *              on is clear.
 *
 * t_free(o)    Frees the object o and all associated data, but not
 *              other objects which are referenced from it. This is only
 *              called from garbage collection. Care should be taken
 *              to remember that errors can occur during object creation
 *              and that the free function might be asked to free a
 *              partially allocated object.
 *
 *              Simple object types which just have to free their data
 *              can use the existing function free_simple() as their
 *              implementation of this function.
 *
 * t_cmp(o1, o2) Compare o1 and o2 and return 0 if they are the same, else
 *              non zero. This similarity is the basis for merging objects
 *              into single atomic objects and the implementation of the ==
 *              operator.
 *
 *              Some objects are by nature both unique and intrinsically
 *              atomic (for example, objects which are one-to-one with
 *              some other allocated data which they alloc when the are
 *              created and free when they die). For these objects the
 *              existing function cmp_unique() can be used as their
 *              implementation of this function.
 *
 *              It is very important in implementing this function not to
 *              miss any fields which may otherwise distinguish two obejcts.
 *              The cmp, hash and copy operations of an object are all
 *              related. It is useful to check that they all regard the
 *              same data fields as significant in performing their
 *              operation.
 *
 * t_copy(o)    Returns a copy of the given object. This is the basis for
 *              the implementation of the copy() function. On failure, NULL
 *              is returned and error is set. The returned object has been
 *              ici_incref'ed. The returned object should cmp() as being equal,
 *              but be a distinct object for objects that are not
 *              intrinsically atomic.
 *
 *              Intrinsically atomic objects may use the existing function
 *              copy_simple() as their implemenation of this function.
 *
 * t_hash(o)    Return an unsigned long hash which is sensitive to the
 *              value of the object. Two objects which cmp() equal should
 *              return the same hash.
 *
 *              The returned hash is used in a hash table shared by objects
 *              of all types. So, somewhat problematically, it is desireable
 *              to generate hashes which have good spread and seperation
 *              across all objects.
 *
 *              Some objects are by nature both unique and intrinsically
 *              atomic (for example, objects which are one-to-one with
 *              some other allocated data which they alloc when the are
 *              created and free when they die). For these objects the
 *              existing function hash_unique() can be used as their
 *              implementation of this function.
 *
 * t_assign(o, k, v) Assign to key k of the object o the value v. Return
 *              1 on error, else 0.
 *
 *              The existing function ici_assign_fail() may be used both as
 *              the implementation of this function for object types which
 *              do not support any assignment, and as a simple method of
 *              generating an error for particular assignments which break
 *              some rule of the object.
 *
 *              Not that it is not necessarilly wrong for an intrinsically
 *              atomic object to support some form of assignment. Only for
 *              the modified field to be significant in a cmp() operation.
 *              Objects which are intrinsically unique and atomic often
 *              support assignments.
 *
 * t_fetch(o, k) Fetch the value of key k of the object o.  Return NULL on
 *              error.
 *              
 *              Note that the returned object does not have any extra
 *              reference count; however, in some circumstances it may not
 *              have any garbage collector visible references to it.  That is,
 *              it may be vunerable to a garbage collection if it is not
 *              either incref()ed or hooked into a referenced object
 *              immediately. Callers are responsible for taking care.
 *
 *              The existing function ici_fetch_fail() may be used both as the
 *              implementation of this function for object types which do not
 *              support any assignment, and as a simple method of generating
 *              an error for particular fetches which break some rule of the
 *              object.
 *
 * t_name       The name of this type. Use for the implementation of
 *              typeof() and in error messages. But apart from that, type
 *              names have no fundamental importance in the langauge and
 *              need not even be unique.
 *
 * t_objname(o, p) Place a short (30 chars or less) human readable
 *              representation of the object in the given buffer. This
 *              is not intended as a basis for re-parsing or serialisation.
 *              It is just for diagnostics and debug. An implementation of
 *              t_objname() must not allocate memory or otherwise allow the
 *              garbage collector to run. It is often used to generate
 *              formatted failure messages after an error has occured, but before
 *              cleanup has completed.
 *
 * t_call(o, s) Call the object o. If s is non-NULL this is a method call
 *              and s is the subject object of the call. Return 1 on error,
 *              else 0.
 *
 * t_ici_name   A string_t copy of the name. This is just a cached version
 *              so that typeof() doesn't keep re-computing the string.
 *
 */

/*
 * Macros to perform the operation on the object.
 */
/*
 * ici_mark()
 *
 * The recursive traversal of all objects performed by marking is particularly
 * expensive. So we take pains to cut short function calls wherever possible.
 * Thus the size of this macro. The o_leafz field of an object tells us it
 * doesn't reference any other objects and is of small (ie o_leafz) size.
 */
#define ici_mark(o)         ((objof(o)->o_flags & O_MARK) == 0 \
                            ? (objof(o)->o_leafz != 0 \
                                ? (objof(o)->o_flags |= O_MARK, objof(o)->o_leafz) \
                                : (*ici_typeof(o)->t_mark)(objof(o))) \
                            : 0L)
#define freeo(o)        ((*ici_typeof(o)->t_free)(objof(o)))
#define hash(o)         ((*ici_typeof(o)->t_hash)(objof(o)))
#define cmp(o1,o2)      ((*ici_typeof(o1)->t_cmp)(objof(o1), objof(o2)))
#define ici_fetch(o,k)      ((*ici_typeof(o)->t_fetch)(objof(o), objof(k)))
#define copy(o)         ((*ici_typeof(o)->t_copy)(objof(o)))
#define ici_assign(o,k,v)   ((*ici_typeof(o)->t_assign)(objof(o), objof(k), objof(v)))
#define assign_super(o,k,v,b) ((*ici_typeof(o)->t_assign_super)(objof(o), objof(k), objof(v), b))
#define fetch_super(o,k,v,b) ((*ici_typeof(o)->t_fetch_super)(objof(o), objof(k), v, b))
#define assign_base(o,k,v) ((*ici_typeof(o)->t_assign_base)(objof(o), objof(k), objof(v)))
#define fetch_base(o,k) ((*ici_typeof(o)->t_fetch_base)(objof(o), objof(k)))

#ifndef BUGHUNT
/*
 * Link an object into the list of objects. We clear the o_leafz field
 * here because that is the safe thing to do and, as of the introduction
 * of this field, much code does not set this field to zero. Objects
 * that want to take advantage of o_leafz must set it after calling rego().
 */
#define rego(o)         (objof(o)->o_leafz = 0, \
                          (objs_top < objs_limit \
                            ? (void)(*objs_top++ = objof(o)) \
                            : grow_objs(objof(o))))
#else
#define rego(o)         bughunt_rego(objof(o))
extern void             bughunt_rego(object_t *);
#endif

#if 0
/*
 * Functions to performs operations on the object.
 */
extern unsigned long    ici_mark(object_t *);
extern void             freeo(object_t *);
extern unsigned long    hash(object_t *);
extern int              cmp(object_t *, object_t *);
extern object_t         *copy(object_t *);
extern object_t         *ici_fetch(object_t *, object_t *);
extern int              ici_assign(object_t *, object_t *, object_t *);
#endif

#define ici_atom_hash_index(h)  (ICI_PTR_HASH_BITS(h) & (atomsz - 1))


/*
 * References from ordinary machine data objects (ie. variables and stuff,
 * not other objects) are invisible to the garbage collector.  These refs
 * must be accounted for if there is a possibility of garbage collection.
 * Note that most routines that make objects (new_*(), copy() etc...)
 * return objects with 1 ref.  The caller is expected to ici_decref() it when
 * they attach it into wherever it is going.
 */
#ifndef BUGHUNT
#define ici_incref(o)       (++objof(o)->o_nrefs)
#define ici_decref(o)       (--objof(o)->o_nrefs)
#else
void bughunt_incref(object_t *o);
void bughunt_decref(object_t *o);
#define ici_incref(o) bughunt_incref(objof(o))
#define ici_decref(o) bughunt_decref(objof(o))
#endif

/*
 * This is the universal header of all objects.
 */
struct object
{
    char        o_tcode;        /* See TC_* below. */
    char        o_flags;        /* See O_* below. */
    char        o_nrefs;        /* No. of refs invisible to collect. */
    char        o_leafz;        /* Iff a leaf object, its size, else 0. */
    /*
     * Each object includes this as a header.  In the real structures
     * associated with each object type the type specific stuff follows...
     */
};
#define objof(x)        ((object_t *)(x))

/*
 * "Object with super." This is a specialised header for all objects
 * that support a super pointer. All such objects must have the
 * O_SUPER flag set in o_flags and provide the t_fetch_super() and
 * t_assign_super() functions.
 */
struct objwsup
{
    object_t    o_head;         /* Universal header. */
    objwsup_t   *o_super;       /* Out super. May be NULL. */
    /*
     * Each object that supports a super type includes this as a
     * header. In the real structures associated with each such object
     * the type specific stuff follows...
     */
};
#define objwsupof(o)    ((objwsup_t *)(o))
/*
 * This object supports a super type. (It may or may not have a super
 * at any particular time).
 */
#define hassuper(o)     (objof(o)->o_flags & O_SUPER)

#define ici_typeof(o)   (ici_types[(int)objof(o)->o_tcode])

/*
 * For static object initialisations...
 */
#define OBJ(tc)    {(tc), 0, 1, 0}

/*
 * Flags that may appear in o_flags. The upper nibble is considered available
 * for type specific use.
 */
#define O_MARK          0x01    /* Garbage collection mark. */
#define O_ATOM          0x02    /* Is a member of the atom pool. */
#define O_TEMP          0x04    /* Is a re-usable temp (flag for asserts). */
#define O_SUPER         0x08    /* Has super (is objwsup_t derived). */

/*
 * The o_tcode field is a small int which allows bypassing the o_type
 * pointer for some types.  Any type not listed here will have a value
 * of 0.  The types listed will set a code here to allow some time
 * critical areas of code to make quicker decisions (typically a switch)
 * based on the type.  It also allows faster decisions based on type
 * combinations (see PAIR() and TRI() below).
 */
#define TC_OTHER        0
#define TC_PC           1
#define TC_SRC          2
#define TC_PARSE        3
#define TC_OP           4
#define TC_STRING       5
#define TC_CATCH        6
#define TC_FORALL       7
#define TC_INT          8
#define TC_FLOAT        9
#define TC_REGEXP       10
#define TC_PTR          11
#define TC_ARRAY        12
#define TC_STRUCT       13
#define TC_SET          14

#define TC_MAX_BINOP    14

#define TC_EXEC         15
#define TC_FILE         16
#define TC_FUNC         17
#define TC_CFUNC        18
#define TC_METHOD       19
#define TC_MARK         20
#define TC_NULL         21
#define TC_HANDLE       22
#define TC_MEM          23

#define TC_MAX_CORE     24

/* Max value of 15 dictated by PAIR (below). */

#define TRI(a,b,t)      (((((a) << 4) + b) << 6) + t_subtype(t))

#define isfalse(o)      ((o) == objof(ici_zero) || (o) == objof(&o_null))

/*
 * End of ici.h export. --ici.h-end--
 */
#endif
