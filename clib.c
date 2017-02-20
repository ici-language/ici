#define ICI_CORE
#include "file.h"
#include "exec.h"
#include "func.h"
#include "op.h"
#include "int.h"
#include "float.h"
#include "str.h"
#include "buf.h"
#ifndef NOSKT
#include "skt.h"
#endif

/*
 * C library and others.  We don't want to go overboard here.  Would
 * like to keep the executable's size under control.
 */

#ifndef _WIN32
extern int      fgetc();
extern int      fputc();
extern int      fflush();
extern int      ungetc();
extern int      fclose();
extern int      pclose();
#endif
extern int      f_sprintf();
static long     xfseek(); /* Below. */
static int      xfeof(); /* Below. */
static int      xfwrite();
#if !defined(NOSYSTEM) && !defined(_WIN32)
extern int      system();
#endif

ftype_t stdio_ftype =
{
    fgetc,
    ungetc,
    fputc,
    fflush,
    fclose,
    xfseek,
    xfeof,
    xfwrite
};

#ifndef NOPIPES
static int
xpclose(FILE *f)
{
    int rc = pclose(f);
    if (rc != 0)
    {
        if (!ici_chkbuf(80))
        {
            sprintf(buf, "popen command exit status %d", rc);
            ici_error = buf;
        }
    }
    return rc;
}

ftype_t  ici_popen_ftype =
{
    fgetc,
    ungetc,
    fputc,
    fflush,
    xpclose,
    xfseek,
    xfeof,
    xfwrite
};
#endif

static int
f_getchar()
{
    file_t              *f;
    int                 c;
    exec_t              *x;

    if (NARGS() != 0)
    {
        if (ici_typecheck("u", &f))
            return 1;
    }
    else
    {
        if ((f = need_stdin()) == NULL)
            return 1;
    }
    ici_signals_blocking_syscall(1);
    x = ici_leave();
    c = (*f->f_type->ft_getch)(f->f_file);
    ici_enter(x);
    ici_signals_blocking_syscall(0);
    if (c == EOF)
    {
        if ((FILE *)f->f_file == stdin)
            clearerr(stdin);
        return null_ret();
    }
    buf[0] = c;
    return ici_ret_with_decref(objof(new_name(buf, 1)));
}

static int
f_getline()
{
    register int        i;
    register int        c;
    register char       *file;
    file_t              *f;
    int                 (*get)();
    exec_t              *x;
    char                *buf;
    int                 buf_size;
    string_t            *str;

    x = NULL;
    if (NARGS() != 0)
    {
        if (ici_typecheck("u", &f))
            return 1;
    }
    else
    {
        if ((f = need_stdin()) == NULL)
            return 1;
    }
    get = f->f_type->ft_getch;
    file = f->f_file;
    if ((buf = malloc(buf_size = 128)) == NULL)
        goto nomem;
    ici_signals_blocking_syscall(1);
    x = ici_leave();
    for (i = 0; (c = (*get)(file)) != '\n' && c != EOF; ++i)
    {
        if (i == buf_size && (buf = realloc(buf, buf_size *= 2)) == NULL)
            break;
        buf[i] = c;
    }
    ici_enter(x);
    ici_signals_blocking_syscall(0);
    if (buf == NULL)
        goto nomem;
    if (i == 0 && c == EOF)
    {
        free(buf);
        if ((FILE *)f->f_file == stdin)
            clearerr(stdin);
        return null_ret();
    }
    str = new_name(buf, i);
    free(buf);
    if (str == NULL)
        return 1;
    return ici_ret_with_decref(objof(str));

nomem:
    ici_error = "ran out of memory";
    return 1;
}

/*
 * ### If given a string argument, interpret it as a file name and
 * do open/get/close.
 */
static int
f_getfile()
{
    register int        i;
    register int        c;
    file_t              *f;
    int                 (*get)();
    char                *file;
    exec_t              *x;
    char                *buf;
    int                 buf_size;
    string_t            *str;

    if (NARGS() != 0)
    {
        if (ici_typecheck("u", &f))
            return 1;
    }
    else
    {
        if ((f = need_stdin()) == NULL)
            return 1;
    }
    get = f->f_type->ft_getch;
    file = f->f_file;
    if ((buf = malloc(buf_size = 128)) == NULL)
        goto nomem;
    ici_signals_blocking_syscall(1);
    x = ici_leave();
    for (i = 0; (c = (*get)(file)) != EOF; ++i)
    {
        if (i == buf_size && (buf = realloc(buf, buf_size *= 2)) == NULL)
            break;
        buf[i] = c;
    }
    ici_enter(x);
    ici_signals_blocking_syscall(0);
    if (buf == NULL)
        goto nomem;
    str = new_name(buf, i);
    free(buf);
    if (str == NULL)
        return 1;
    return ici_ret_with_decref(objof(str));

nomem:
    ici_error = "ran out of memory";
    return 1;
}

static int
f_put()
{
    char                *s;
    file_t              *f;
    exec_t              *x;

    if (NARGS() > 1)
    {
        if (ici_typecheck("su", &s, &f))
            return 1;
    }
    else
    {
        if (ici_typecheck("s", &s))
            return 1;
        if ((f = need_stdout()) == NULL)
            return 1;
    }
    x = ici_leave();
    if
    (
        (*f->f_type->ft_write)(s, stringof(ARG(0))->s_nchars, f->f_file)
        !=
        stringof(ARG(0))->s_nchars
    )
    {
        ici_enter(x);
        ici_error = "write failed";
        return 1;
    }
    ici_enter(x);
    return null_ret();
}

static int
f_fflush()
{
    file_t              *f;
    exec_t              *x;

    if (NARGS() > 0)
    {
        if (ici_typecheck("u", &f))
            return 1;
    }
    else
    {
        if ((f = need_stdout()) == NULL)
            return 1;
    }
    x = ici_leave();
    if ((*f->f_type->ft_flush)(f->f_file) == -1)
    {
        ici_enter(x);
        ici_error = "flush failed";
        return 1;
    }
    ici_enter(x);
    return null_ret();
}

static long
xfseek(stream, offset, whence)
FILE    *stream;
long    offset;
int     whence;
{
    if (fseek(stream, offset, whence) == -1)
    {
        ici_error = "seek failed";
        return -1;
    }
    return ftell(stream);
}

static int
xfeof(stream)
FILE *stream;
{
    return feof(stream);
}

static int
xfwrite(s, n, stream)
char *s;
long n;
FILE *stream;
{
    return fwrite(s, 1, (size_t)n, stream);
}

static int
f_fopen()
{
    char        *name;
    char        *mode;
    file_t      *f;
    FILE        *stream;
    exec_t      *x;

    mode = "r";
    if (ici_typecheck(NARGS() > 1 ? "ss" : "s", &name, &mode))
        return 1;
    x = ici_leave();
    if ((stream = fopen(name, mode)) == NULL)
    {
        ici_enter(x);
        if (ici_chkbuf(strlen(name) + 50))
            return 1;
        sprintf(buf, "could not open \"%s\" (%s)", name, syserr());
        ici_error = buf;
        return 1;
    }
    ici_enter(x);
    if ((f = new_file((char *)stream, &stdio_ftype, stringof(ARG(0)))) == NULL)
    {
        fclose(stream);
        return 1;
    }
    return ici_ret_with_decref(objof(f));
}

static int
f_fseek()
{
    file_t      *f;
    long        offset;
    long        whence;

    if (ici_typecheck("uii", &f, &offset, &whence))
        return 1;
    switch (whence)
    {
    case 0:
    case 1:
    case 2:
        break;
    default:
        ici_error = "invalid whence value in seek()";
        return 1;
    }
    if ((offset = (*f->f_type->ft_seek)(f->f_file, offset, (int)whence)) == -1)
        return 1;
    return int_ret(offset);
}

#ifndef NOPIPES
static int
f_popen()
{
    char        *name;
    char        *mode;
    file_t      *f;
    FILE        *stream;
    exec_t      *x;
    extern int  pclose();

    mode = "r";
    if (ici_typecheck(NARGS() > 1 ? "ss" : "s", &name, &mode))
        return 1;
    x = ici_leave();
    if ((stream = popen(name, mode)) == NULL)
    {
        ici_enter(x);
        if (ici_chkbuf(strlen(name) + 40))
            return 1;
        sprintf(buf, "could not popen \"%s\" (%s)", name, syserr());
        ici_error = buf;
        return 1;
    }
    ici_enter(x);
    if ((f = new_file((char *)stream, &ici_popen_ftype, stringof(ARG(0)))) == NULL)
    {
        pclose(stream);
        return 1;
    }
    return ici_ret_with_decref(objof(f));
}
#endif

#ifndef NOSYSTEM
static int
f_system()
{
    char        *cmd;
    long        result;
    exec_t      *x;

    if (ici_typecheck("s", &cmd))
        return 1;
    x = ici_leave();
    result = system(cmd);
    ici_enter(x);
    return int_ret(result);
}
#endif

static int
f_fclose()
{
    object_t    *o;
    exec_t      *x;
    int         r;

    if (NARGS() != 1)
        return ici_argcount(1);
    o = ARG(0);
    x = ici_leave();
    if (isfile(o))
        r = f_close(fileof(o)) ? 1 : null_ret();
#ifndef NOSKT
    else if (isskt(o))
        r = skt_close(sktof(o)) ? 1 : null_ret();
#endif
    else
        r = ici_argerror(0);
    ici_enter(x);
    return r;
}

static int
f_eof()
{
    file_t              *f;
    exec_t              *x;
    int                 r;

    if (NARGS() != 0)
    {
        if (ici_typecheck("u", &f))
            return 1;
    }
    else
    {
        if ((f = need_stdin()) == NULL)
            return 1;
    }
    x = ici_leave();
    r = int_ret((long)(*f->f_type->ft_eof)(f->f_file));
    ici_enter(x);
    return r;
}

static int
f_remove(void)
{
    char        *s;

    if (ici_typecheck("s", &s))
        return 1;
    if (remove(s) != 0)
    {
        if (ici_chkbuf(strlen(s) + 80))
            return 1;
        sprintf(buf, "could not remove \"%s\" (%s)", s, syserr());
        ici_error = buf;
        return 1;
    }
    return null_ret();
}

cfunc_t clib_cfuncs[] =
{
    {CF_OBJ,    "printf",       f_sprintf,      (int (*)())1},
    {CF_OBJ,    "getchar",      f_getchar},
    {CF_OBJ,    "getfile",      f_getfile},
    {CF_OBJ,    "getline",      f_getline},
    {CF_OBJ,    "fopen",        f_fopen},
#ifndef NOPIPES
    {CF_OBJ,    "popen",        f_popen},
#endif
    {CF_OBJ,    "put",          f_put},
    {CF_OBJ,    "flush",        f_fflush},
    {CF_OBJ,    "close",        f_fclose},
    {CF_OBJ,    "seek",         f_fseek},
#ifndef NOSYSTEM
    {CF_OBJ,    "system",       f_system},
#endif
    {CF_OBJ,    "eof",          f_eof},
    {CF_OBJ,    "remove",       f_remove},

    {CF_OBJ}
};
