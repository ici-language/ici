#ifndef ICI_RE_H
#define ICI_RE_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

#include "pcre/pcre.h"

struct ici_regexp
{
    object_t    o_head;
    pcre        *r_re;
    pcre_extra  *r_rex;
    string_t    *r_pat;
};
/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
#define regexpof(o)     ((regexp_t *)(o))
#define isregexp(o)     ((o)->o_tcode == TC_REGEXP)

extern int
ici_pcre(regexp_t *r,
    const char *subject, int length, int start_offset,
    int options, int *offsets, int offsetcount);


/*
 * End of ici.h export. --ici.h-end--
 */

#endif
