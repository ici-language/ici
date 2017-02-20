#ifndef ICI_OP_H
#define ICI_OP_H

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
struct op
{
    object_t    o_head;
    int         (*op_func)();
    int         op_ecode;       /* See OP_* below. */
    int         op_code;
};
#define opof(o) ((op_t *)o)
#define isop(o) ((o)->o_tcode == TC_OP)

/*
 * End of ici.h export. --ici.h-end--
 */

/*
 * Operator codes. These are stored in the op_ecode field and
 * allow direct switching to the appropriate code in the main
 * execution loop. If op_ecode is OP_OTHER, then the op_func field
 * is significant instead.
 */
enum
{
    OP_OTHER,
    OP_CALL,
    OP_NAMELVALUE,
    OP_DOT,
    OP_DOTKEEP,
    OP_DOTRKEEP,
    OP_ASSIGN,
    OP_ASSIGNLOCAL,
    OP_EXEC,
    OP_LOOP,
    OP_IFNOTBREAK,
    OP_IFBREAK,
    OP_BREAK,
    OP_QUOTE,
    OP_BINOP,
    OP_AT,
    OP_SWAP,
    OP_BINOP_FOR_TEMP,
    OP_AGGR_KEY_CALL,
    OP_COLON,
    OP_COLONCARET,
    OP_METHOD_CALL,
    OP_SUPER_CALL,
    OP_ASSIGNLOCALVAR,
    OP_CRITSECT,
    OP_WAITFOR,
};
#endif /* ICI_OP_H */
