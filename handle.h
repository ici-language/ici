#ifndef ICI_HANDLE_H
#define ICI_HANDLE_H

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
/*
 * A handle is a generic object that can be used to refer to some C data
 * object. It supports a super pointer. This can be used to (a) identify
 * a handle as being associated with a particular C data type, and (b) to
 * provide objects of the same type with a class full of methods to operate
 * on the objects of that type.
 */
struct ici_handle
{
    objwsup_t   o_head;
    void        *h_ptr;
    string_t    *h_name;
};
#define handleof(o)        ((ici_handle_t *)(o))
#define ishandle(o)        (objof(o)->o_tcode == TC_HANDLE)
/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_HANDLE_H */
