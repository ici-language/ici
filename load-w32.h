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
 * Windows version of the get_dll_path() function. Returns the seach
 * path on which ICI modules will be searched for. We try to mimic the
 * search behaviour of LoadLibrary().
 */
char *
ici_get_dll_path(void)
{
    static char     *dll_path;
    char            *env_path;
    char            mod_path[MAX_PATH];
    int             size;
    int             sdz;
    int             wdz;
    char            *p;
    
    if (dll_path != NULL)
        return dll_path;
        
    GetModuleFileName(NULL, mod_path, sizeof mod_path);
    if ((p = strrchr(mod_path, '\\')) == NULL)
        p = mod_path;
    *p = '\0';
    if ((env_path = getenv("PATH")) == NULL)
        env_path = "";
    size = strlen(mod_path)
        + 1
        + (sdz = GetSystemDirectory(NULL, 0))
        + (wdz = GetWindowsDirectory(NULL, 0))
        + strlen(env_path);
        
    if ((dll_path = malloc(size + 10)) == NULL)
        goto fail;
    p = dll_path;
    p += sprintf(p, "%s;.;", mod_path);
    p += GetSystemDirectory(p, sdz + 1);
    *p++ = ';';
    p += GetWindowsDirectory(p, wdz + 1);
    *p++ = ';';
    strcpy(p, env_path);
    return dll_path;

fail:
    return ".";
}

#endif /* ICI_LOAD_W32_H */
