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
    OP_ASSIGN_TO_NAME,
    OP_ASSIGNLOCAL,
    OP_EXEC,
    OP_LOOP,
    OP_REWIND,
    OP_ENDCODE,
    OP_IF,
    OP_IFELSE,
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

/*
 * Extern definitions for various statically defined op objects. They
 * Are defined in various source files. Generally where they are
 * implemented.
 */
extern op_t             o_quote;
extern op_t             o_looper;
extern op_t             o_loop;
extern op_t             o_rewind;
extern op_t             o_end;
extern op_t             o_break;
extern op_t             o_continue;
extern op_t             o_offsq;
extern op_t             o_exec;
extern op_t             o_mkfunc;
extern op_t             o_return;
extern op_t             o_call;
extern op_t             o_method_call;
extern op_t             o_super_call;
extern op_t             o_if;
extern op_t             o_ifnot;
extern op_t             o_ifnotbreak;
extern op_t             o_ifbreak;
extern op_t             o_ifelse;
extern op_t             o_pop;
extern op_t             o_colon;
extern op_t             o_coloncaret;
extern op_t             o_dot;
extern op_t             o_dotkeep;
extern op_t             o_dotrkeep;
extern op_t             o_mkptr;
extern op_t             o_openptr;
extern op_t             o_fetch;
extern op_t             o_for;
extern op_t             o_mklvalue;
extern op_t             o_rematch;
extern op_t             o_renotmatch;
extern op_t             o_reextract;
extern op_t             o_onerror;
extern op_t             o_andand;
extern op_t             o_barbar;
extern op_t             o_namelvalue;
extern op_t             o_switch;
extern op_t             o_switcher;
extern op_t             o_critsect;
extern op_t             o_waitfor;

#endif /* ICI_OP_H */
