#define ICI_CORE
#include "exec.h"
#include "float.h"
#include "int.h"
#include "op.h"
#include "parse.h"
#include "buf.h"
#include "null.h"

int
ici_op_unary(void)
{
    int_t       *i;
    float_t     *f;

    switch (opof(ici_xs.a_top[-1])->op_code)
    {
    case t_subtype(T_EXCLAM):
        if (isfalse(ici_os.a_top[-1]))
            ici_os.a_top[-1] = objof(ici_one);
        else
            ici_os.a_top[-1] = objof(ici_zero);
        --ici_xs.a_top;
        return 0;

    case t_subtype(T_TILDE):
        if (!isint(ici_os.a_top[-1]))
            goto fail;
        if ((i = ici_int_new(~intof(ici_os.a_top[-1])->i_value)) == NULL)
            return 1;
        ici_os.a_top[-1] = objof(i);
        ici_decref(i);
        --ici_xs.a_top;
        return 0;

    case t_subtype(T_MINUS):
        if (isint(ici_os.a_top[-1]))
        {
            if ((i = ici_int_new(-intof(ici_os.a_top[-1])->i_value)) == NULL)
                return 1;
            ici_os.a_top[-1] = objof(i);
            ici_decref(i);
            --ici_xs.a_top;
            return 0;
        }
        else if (isfloat(ici_os.a_top[-1]))
        {
            if ((f = ici_float_new(-floatof(ici_os.a_top[-1])->f_value)) == NULL)
                return 1;
            ici_os.a_top[-1] = objof(f);
            ici_decref(f);
            --ici_xs.a_top;
            return 0;
        }
    fail:
    default:
        switch (opof(ici_xs.a_top[-1])->op_code)
        {
        case t_subtype(T_EXCLAM): ici_error = "!"; break;
        case t_subtype(T_TILDE): ici_error = "~"; break;
        case t_subtype(T_MINUS): ici_error = "-"; break;
        default: ici_error = "<unknown unary operator>"; break;
        }
        sprintf(buf, "attempt to perform \"%s %s\"",
            ici_error, ici_typeof(ici_os.a_top[-1])->t_name);
        ici_error = buf;
        return 1;
    }
}
