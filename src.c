#define ICI_CORE
#include "exec.h"
#include "src.h"

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_src(object_t *o)
{
    long        mem;

    o->o_flags |= O_MARK;
    mem = sizeof(src_t);
    if (srcof(o)->s_filename != NULL)
        mem += ici_mark(srcof(o)->s_filename);
    return mem;
}

src_t *
new_src(int lineno, string_t *filename)
{
    register src_t      *s;

    if ((s = ici_talloc(src_t)) == NULL)
        return NULL;
    ICI_OBJ_SET_TFNZ(s, TC_SRC, 0, 1, 0);
    s->s_lineno = lineno;
    s->s_filename = filename;
    ici_rego(s);
    return s;
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_src(object_t *o)
{
    ici_tfree(o, src_t);
}

type_t  src_type =
{
    mark_src,
    free_src,
    ici_hash_unique,
    ici_cmp_unique,
    ici_copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "src"
};
