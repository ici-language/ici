#ifndef ICI_PC_H
#define ICI_PC_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct pc
{
    object_t    o_head;
    array_t     *pc_code;
    object_t    **pc_next;
};
#define pcof(o)         ((pc_t *)(o))
#define ispc(o)         ((o)->o_tcode == TC_PC)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif  /* ICI_PC_H */
