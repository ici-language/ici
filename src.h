#ifndef ICI_SRC_H
#define ICI_SRC_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif
/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct ici_src
{
    object_t    s_head;
    int         s_lineno;
    string_t    *s_filename;
};
#define srcof(o)        ((src_t *)o)
#define issrc(o)        ((o)->o_tcode == TC_SRC)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_SRC_H */
