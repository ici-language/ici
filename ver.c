/*
 * All this file does is define a string to identify the version number
 * of the interpeter. Update for new releases.
 */
#include "fwd.h"

#if defined(__STDC__)
/*
 * Eg: @(#)ICI 2.0, conf-sco.h, Apr 20 1994, math win waitfor system popen
 */
char ici_version_string[] = "@(#)ICI 2.0, " CONFIG_FILE ", " __DATE__
			    ", " CONFIG_STR;
#else
char ici_version_string[] = "@(#)ICI 2.0";
#endif
