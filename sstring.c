#define ICI_CORE
#include "fwd.h"
#include "str.h"

/*
 * Include sstring.h to define static string objects (with 1 ref count
 * and a pesudo size of 1 in o_leafz).
 */
#if KEEP_STRING_HASH
#   define SSTRING(name, str)    sstring_t ici_ss_##name \
        = {{TC_STRING, 0, 1, 1}, NULL, NULL, 0, 0, (sizeof str) - 1, NULL, str};
#else
#   define SSTRING(name, str)    sstring_t ici_ss_##name \
        = {{TC_STRING, 0, 1, 1}, NULL, NULL, 0, (sizeof str) - 1, NULL, str};
#endif
#include "sstring.h"
#undef  SSTRING

/*
 * Make all our staticly initialised strings atoms. Note that they are
 * *not* registered with the garbage collector.
 */
int
ici_init_sstrings(void)
{
    if
    (
#define SSTRING(name, str) \
    (SS(name)->s_chars = SS(name)->s_u.su_inline_chars, ici_atom(SSO(name), 1)) == SSO(name) \
    &&
#include "sstring.h"
#undef SSTRING
        1
    )
        return 0;
    ici_error = "failed to setup static strings";
    return 1;
}

/*
 * Reset all our statically initialised strings ready for a future re-initialisation.
 */
void
ici_uninit_sstrings(void)
{
#define SSTRING(name, str) \
    ICI_OBJ_SET_TFNZ(SSO(name), TC_STRING, 0, 1, 1); \
    ici_ss_##name.s_struct = NULL; \
    ici_ss_##name.s_slot = NULL; \
    ici_ss_##name.s_vsver = 0;
#include "sstring.h"
#undef SSTRING
}

