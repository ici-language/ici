#ifndef ICI_WRAP_H
#define ICI_WRAP_H

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct wrap
{
    wrap_t      *w_next;
    void        (*w_func)(void);
};

/*
 * End of ici.h export. --ici.h-end--
 */
#endif
