#ifndef ICI_CONF_H
#define ICI_CONF_H

#include <math.h>

#include <sys/param.h>
#undef isset

#define NOEVENTS        /* Event loop and associated processing. */

#define NOPROFILE       /* Profiler, see profile.c. */
#define NOTRACE         /* For debugging. */

#undef  NOMATH          /* Trig and etc. */
#undef  NOWAITFOR       /* Requires select() or similar system primitive. */
#undef  NOSYSTEM        /* Command interpreter (shell) escape. */
#undef  NOPIPES         /* Requires popen(). */
#undef  NODIR           /* Directory reading function, dir(). */
#undef  NODLOAD         /* Dynamic loading of native machine code modules. */
#undef  NOSTARTUPFILE   /* Parse a standard file of ICI code at init time. */
#undef  NODEBUGGING     /* Debugger interface and functions */
#undef  NOSIGNALS       /* ICI level signal handling */

#define CONFIG_STR "Mac OS X"

#ifndef PREFIX
#define PREFIX  "/usr/local/"
#endif

#define ICI_DLL_EXT     ".dylib"

#if 0
/* Need to wait until sem_init replacement exists */
# define ICI_USE_POSIX_THREADS
#endif

#include <crt_externs.h>
#define environ *_NSGetEnviron()

#endif /*ICI_CONF_H*/
