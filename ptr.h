#ifndef ICI_PTR_H
#define ICI_PTR_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct ici_ptr
{
    object_t    o_head;
    object_t    *p_aggr;        /* The aggregate which contains the object. */
    object_t    *p_key;         /* The key which references it. */
};
#define ptrof(o)        ((ptr_t *)o)
#define isptr(o)        ((o)->o_tcode == TC_PTR)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif  /* ICI_PTR_H */
