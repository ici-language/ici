#ifndef ICI_FLOAT_H
#define ICI_FLOAT_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct ici_float
{
    ici_obj_t   o_head;
    double      f_value;
};
#define floatof(o)      ((ici_float_t *)o)
#define isfloat(o)      ((o)->o_tcode == TC_FLOAT)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_FLOAT_H */
