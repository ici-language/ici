#ifndef ICI_FUNC_H
#define ICI_FUNC_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct ici_func
{
    object_t    o_head;
    array_t     *f_code;        /* The code of this function, atom. */
    array_t     *f_args;        /* Array of argument names. */
    struct_t    *f_autos;       /* Prototype struct of autos (incl. args). */
    string_t    *f_name;        /* Some name for the function (diagnostics). */
    int         f_nautos;       /* If !=0, a hint for auto struct alloc. */
};

#define funcof(o)       ((func_t *)(o))
#define isfunc(o)       (objof(o)->o_tcode == TC_FUNC)

/*
 * End of ici.h export. --ici.h-end--
 */

#ifndef ICI_CFUNC_H
/*
 * We'd rather not do this, but lots of code expects it. So for backwards
 * compatibility, we'll also include cfunc.h.
 */
#include "cfunc.h"
#endif

#endif /* ICI_FUNC_H */
