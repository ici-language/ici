#ifndef ICI_INT_H
#define ICI_INT_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct ici_int
{
    object_t    o_head;
    long        i_value;
};
#define intof(o)        ((int_t *)(o))
#define isint(o)        ((o)->o_tcode == TC_INT)
/*
 * End of ici.h export. --ici.h-end--
 */

#define ICI_SMALL_INT_MASK  0x1F
extern int_t                *ici_small_ints[32];

#endif /* ICI_INT_H */
