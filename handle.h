#ifndef ICI_HANDLE_H
#define ICI_HANDLE_H

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
/*
 * A handle is a generic object that can be used to refer to some C data
 * object. Handles support an (optional) super pointer. Handles are named
 * with an ICI string to give type checking, reporting, and diagnostic
 * support. The handle object provides most of the generic machinery
 * of ICI objects. An optional pre-free function pointer can be supplied
 * to handle cleanup on final collection of the handle.
 */
struct ici_handle
{
    ici_objwsup_t   o_head;
    void        *h_ptr;
    ici_str_t   *h_name;
    void        (*h_pre_free)(ici_handle_t *h);
};
#define handleof(o)        ((ici_handle_t *)(o))
#define ishandle(o)        (objof(o)->o_tcode == TC_HANDLE)
#define ishandleof(o, n)   (ishandle(o) && handleof(o)->h_name == (n))
/*
 * Flags set in the upper nibble of o_head.o_flags, which is
 * allowed for type specific use.
 */
#define H_CLOSED                0x10
#define H_HAS_PRIV_STRUCT       0x20
/*
 * H_CLOSED             If set, the thing h_ptr points to is no longer
 *                      valid (it has probably been freed). This flag
 *                      exists for the convenience of users, as the
 *                      core handle code doesn't touch this much.
 *                      Use of this latent feature depends on needs.
 *
 * H_HAS_PRIV_STRUCT    This handle has had a private struct allocated
 *                      to hold ICI values that have been assigned to
 *                      it. This does not happen until required, as
 *                      not all handles will ever need one. The super
 *                      is the private struct (and it's super is the
 *                      super the creator originally supplied).
 */

/*
 * End of ici.h export. --ici.h-end--
 */

#endif /* ICI_HANDLE_H */
