#define ICI_CORE
#include <fwd.h>
#ifndef BSD
#include <errno.h>
# ifdef hpux
/*
 * HPUX only defines them for C++
 */
extern int      errno;
extern int      sys_nerr;
extern char     *sys_errlist[];
# endif /* hpux */
#else /* BSD */

# ifdef sun
/*
 * Suns don't define these in any include files...
 */
extern int      errno;
extern int      sys_nerr;
extern char     *sys_errlist[];
# endif

# if defined(__APPLE__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__linux__)
#  include <errno.h>
# endif

#endif

char *
syserr(void)
{
#ifndef NOSYSERR
    if (errno < sys_nerr)
        return (char *)sys_errlist[errno];
#endif
    return "system call failure";
}
