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
    ici_obj_t   s_head;
    int         s_lineno;
    ici_str_t   *s_filename;
    /*int         s_code_index;*/
};
/*
 * s_filename           The name of the source file this source
 *                      marker is associated with.
 *
 * s_lineno             The linenumber.
 *
 * s_code_index         The index, within the code arrary that this
 *                      source marker if found in, of the first
 *                      instruction this marker applies to.
 */
#define srcof(o)        ((ici_src_t *)o)
#define issrc(o)        ((o)->o_tcode == TC_SRC)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_SRC_H */
