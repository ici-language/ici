#define ICI_CORE
#include "exec.h"
#include "src.h"

#ifdef NOTDEF
/*
 * Search a the code array the given 'pc' references for the source
 * marker that is just before or at pc->pc_next. Return that source
 * marker (NULL if none) without incref.
 */
ici_src_t *
ici_find_src(ici_pc_t *pc)
{
    ici_src_t           *candidate;
    int                 code_index;
    object_t            **e;

    /*
     * Code arrarys are always stacks, so it is save to do this.
     */
    code_index = pc->pc_next - pc->pc_code->pc_bot;
    candidate = NULL;
    for (e = pc->pc_code->pc_bot; e < pc->pc_code->pc_top; ++e)
    {
        if (!issrc(*e))
            continue;
        if (srcof(*e)->s_code_index >= code_index)
            break;
        candidate = *e;
    }
    return candidate;
}
#endif

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_src(ici_obj_t *o)
{
    long        mem;

    o->o_flags |= O_MARK;
    mem = sizeof(ici_src_t);
    if (srcof(o)->s_filename != NULL)
        mem += ici_mark(srcof(o)->s_filename);
    return mem;
}

ici_src_t *
new_src(int lineno, ici_str_t *filename)
{
    register ici_src_t  *s;

    if ((s = ici_talloc(ici_src_t)) == NULL)
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
free_src(ici_obj_t *o)
{
    ici_tfree(o, ici_src_t);
}

ici_type_t  src_type =
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
