#ifndef ICI_CONF_H
#define ICI_CONF_H

#undef	NOMATH		/* Trig and etc. */
#define	NOWIN		/* Lib curses based screen handling. */
#define	NODB		/* Simple ascii file based data base. */
#define	NOTRACE		/* For debugging. */
#define	NOWAITFOR	/* Requires select() or similar system primitive. */
#define	NOSYSTEM	/* Command interpreter (shell) escape. */
#define	NOPIPES		/* Requires popen(). */
#define	NOSKT		/* BSD style network interface. */
#define	NOSYSCALL	/* A few UNIX style system calls. */

/*
 * Mentioned in the version string.
 */
#define	CONFIG_STR	"math"

#endif /*ICI_CONF_H*/
