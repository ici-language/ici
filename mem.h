#ifndef ICI_MEM_H
#define ICI_MEM_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct ici_mem
{
    object_t            o_head;
    void                *m_base;
    size_t              m_length;       /* In m_accessz units. */
    int                 m_accessz;      /* Read/write size. */
    void                (*m_free)();
};

#define memof(o)        ((mem_t *)o)
#define ismem(o)        (objof(o)->o_tcode == TC_MEM)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_MEM_H */
