#define ICI_CORE
#include "fwd.h"

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
