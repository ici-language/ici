#ifndef ICI_METHOD_H
#define ICI_METHOD_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct method
{
    object_t    o_head;
    object_t    *m_subject;
    object_t    *m_callable;
};
#define methodof(o)     ((method_t *)(o))
#define ismethod(o)     (objof(o)->o_tcode == TC_METHOD)

/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_CFUNC_H */
