#define ICI_CORE
#include "wrap.h"

#error wrap.c is obsolete. Please remove from your make.

/*
 * Call functions that have been queued to be executed on interpreter
 * shutdown.
 */
void
wrapup(void)
{
    while (wraps != NULL)
    {
        (*wraps->w_func)();
        wraps = wraps->w_next;
    }
}
