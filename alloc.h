#ifndef ICI_ALLOC_H
#define ICI_ALLOC_H

/*
 * Define this to 1 to prevent the use of fast free lists. All
 * allocations go to the native malloc. We allow it to be set
 * in the configuration file - but that would be rare. It will
 * slow performance considerably. Can be very useful to set,
 * along with ALLCOLLECT in object.c, during debug and test.
 */
#if     !ICI_ALLALLOC
#define ICI_ALLALLOC    0   /* Always call malloc, no caches. */
#define ICI_RAWMALLOC   0   /* If ALLALLOC, just use malloc. */
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

/*
 * Is an object of this type of a size suitable for one of the
 * fast free lists?
 */
#define ICI_FLOK(t)     (sizeof(t) <= 64)

/*
 * Determine what free list an object of the given type is appropriate
 * for, or the size of the object if it is too big. We assume the
 * compiler will reduce this constant expression to a constant at
 * compile time.
 */
#define ICI_FLIST(t)    (sizeof(t) <=  8 ? 0  \
                        : sizeof(t) <= 16 ? 1 \
                        : sizeof(t) <= 32 ? 2 \
                        : sizeof(t) <= 64 ? 3 \
                        : sizeof(t))

/*
 * Allocate an object of the given type. Return NULL on failure, usual
 * conventions. The resulting object *must* be freed with ici_tfree().
 * Note that ici_tfree() also requires to know the type of the object
 * being freed.
 *
 * If the object is too big for a fast free list, this macro should
 * reduce to a simple function call. If it is small, it will reduce
 * to an attempt to pop a block off the correct fast free list, but
 * call the function if the list is empty.
 */
#if     !ICI_ALLALLOC

#   define ici_talloc(t)   \
    (ICI_FLOK(t) && (ici_fltmp = ici_flists[ICI_FLIST(t)]) != NULL  \
        ? (ici_flists[ICI_FLIST(t)] = *(char **)ici_fltmp,          \
            ici_mem += sizeof(t),                                   \
            ici_fltmp)                                              \
        : ici_talloc_work(ICI_FLIST(t), sizeof(t)))

#   define ici_tfree(p, t) \
    (ICI_FLOK(t)                                        \
        ? (*(char **)(p) = ici_flists[ICI_FLIST(t)],    \
            ici_flists[ICI_FLIST(t)] = (char *)(p),     \
            ici_mem -= sizeof(t))                       \
        : ici_nfree((p), sizeof(t)))

#elif !ICI_RAWMALLOC

#   define ici_talloc(t)   ici_talloc_work(ICI_FLIST(t), sizeof(t))
#   define ici_tfree(p, t) ici_nfree((p), sizeof(t))

#else

#   define ici_talloc(t)    (t *)malloc(sizeof(t))
#   define ici_tfree(p, t)  free(p)
#   define ici_nalloc       malloc
#   define ici_nfree(p, z)  free(p)
#   define ici_alloc        malloc 
#   define ici_free         free 

#endif  /* ICI_ALLALLOC */

extern DLI char         *ici_flists[4];
extern DLI char         *ici_fltmp;
extern DLI long         ici_mem;
extern long             ici_mem_limit;

#if !ICI_RAWMALLOC
extern void             *ici_talloc_work(int fi, size_t z);
extern void             *ici_nalloc(size_t z);
extern void             ici_nfree(void *p, size_t z);
extern void             *ici_alloc(size_t z);
extern void             ici_free(void *p);
#endif

/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_ALLOC_H */
