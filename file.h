#ifndef ICI_FILE_H
#define ICI_FILE_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

/*
 * File abstraction. Each function is assumed to be compatible with
 * the stdio function of the same name. In the case were the file is
 * a stdio stream, these are the stdio functions.
 */
struct ici_ftype
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

struct ici_file
{
    object_t    o_head;
    void        *f_file;
    ftype_t     *f_type;
    string_t    *f_name;    /* Reasonable name to call it by. */
    object_t    *f_ref;
};
/*
 * f_ref                An object for this file object to reference.
 *                      This is used to reference the string when we
 *                      are treating a string as a file, and other cases,
 *                      to keep the object referenced. Basically if f_file
 *                      is an implicit reference to some object. May be NULL.
 */
#define fileof(o)   ((file_t *)(o))
#define isfile(o)   (objof(o)->o_tcode == TC_FILE)

#define F_CLOSED    0x10    /* File is closed. */
#define F_NOCLOSE   0x20    /* Don't close on object free. */
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_FILE_H */
