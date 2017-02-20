#define ICI_CORE
#include "fwd.h"
#ifndef _WIN32
#include <unistd.h> /* access(2) prototype */
#else
#include <io.h>
#endif

/*
 * ici_find_on_path - Search for the given file, with the optional extension,
 * on the given path. The path is assumed to uses the local system seperator
 * character. name must point to a buffer of at least FILENAME_MAX chars
 * which will be overwritten with the full file name should it be found.
 * Returns 1 if the expansion was made, else 0, never errors. ext must be
 * less than 10 chars long and include any leading dot.
 */
int
ici_find_on_path(char *path, char name[FILENAME_MAX], char *ext)
{
    char        *p;
    char        *d;
    char        realname[FILENAME_MAX];
    int         xlen;

    xlen = 1 + strlen(name) + (ext != NULL ? strlen(ext) : 0) + 1;
    for (p = path; p != NULL && *p != 0; *p != '\0' ? ++p : 0)
    {
        for
        (
            d = realname;
            *p != '\0'
                && *p != ICI_PATH_SEP
                && d < realname + sizeof realname - xlen;
            *d++ = *p++
        )
            ;
#ifdef _WIN32
        *d++ = '\\';
#else
        *d++ = '/';
#endif
        strcpy(d, name);
        strcpy(d, name);
        if (ext != NULL)
            strcat(d, ext);
        if (access(realname, 0) == 0)
        {
            strcpy(name, realname);
            return 1;
        }
    }
    return 0;
}
