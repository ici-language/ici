#define ICI_CORE
#include "fwd.h"
#include "str.h"

/*
 * Include sstring.h to define static string objects (with 1 ref count
 * and a pesudo size of 1 in o_leafz).
 */
#define SSTRING(name, str)    sstring_t ici_ss_##name \
        = {{TC_STRING, 0, 1, 1}, NULL, NULL, 0, 0, (sizeof str) - 1, str};
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
#define SSTRING(name, str) atom(SSO(name), 1) == SSO(name) &&
#include "sstring.h"
#undef SSTRING
        1
    )
        return 0;
    ici_error = "failed to setup static strings";
    return 1;
}
