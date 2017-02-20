#define ICI_CORE
#include "fwd.h"
#include "buf.h"

#ifdef  _WIN32
#include <windows.h>

/*
 * Convert the current gdi error into an ICI error message.
 * Returns 1 so it can be use directly in a return from an
 * ICI instrinsic function.
 */
int
ici_get_last_win32_error(void)
{
    FormatMessage
    (
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        ici_buf,
        ici_bufz,
        NULL
    );
    ici_error = ici_buf;
    return 1;
}
#endif /* _WIN32 */
