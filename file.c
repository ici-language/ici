#define ICI_CORE
#include "file.h"
#include "primes.h"

/*
 * Returns 0 if these objects are eq, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_file(object_t *o1, object_t *o2)
{
    return fileof(o1)->f_file != fileof(o2)->f_file
        || fileof(o1)->f_type != fileof(o2)->f_type
        || (o1->o_flags & F_CLOSED) != (o2->o_flags & F_CLOSED);
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_file(object_t *o)
{
    if ((o->o_flags & F_CLOSED) == 0)
    {
        if (o->o_flags & F_NOCLOSE)
            (*fileof(o)->f_type->ft_flush)(fileof(o)->f_file);
        else
            ici_file_close(fileof(o));
    }
    ici_tfree(o, file_t);
}

/*
 * Return a file object with the given ftype and a type specific
 * pointer fp which is often somethins like a STREAM * or a file
 * descriptor. The name is mostly for error messages and stuff.
 * The returned object has a ref count of 1. Return NULL on error.
 * Note that files are intrinsically atomic.
 *
 * The ref argument is an object reference that the file object
 * will keep in case the fp argument is an implicit reference into
 * some object. It may be NULL if not required.
 */
file_t *
ici_file_new(void *fp, ftype_t *ftype, string_t *name, object_t *ref)
{
    register file_t     *f;

    if ((f = ici_talloc(file_t)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(f, TC_FILE, 0, 1, 0);
    f->f_file = fp;
    f->f_type = ftype;
    f->f_name = name;
    f->f_ref = ref;
    ici_rego(f);
    return f;
}

int
ici_file_close(file_t *f)
{
    if (objof(f)->o_flags & F_CLOSED)
    {
        ici_error = "file already closed";
        return 1;
    }
    objof(f)->o_flags |= F_CLOSED;
    return (*f->f_type->ft_close)(f->f_file);
}

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_file(object_t *o)
{
    long        mem;

    o->o_flags |= O_MARK;
    mem = sizeof(file_t);
    if (fileof(o)->f_name != NULL)
        mem += ici_mark(objof(fileof(o)->f_name));
    if (fileof(o)->f_ref != NULL)
        mem += ici_mark(objof(fileof(o)->f_ref));
    return mem;
}

type_t  file_type =
{
    mark_file,
    free_file,
    ici_hash_unique,
    cmp_file,
    ici_copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "file"
};
