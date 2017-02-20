#include "file.h"
#include "exec.h"
#include "struct.h"

int
mkstream(name, stream)
char	*name;
FILE	*stream;
{
    register file_t	*f;
    string_t		*n;
    int			r;

    if ((n = new_cname(name)) == NULL)
	return 1;
    if ((f = new_file((char *)stream, &stdio_ftype, n)) == NULL)
	return 1;
    r = assign(structof(v_top[-1])->s_super, f->f_name, f);
    loose(f);
    loose(n);
    return r;
}
