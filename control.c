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

/*
 * obj self     => obj self pc (xs)
 *              => (os)
 */
int
ici_op_looper()
{
    get_pc(arrayof(ici_xs.a_top[-2]), ici_xs.a_top);
    ++ici_xs.a_top;
    return 0;
}

/*
 * Pop the execution stack until a looper is found.
 */
int
ici_op_continue()
{
    object_t    **s;

    for (s = ici_xs.a_top; s > ici_xs.a_base + 1; --s)
    {
        if (iscatch(s[-1]))
        {
            if (s[-1]->o_flags & CF_EVAL_BASE)
                break;
            if (s[-1]->o_flags & CF_CRIT_SECT)
                --ici_exec->x_critsect;
        }
        if (s[-1] == objof(&o_looper) || isforall(s[-1]))
        {
            ici_xs.a_top = s;
            return 0;
        }
    }
    ici_error = "continue not within loop";
    return 1;
}

/*
 * bool obj => bool (os) OR pc (xs)
 */
int
ici_op_andand()
{
    int         c;

    if ((c = !isfalse(ici_os.a_top[-2])) == opof(ici_xs.a_top[-1])->op_code)
    {
        /*
         * Have to test next part of the condition.
         */
        --ici_xs.a_top;
        get_pc(arrayof(ici_os.a_top[-1]), ici_xs.a_top);
        ++ici_xs.a_top;
        ici_os.a_top -= 2;
        return 0;
    }
    /*
     * Reduce the value to 0 or 1.
     */
    ici_os.a_top[-2] = objof(c ? o_one : o_zero);
    --ici_os.a_top;
    --ici_xs.a_top;
    return 0;
}

/*
 * NULL self (xs) =>
 */
int
ici_op_switcher()
{
    ici_xs.a_top -= 2;
    return 0;
}

/*
 * value array struct   => (os)
 *                      => NULL switcher (pc(array) + struct.value) (xs)
 */
int
ici_op_switch()
{
    register slot_t     *sl;

    if ((sl = find_raw_slot(structof(ici_os.a_top[-1]), ici_os.a_top[-3]))->sl_key == NULL)
    {
        if ((sl = find_raw_slot(structof(ici_os.a_top[-1]), objof(&o_mark)))->sl_key == NULL)
        {
            ici_os.a_top -= 3;
            --ici_xs.a_top;
            return 0;
        }
    }
    ici_xs.a_top[-1] = objof(&o_null);
    *ici_xs.a_top++ = objof(&o_switcher);
    get_pc(arrayof(ici_os.a_top[-2]), ici_xs.a_top);
    pcof(*ici_xs.a_top)->pc_next += intof(sl->sl_value)->i_value;
    ++ici_xs.a_top;
    ici_os.a_top -= 3;
    return 0;
}

int
ici_op_pop()
{
    --ici_os.a_top;
    --ici_xs.a_top;
    return 0;
}

op_t    o_exec          = {OBJ(TC_OP), NULL, OP_EXEC};
op_t    o_looper        = {OBJ(TC_OP), ici_op_looper};
op_t    o_loop          = {OBJ(TC_OP), NULL, OP_LOOP};
op_t    o_break         = {OBJ(TC_OP), NULL, OP_BREAK};
op_t    o_continue      = {OBJ(TC_OP), ici_op_continue};
op_t    o_if            = {OBJ(TC_OP), NULL, OP_IF};
op_t    o_ifnotbreak    = {OBJ(TC_OP), NULL, OP_IFNOTBREAK};
op_t    o_ifbreak       = {OBJ(TC_OP), NULL, OP_IFBREAK};
op_t    o_ifelse        = {OBJ(TC_OP), NULL, OP_IFELSE};
op_t    o_pop           = {OBJ(TC_OP), ici_op_pop};
op_t    o_andand        = {OBJ(TC_OP), ici_op_andand, 0, 1};
op_t    o_barbar        = {OBJ(TC_OP), ici_op_andand, 0, 0};
op_t    o_switch        = {OBJ(TC_OP), ici_op_switch};
op_t    o_switcher      = {OBJ(TC_OP), ici_op_switcher};
op_t    o_critsect      = {OBJ(TC_OP), NULL, OP_CRITSECT};
op_t    o_waitfor       = {OBJ(TC_OP), NULL, OP_WAITFOR};
op_t    o_rewind        = {OBJ(TC_OP), NULL, OP_REWIND};
op_t    o_end           = {OBJ(TC_OP), NULL, OP_ENDCODE};

