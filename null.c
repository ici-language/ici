#define ICI_CORE
#include "null.h"

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_null(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof(null_t);
}

type_t  null_type =
{
    mark_null,
    free_simple,
    hash_unique,
    cmp_unique,
    copy_simple,
    ici_assign_fail,
    ici_fetch_fail,
    "NULL"
};

null_t  o_null  = {{TC_NULL, O_ATOM, 1, 0}};
