#include "fwd.h"
#include "buf.h"

/*
 * Get a line from the stream into buf.  Returns the number of chars,
 * or -1 on error or end of file.
 */
int
getline(stream)
register FILE	*stream;
{
    register int	i;
    register int	c;

    i = 0;
    while ((c = getc(stream)) != EOF && c != '\n')
    {
	if (chkbuf(i))
	    return -1;
	buf[i++] = c;
    }
    if (c == EOF)
	return -1; /* Trailing \n-less lines ignored, correct. */
    buf[i] = '\0';
    return i;
}
