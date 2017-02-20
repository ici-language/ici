#ifndef ICI_STRING_H
#define ICI_STRING_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */

struct string
{
    object_t    o_head;
    struct_t    *s_struct;      /* Where we were last found on the vs. */
    slot_t      *s_slot;        /* And our slot. */
    long        s_vsver;        /* The vs version at that time. */
    unsigned long s_hash;       /* String hash code or 0 if not yet computed */
    int         s_nchars;
    char        s_chars[1];     /* And following bytes. */
};
#define stringof(o)     ((string_t *)o)
#define isstring(o)     ((o)->o_tcode == TC_STRING)

/*
 * This flag indicates that the lookup-lookaside mechanism is referencing
 * an atomic struct. It is stored in the allowed area of o_flags in o_head.
 */
#define S_LOOKASIDE_IS_ATOM 0x10

/*
 * Macros to assist external modules in getting ICI strings. To use, make
 * an include file (say icistr.h) with your strings, and what you want to
 * call them by, formatted like this:
 *
 *  ICI_STR(fred, "fred")
 *  ICI_STR(amp, "&")
 *
 * etc. Include that file in any files that access ICI strings.
 * Access them with either ICIS(fred) or ICISO(fred) which return
 * string_t* and object_t* pointers respectively. For example:
 *
 *  o = fetch(s, ICIS(fred));
 *
 * Next, in a source file, write:
 *
 *  #undef  ICI_STR
 *  #define ICI_STR ICI_STR_DECL
 *  #include "icistr.h"
 *
 *  int
 *  init_ici_str(void)
 *  {
 *  #undef  ICI_STR
 *  #define ICI_STR ICI_STR_MAKE
 *      return
 *  #include "icistr.h"
 *      0;
 *  }
 *  #undef  ICI_STR
 *  #define ICI_STR ICI_STR_NORM
 *
 * Finally, call init_ici_str() at startup. It returns 1 on error, usual
 * conventions.
 */
#define ICIS(name)              (ici_str_##name)
#define ICISO(name)             (objof(ICIS(name)))
#define ICI_STR_NORM(name, str) extern string_t *ici_str_##name;
#define ICI_STR_DECL(name, str) string_t *ici_str_##name;
#define ICI_STR_MAKE(name, str) (ICIS(name) = new_cname(str)) == NULL ||
#define ICI_STR_REL(name, str)  decref(ICIS(name));
#define ICI_STR                 ICI_STR_NORM
/*
 * End of ici.h export. --ici.h-end--
 */

#ifdef  ICI_CORE
/*
 * A structure to hold static (ie, not allocated) strings. These can
 * only be used where the atom() operation is guaranteed to use the
 * string given, and never find an existing one already in the atom pool.
 * They are only used by the ICI core on first initialisation. They
 * are registered with the garbage collector because string reference
 * other objects and they may be the only ref. But they are never collected
 * because they always have a ref. They are inserted into the atom pool
 * of course. They only support strings up to 15 characters. See sstring.c.
 *
 * This structure must be an exact overlay of the one above.
 */
typedef struct sstring  sstring_t;
struct sstring
{
    object_t    o_head;
    struct_t    *s_struct;      /* Where we were last found on the vs. */
    slot_t      *s_slot;        /* And our slot. */
    long        s_vsver;        /* The vs version at that time. */
    unsigned long s_hash;       /* String hash code or 0 if not yet computed */
    int         s_nchars;
    char        s_chars[16];    /* And following bytes. */
};

#define SSTRING(name, str)    extern sstring_t ici_ss_##name;
#include "sstring.h"
#undef SSTRING

#define SS(name)         ((string_t *)&ici_ss_##name)
#define SSO(name)        ((object_t *)&ici_ss_##name)

#endif /* ICI_CORE */

#endif /* ICI_STRING_H */
