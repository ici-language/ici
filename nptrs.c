#define ICI_CORE
#include "fwd.h"

#error nptrs.c is obsolete. Please remove from your make.

/*
 * Return the number of pointers in a NULL terminated array of pointers.
 */
int
nptrs(char **p)
{
    register int        i;

    i = 0;
    while (*p++ != NULL)
        ++i;
    return i;
}
