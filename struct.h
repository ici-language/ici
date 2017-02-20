#ifndef ICI_STRUCT_H
#define ICI_STRUCT_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif
/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct slot
{
    object_t    *sl_key;
    object_t    *sl_value;
};

struct structs
{
    objwsup_t   o_head;
    int         s_nels;         /* How many slots used. */
    int         s_nslots;       /* How many slots allocated. */
    slot_t      *s_slots;
};
#define structof(o)     ((struct_t *)(o))
#define isstruct(o)     (objof(o)->o_tcode == TC_STRUCT)

/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_STRUCT_H */
