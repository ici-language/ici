#ifndef ICI_CFUNC_H
#define ICI_CFUNC_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct cfunc
{
    object_t    o_head;
    char        *cf_name;
    int         (*cf_cfunc)();
    void        *cf_arg1;
    void        *cf_arg2;
};
#define cfuncof(o)      ((cfunc_t *)(o))
#define iscfunc(o)      (objof(o)->o_tcode == TC_CFUNC)

/*
 * Convienience macro for the object header for use in static
 * initialisations of cfunc_t objects.
 */
#define CF_OBJ          {TC_CFUNC, 0, 1, 0}

/*
 * The operand stack on entry to an intrinsic function:
 *
 * arg(n-1) ... arg(1) arg(0) NARGS FUNC
 *                                        ^-ici_os.a_top
 *
 * NARGS is an ICI int and FUNC is the function object (us).
 */

/*
 * The i-th argument (first is 0) during execution of a
 * function's C code.
 */
#define ARG(n)          (ici_os.a_top[-3 - (n)])

/*
 * Count of actual arguments to this C function.
 */
#define NARGS()         ((int)intof(ici_os.a_top[-2])->i_value)

/*
 * A pointer to the first arg to this C function, decrement for next
 * and subsequent.
 */
#define ARGS()          (&ici_os.a_top[-3])

/*
 * Return the cf_arg1 and cf_arg2 fields of the current C function.
 * The first is a function pointer, the second a char *.
 */
#define CF_ARG1()       (cfuncof(ici_os.a_top[-1])->cf_arg1)
#define CF_ARG2()       (cfuncof(ici_os.a_top[-1])->cf_arg2)

/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_CFUNC_H */
