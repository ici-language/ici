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
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_file(object_t *o)
{
    return (unsigned long)fileof(o)->f_file * FILE_PRIME
        + (unsigned long)fileof(o)->f_type;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_file(object_t *o)
{
    if (!(o->o_flags & F_CLOSED))
        f_close(fileof(o));
    ici_tfree(o, file_t);
}

/*
 * Return a file object with the given ftype and a type specific
 * pointer fp which is often somethins like a STREAM * or a file
 * descriptor. The name is mostly for error messages and stuff.
 * The returned object has a ref count of 1. Return NULL on error.
 * Note that files are intrinsically atomic.
 */
file_t *
new_file(char *fp, ftype_t *ftype, string_t *name)
{
    register file_t     *f;
    static file_t       proto = {OBJ(TC_FILE)};

    proto.f_file = fp;
    proto.f_type = ftype;
    proto.f_name = name;
    if ((f = fileof(atom_probe(objof(&proto)))) != NULL)
    {
        incref(f);
        return f;
    }
    if ((f = ici_talloc(file_t)) == NULL)
        return NULL;
    *f = proto;
    rego(f);
    return fileof(atom(objof(f), 1));
}

int
f_close(file_t *f)
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
        mem += mark(objof(fileof(o)->f_name));
    return mem;
}

type_t  file_type =
{
    mark_file,
    free_file,
    hash_file,
    cmp_file,
    copy_simple,
    assign_simple,
    fetch_simple,
    "file"
};
