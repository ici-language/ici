#ifndef ICI_SKT_H
#define ICI_SKT_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif
/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

#ifdef _WIN32
/*
 * Windows uses a special type to represent its SOCKET descriptors.
 * For correctness we include winsock.h here. Other platforms (i.e.,
 * BSD) are easier and use integers.
 */
#include <winsock.h>
#else
#define SOCKET  int
#endif

struct skt
{
    object_t    o_head;
    SOCKET      s_skt;
    int         s_closed;
};
#define sktof(o)        ((skt_t *)(o))
#define isskt(o)        (objof(o)->o_tcode == TC_SOCKET)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_SKT_H */
