#define ICI_CORE
#include "exec.h"
#include "op.h"
#include "int.h"
#include "buf.h"
#include "pc.h"
#include "struct.h"
#include "null.h"
#include "forall.h"
#include "catch.h"

/*
 * self => array looper &array[2] (xs)
 * array => - (os)
 *
 * Ie, like a loop, but on the first time in it skips the first two elements
 * of the the loop, which are expected to be an array of code for the step
 * and an exec operator to run it.
 */
int
ici_op_for()
{
    get_pc(arrayof(ici_os.a_top[-1]), ici_xs.a_top + 1);
    pcof(ici_xs.a_top[1])->pc_next += opof(ici_xs.a_top[-1])->op_code;
    ici_xs.a_top[-1] = ici_os.a_top[-1];
    *ici_xs.a_top++ = objof(&o_looper);
    ++ici_xs.a_top; /* pc */
    --ici_os.a_top;
    return 0;
}

op_t    o_exec          = {OBJ(TC_OP), NULL, OP_EXEC};
op_t    o_looper        = {OBJ(TC_OP), NULL, OP_LOOPER};
op_t    o_loop          = {OBJ(TC_OP), NULL, OP_LOOP};
op_t    o_break         = {OBJ(TC_OP), NULL, OP_BREAK};
op_t    o_continue      = {OBJ(TC_OP), NULL, OP_CONTINUE};
op_t    o_if            = {OBJ(TC_OP), NULL, OP_IF};
op_t    o_ifnotbreak    = {OBJ(TC_OP), NULL, OP_IFNOTBREAK};
op_t    o_ifbreak       = {OBJ(TC_OP), NULL, OP_IFBREAK};
op_t    o_ifelse        = {OBJ(TC_OP), NULL, OP_IFELSE};
op_t    o_pop           = {OBJ(TC_OP), NULL, OP_POP};
op_t    o_andand        = {OBJ(TC_OP), NULL, OP_ANDAND, 1};
op_t    o_barbar        = {OBJ(TC_OP), NULL, OP_ANDAND, 0};
op_t    o_switch        = {OBJ(TC_OP), NULL, OP_SWITCH};
op_t    o_switcher      = {OBJ(TC_OP), NULL, OP_SWITCHER};
op_t    o_critsect      = {OBJ(TC_OP), NULL, OP_CRITSECT};
op_t    o_waitfor       = {OBJ(TC_OP), NULL, OP_WAITFOR};
op_t    o_rewind        = {OBJ(TC_OP), NULL, OP_REWIND};
op_t    o_end           = {OBJ(TC_OP), NULL, OP_ENDCODE};

