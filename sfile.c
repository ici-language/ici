#define ICI_CORE
#include "file.h"
#include "str.h"
#include "mem.h"
#include "buf.h"

/*
 * This structure is used wherever a character buffer in memory is treated as
 * an ICI file object.  Two related file types reference this structure:
 *
 * ici_charbuf_ftype:   used for atomic string objects, memory objects, and
 *                      simple C strings, which cannot move in memory.
 *
 * ici_strbuf_ftype:    used for mutable string buffer objects, which can move
 *                      unexpectedly in memory.
 */
typedef struct charbuf
{
    char        *cb_data;
    char        *cb_ptr;
    int         cb_size;
    int         cb_eof;
    ici_obj_t   *cb_ref;
    int         cb_readonly;
}
    charbuf_t;

static int
cbgetc(charbuf_t *cb)
{
    if (cb->cb_ptr < cb->cb_data || cb->cb_ptr >= cb->cb_data + cb->cb_size)
    {
        cb->cb_eof = 1;
        return EOF;
    }
    cb->cb_eof = 0;
    return *cb->cb_ptr++ & 0xFF;
}

static int
cbungetc(int c, charbuf_t *cb)
{
    if (c == EOF || cb->cb_ptr <= cb->cb_data || cb->cb_ptr > cb->cb_data + cb->cb_size)
        return EOF;
    *--cb->cb_ptr = c;
    cb->cb_eof = 0;
    return c;
}

static int
cbputc(int c, charbuf_t *cb)
{
    if (cb->cb_readonly || cb->cb_ptr < cb->cb_data || cb->cb_ptr >= cb->cb_data + cb->cb_size)
        return EOF;
    return *cb->cb_ptr++ = c;
}

static int
cbflush(charbuf_t *cb)
{
    /* NOTE fflush() returns 0 if file is not writeable */
    return 0;
}

static int
cbclose(charbuf_t *cb)
{
    if (cb->cb_ref == NULL)
        ici_free(cb->cb_data);
    ici_tfree(cb, charbuf_t);
    return 0;
}

static long
cbseek(charbuf_t *cb, long offset, long whence)
{
    switch (whence)
    {
    case 0:
        cb->cb_ptr = cb->cb_data + offset;
        break;

    case 1:
        cb->cb_ptr += offset;
        break;

    case 2:
        cb->cb_ptr = cb->cb_data + cb->cb_size + offset;
        break;
    }
    return (long)(cb->cb_ptr - cb->cb_data);
}

static int
cbeof(charbuf_t *cb)
{
    return cb->cb_eof;
}

static int
cbwrite(char *data, long count, charbuf_t *cb)
{
    if (cb->cb_readonly || count <= 0)
        return 0;
    if (cb->cb_ptr < cb->cb_data || cb->cb_ptr >= cb->cb_data + cb->cb_size)
        return 0;
    if (count > cb->cb_data + cb->cb_size - cb->cb_ptr)
        count = cb->cb_data + cb->cb_size - cb->cb_ptr;
    memcpy(cb->cb_ptr, data, count);
    cb->cb_ptr += count;
    return count;
}

/*
 * ici_charbuf_ftype is used for buffers which cannot move in memory.  This
 * includes atomic string objects, memory objects, and C strings.  It is
 * slightly more efficient at reading and writing than ici_strbuf_ftype is,
 * since it stores pointers directly into the immovable buffer.
 *
 * This type replaces string_ftype (which was a misnomer since it could be
 * used for memory objects but not mutable string objects).
 */
ici_ftype_t ici_charbuf_ftype =
{
    0,
    cbgetc,
    cbungetc,
    cbputc,
    cbflush,
    cbclose,
    cbseek,
    cbeof,
    cbwrite
};

static void
reattach_string_buffer(charbuf_t *sb)
{
    int     index;

    index = sb->cb_ptr - sb->cb_data;
    sb->cb_data = stringof(sb->cb_ref)->s_chars;
    sb->cb_size = stringof(sb->cb_ref)->s_nchars;
    sb->cb_ptr = sb->cb_data + index;
}

static int
sbgetc(charbuf_t *sb)
{
    reattach_string_buffer(sb);
    return cbgetc(sb);
}

static int
sbungetc(int c, charbuf_t *sb)
{
    reattach_string_buffer(sb);
    return cbungetc(c, sb);
}

static long
sbseek(charbuf_t *sb, long offset, long whence)
{
    reattach_string_buffer(sb);
    return cbseek(sb, offset, whence);
}

static int
sbwrite(char *data, long count, charbuf_t *sb)
{
    ici_str_t   *s;
    int         size;

    if (sb->cb_readonly || count <= 0)
        return 0;
    if (sb->cb_ptr < sb->cb_data || sb->cb_ptr > sb->cb_data + sb->cb_size)
        return 0;
    s = stringof(sb->cb_ref);
    size = sb->cb_ptr - sb->cb_data + count;
    if (ici_str_need_size(s, size))
        return 0;
    if (s->s_nchars < size)
        s->s_nchars = size;
    s->s_chars[s->s_nchars] = '\0';
    reattach_string_buffer(sb);
    memcpy(sb->cb_ptr, data, count);
    sb->cb_ptr += count;
    return count;
}

static int
sbputc(int c, charbuf_t *sb)
{
    char cc = c;
    return (sbwrite(&cc, 1, sb) == 1) ? c : EOF;
}

/*
 * ici_strbuf_ftype is used for mutable string buffer objects, which can move
 * unexpectedly in memory as they grow.  Because of this, this file type must
 * check on every file operation whether the referenced string buffer has
 * moved.  This makes it less efficient than ici_charbuf_ftype for buffers
 * that are known to be immovable.
 */
ici_ftype_t ici_strbuf_ftype =
{
    0,
    sbgetc,
    sbungetc,
    sbputc,
    cbflush,
    cbclose,
    sbseek,
    cbeof,
    sbwrite
};

/*
 * Create an ICI file object that treats the character buffer referenced by
 * 'data' (of length 'size') as a file.  If 'ref' is non-NULL it must refer to
 * either a string object or a memory object that owns the data, and the data
 * is used in-place.  But if 'ref' is NULL, it is assumed that the data must
 * be copied into a private allocation first.  The private allocation will be
 * freed when the file is closed.
 *
 * If 'readonly' is non-zero, the returned file gives read-only access to the
 * data.  If 'readonly' is zero, 'ref' must refer to either a mutable string
 * buffer object (created by ici_str_buf_new()) or a memory object, and the
 * file gives read/write access to the object's data.  The object's initial
 * data is preserved.  Writing past the end of a string buffer will extend it;
 * writing past the end of a memory object gets truncated.
 *
 * Returns NULL on error, usual conventions.
 *
 * This --func-- forms part of the --ici-api--.
 */
ici_file_t *
ici_open_charbuf(char *data, int size, ici_obj_t *ref, int readonly)
{
    register ici_file_t     *f      = NULL;
    register charbuf_t      *cb;

    if ((cb = ici_talloc(charbuf_t)) == NULL)
        return NULL;
    cb->cb_size = size;
    cb->cb_eof = 0;
    cb->cb_readonly = readonly;
    cb->cb_ref = ref;
    if (ref != NULL)
    {
        /*
         * Use this object's data in-place; the ici_file_t object keeps a
         * reference to it so it won't go away.
         */
        cb->cb_data = data;
        cb->cb_ptr = data;
        /*
         * If this is a mutable string buffer, use ici_strbuf_ftype.  If this
         * is an atomic (immutable) string, or a memory object, it is slightly
         * more efficient to use ici_charbuf_ftype.  Of course an atomic
         * string can't be opened for writing.
         */
        if (isstring(ref))
        {
            if ((ref->o_flags & (O_ATOM|ICI_S_SEP_ALLOC)) == ICI_S_SEP_ALLOC)
                f = ici_file_new((char *)cb, &ici_strbuf_ftype, NULL, ref);
            else if (readonly)
                f = ici_file_new((char *)cb, &ici_charbuf_ftype, NULL, ref);
            else
                ici_error = "attempt to open an atomic string for writing";
        }
        else if (ismem(ref))
        {
            f = ici_file_new((char *)cb, &ici_charbuf_ftype, NULL, ref);
        }
        else if (!ici_chkbuf(50))
        {
            char n[ICI_OBJNAMEZ];
            sprintf(buf, "attempt to open %s as a char buffer", ici_objname(n, ref));
            ici_error = buf;
        }
    }
    else
    {
        /*
         * This is not an ici object, so it's read only, and we need to take a
         * copy of it.
         */
        if (readonly)
        {
            if ((cb->cb_data = ici_alloc(size)) != NULL)
            {
                memcpy(cb->cb_data, data, size);
                cb->cb_ptr = cb->cb_data;
                f = ici_file_new((char *)cb, &ici_charbuf_ftype, NULL, ref);
                if (f == NULL)
                    ici_free(cb->cb_data);
            }
        }
        else
        {
            ici_error = "attempt to open non-object char buffer for writing";
        }
    }
    if (f == NULL)
        ici_tfree(cb, charbuf_t);
    return f;
}
