#define ICI_CORE
#include "ptr.h"
#include "exec.h"
#include "file.h"
#include "str.h"
#include "struct.h"
#include "buf.h"
#include "wrap.h"
#include "func.h"
#ifndef NOPROFILE
#include "profile.h"
#endif

int
ici_main(int argc, char *argv[])
#ifndef NODEBUGGING
{
    int ici_maind(int, char *[], int);
    return ici_maind(argc, argv, 0);
}

int
ici_maind(int argc, char *argv[], int debugging)
#endif
{
    int                 i;
    int                 j;
    char                *s;
    char                *fmt;
    char                *arg0;
    ici_array_t         *av;
    FILE                *stream;
    ici_file_t          *f;

    if (ici_init())
        goto fail;

    /*
     * Process arguments.  Two pass, first gather "unused" arguments,
     * ie, arguments which are passed into the ICI code.  Stash these in
     * the array av.  NB: must be in sync with the second pass below.
     */
    if ((av = ici_array_new(1)) == NULL)
        goto fail;
    *av->a_top++ = objof(&o_null); /* Leave room for argv[0]. */
    arg0 = NULL;
    if (argc > 1 && argv[1][0] != '-')
    {
        /*
         * Usage1: ici file [args...]
         */
        arg0 = argv[1];
        for (i = 2; i < argc; ++i)
        {
            if (ici_stk_push_chk(av, 1))
                goto fail;
            if ((*av->a_top = objof(ici_str_get_nul_term(argv[i]))) == NULL)
                goto fail;
            ++av->a_top;
        }
    }
    else
    {
        /*
         * Usage2: ici [-f file] [-p prog] etc... [--] [args...]
         */
        for (i = 1; i < argc; ++i)
        {
            if (argv[i][0] == '-')
            {
                for (j = 1; argv[i][j] != '\0'; ++j)
                {
                    switch (argv[i][j])
                    {
                    case 'v':
                        fprintf(stderr, "%s\n", ici_version_string);
                        return 0;

                    case 'm':
                        if (argv[i][++j] != '\0')
                            s = &argv[i][j];
                        else if (++i >= argc)
                            goto usage;
                        else
                            s = argv[i];
                        if ((av->a_base[0] = objof(ici_str_get_nul_term(s))) == NULL)
                            goto fail;
                        break;

                    case '-':
                        while (++i < argc)
                        {
                            if (ici_stk_push_chk(av, 1))
                                goto fail;
                            if ((*av->a_top = objof(ici_str_get_nul_term(argv[i])))==NULL)
                                goto fail;
                            ++av->a_top;
                        }
                        break;

                    case 'f':
                    case 'l':
                    case 'e':
                        if (argv[i][++j] != '\0')
                            arg0 = &argv[i][j];
                        else if (++i >= argc)
                            goto usage;
                        else
                            arg0 = argv[i];
                        break;

                    case 'w':
                        if (argv[i][++j] != '\0')
                            s = &argv[i][j];
                        else if (++i >= argc)
                            goto usage;
                        else
                            s = argv[i];
                        break;

                    case '0': case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                        continue;

                    default:
                        goto usage;
                    }
                    break;
                }
            }
            else
            {
                if (ici_stk_push_chk(av, 1))
                    goto fail;
                if ((*av->a_top = objof(ici_str_get_nul_term(argv[i]))) == NULL)
                    goto fail;
                ++av->a_top;
            }
        }
    }
    if (av->a_base[0] == objof(&o_null))
    {
        if (arg0 == NULL)
            arg0 = argv[0];
        if ((av->a_base[0] = objof(ici_str_get_nul_term(arg0))) == NULL)
            goto fail;
    }
    else
        arg0 = stringof(av->a_base[0])->s_chars;
    if (av->a_top - av->a_base == 2 && stringof(av->a_base[1])->s_nchars == 0)
        --av->a_top; /* Patch around Bourne shell "$@" with no args bug. */

    {
        long            l;

        l = av->a_top - av->a_base;
        if
        (
            ici_set_val(objwsupof(ici_vs.a_top[-1])->o_super, SS(argv), 'o', objof(av))
            ||
            ici_set_val(objwsupof(ici_vs.a_top[-1])->o_super, SS(argc), 'i', &l)
        )
            goto fail;
        ici_decref(av);
     }

#ifndef NODEBUGGING
    if (debugging)
        ici_debug_enabled = 1;
#endif

    /*
     * Pass two over the arguments; actually parse the modules.
     */
    if (argc > 1 && argv[1][0] != '-')
    {
        if ((stream = fopen(argv[1], "r")) == NULL)
        {
            sprintf(buf, "%s: Could not open %s.", argv[0], argv[1]);
            ici_error = buf;
            goto fail;
        }
        if (ici_parse_file(argv[1], (char *)stream, &ici_stdio_ftype))
            goto fail;
    }
    else
    {
        for (i = 1; i < argc; ++i)
        {
            if (argv[i][0] != '-')
                continue;
            if (argv[i][1] == '\0')
            {
                if (ici_parse_file("stdin", (char *)stdin, &ici_stdio_ftype))
                    goto fail;
                continue;
            }
            for (j = 1; argv[i][j] != '\0'; ++j)
            {
                switch (argv[i][j])
                {
                case '-':
                    i = argc;
                    break;

                case 'e':
                    if (argv[i][++j] != '\0')
                        s = &argv[i][j];
                    else if (++i >= argc)
                        goto usage;
                    else
                        s = argv[i];
                    if ((f = ici_sopen(s, strlen(s), NULL)) == NULL)
                        goto fail;
                    f->f_name = SS(empty_string);
                    if (parse_module(f, objwsupof(ici_vs.a_top[-1])) < 0)
                        goto fail;
                    ici_decref(f);
                    break;

                case 'f':
                    fmt = "%s";
                    if (argv[i][++j] != '\0')
                        s = &argv[i][j];
                    else if (++i >= argc)
                        goto usage;
                    else
                        s = argv[i];
                    if (ici_chkbuf(strlen(s) + strlen(fmt)))
                        goto fail;
                    sprintf(buf, fmt, s);
                    if ((stream = fopen(buf, "r")) == NULL)
                    {
                        sprintf(buf, "%s: Could not open %s.", argv[0], s);
                        ici_error = buf;
                        goto fail;
                    }
                    if (ici_parse_file(buf, (char *)stream, &ici_stdio_ftype))
                        goto fail;
                    break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    if ((stream = fdopen(argv[i][j] - '0', "r")) == NULL)
                    {
                        sprintf(buf, "%s: Could not access file descriptor %d.",
                            argv[0], argv[i][j] - '0');
                        ici_error = buf;
                        goto fail;
                    }
                    if (ici_parse_file(arg0, (char *)stream, &ici_stdio_ftype))
                        goto fail;
                    continue;

                case 's':
                    break;
                }
                break;
            }
        }
    }

#ifndef NOPROFILE
    /* Make sure any profiling that started while parsing has finished. */
    if (ici_profile_active)
        ici_profile_return();
#endif

    ici_uninit();
    return 0;

usage:
    fprintf(stderr, "usage:\t%s [-s] [-f file] [-e prog] [-digit] [-m name] [--] args...\n", argv[0]);
    fprintf(stderr, "\t%s file args...\n", argv[0]);
    ici_uninit();
    ici_error = "invalid command line arguments";
    return 1;

fail:
    fflush(stdout);
    fprintf(stderr, "%s\n", ici_error);
    ici_uninit();
    return 1;
}
