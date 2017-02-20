#ifndef ICI_WRAP_H
#define ICI_WRAP_H

#ifndef ICI_FWD_H
#include <fwd.h>
#endif
/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct wrap
{
    wrap_t      *w_next;
    int         (*w_func)();
};

extern DLI wrap_t       *wraps;
/*
 * End of ici.h export. --ici.h-end--
 */
#endif
