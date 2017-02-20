#define ICI_CORE
#include "fwd.h"
#include "str.h"

#define SSTRING(name, str)    sstring_t ici_ss_##name \
        = {OBJ(TC_STRING), \
          NULL, NULL, 0, 0, (sizeof str) - 1, str};
#include "sstring.h"
#undef  SSTRING

static int
mk_sstring(string_t *s)
{
    rego(s);
    objof(s)->o_leafz = 1; /* Not allocated. */
    if (atom(objof(s), 1) != objof(s))
    {
        ici_error = "failed to setup static strings";
        return 1;
    }
    return 0;
}

int
ici_init_sstrings(void)
{
    return
#define SSTRING(name, str) mk_sstring(SS(name)) ||
#include "sstring.h"
#undef SSTRING
        0;
}
