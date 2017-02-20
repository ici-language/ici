#ifndef ICI_FILE_H
#define ICI_FILE_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct ftype
{
    int         (*ft_getch)();
    int         (*ft_ungetch)();
    int         (*ft_putch)();
    int         (*ft_flush)();
    int         (*ft_close)();
    long        (*ft_seek)();
    int         (*ft_eof)();
    int         (*ft_write)();
};

struct file
{
    object_t    o_head;
    char        *f_file;
    ftype_t     *f_type;
    string_t    *f_name;    /* Reasonable name to call it by. */
};
#define fileof(o)   ((file_t *)(o))
#define isfile(o)   (objof(o)->o_tcode == TC_FILE)

#define F_CLOSED    0x10
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_FILE_H */
