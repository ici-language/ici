#define ICI_CORE
#include "fwd.h"
#ifndef NOEVENTS
#include "exec.h"
#include "func.h"

#ifdef  _WIN32
#include <windows.h>
/*
 * Win32 specific event processing.
 */

static int
f_eventloop()
{
    MSG         msg;
    exec_t      *x;

    x = ici_leave();
    for (;;)
    {
        switch (GetMessage(&msg, NULL, 0, 0))
        {
        case 0:
            ici_enter(x);
            return null_ret();

        case -1:
            ici_enter(x);
            return ici_get_last_win32_error();
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

#endif /* _WIN32 */

cfunc_t ici_event_cfuncs[] =
{
    {CF_OBJ,    "eventloop",       f_eventloop},
    {CF_OBJ}
};
#endif /* NOEVENTS */
