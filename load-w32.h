#ifndef ICI_LOAD_W32_H
#define ICI_LOAD_W32_H

#ifndef NODLOAD

#include <windows.h>

typedef void    *dll_t;

#define valid_dll(dll)  ((dll) != NULL)

static dll_t
dlopen(const char *name, int mode)
{
    return LoadLibrary(name);
}

static void *
dlsym(dll_t hinst, const char *name)
{
    return GetProcAddress(hinst, name);
}

static char *
dlerror(void)
{
    static char     msg[80];

    FormatMessage
    (
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msg,
        sizeof msg,
        NULL
    );
    return msg;
}

static void
dlclose(dll_t hinst)
{
}

#endif /* NODLOAD */

/*
 * Push path elements specific to Windows onto the array a (which is the ICI
 * path array used for finding dynamically loaded modules and stuff). These
 * are in addition to the ICIPATH environment variable. We try to mimic
 * the search behaviour of LoadLibrary() (that being the Windows thing to
 * do).
 */
static int
push_os_path_elements(array_t *a)
{
    char                fname[MAX_PATH];
    char                *p;

    if (GetModuleFileName(NULL, fname, sizeof fname - 10) > 0)
    {
        if ((p = strrchr(fname, ICI_DIR_SEP)) != NULL)
            *p = '\0';
        if (push_path_elements(a, fname))
            return 1;
        p = fname + strlen(fname);
        *p++ = ICI_DIR_SEP;
        strcpy(p, "ici");
        if (push_path_elements(a, fname))
            return 1;
    }
    if (push_path_elements(a, "."))
        return 1;
    if (GetSystemDirectory(fname, sizeof fname - 10) > 0)
    {
        if (push_path_elements(a, fname))
            return 1;
        p = fname + strlen(fname);
        *p++ = ICI_DIR_SEP;
        strcpy(p, "ici");
        if (push_path_elements(a, fname))
            return 1;
    }
    if (GetWindowsDirectory(fname, sizeof fname - 10) > 0)
    {
        if (push_path_elements(a, fname))
            return 1;
        p = fname + strlen(fname);
        *p++ = ICI_DIR_SEP;
        strcpy(p, "ici");
        if (push_path_elements(a, fname))
            return 1;
    }
    if ((p = getenv("PATH")) != NULL)
    {
        if (push_path_elements(a, p))
            return 1;
    }
    return 0;
}

#endif /* ICI_LOAD_W32_H */
