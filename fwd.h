#ifndef ICI_FWD_H
#define ICI_FWD_H
/*
 * fwd.h - basic configuration defines, universally used macros and
 * forward type, data and function defintions. Almost every ICI source
 * file needs this.
 */

/*
 * In general CONFIG_FILE is defined by the external build environment,
 * but for some common cases we have standard settings that can be used
 * directly. Windows in particular, because it is such a pain to handle
 * setting like this in Visual C, Project Builder and similar "advanced"
 * development environments.
 */
#if defined(_WIN32) && !defined(CONFIG_FILE)
#define CONFIG_FILE "conf-w32.h"
#elif defined(__MACH__) && defined(__APPLE__) && !defined(CONFIG_FILE)
#define CONFIG_FILE "conf-osx.h"
#endif

#ifndef CONFIG_FILE
/*
 * CONFIG_FILE is supposed to be set from some makefile with some compile
 * line option to something like "conf-sun.h" (including the quotes).
 */
#error "The preprocessor define CONFIG_FILE has not been set."
#endif

#ifndef ICI_CONF_H
#include CONFIG_FILE
#endif

#ifndef NDEBUG
#define BUGHUNT
#endif

#include <assert.h>

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef NOSIGNALS
# ifdef SUNOS5
#  include <signal.h>
# endif
#endif

/*
 * ICI version number. Note that this occurs in a string in conf.c too.
 */
#define ICI_VER_MAJOR   4
#define ICI_VER_MINOR   0
#define ICI_VER_RELEASE 2

/*
 * The ICI version number composed into an 8.8.16 long for simple comparisons.
 */
#define ICI_VER         ((ICI_VER_MAJOR << 24) | (ICI_VER_MINOR << 16) | ICI_VER_RELEASE)

/*
 * The oldet version number for which the binary interface for seperately
 * compiled modules is backwards compatible. This must be updated whenever
 * the exernal interface changes in a way that could break already compiled
 * modules. See ici_interface_check().
 */
#define ICI_BACK_COMPATIBLE_VER ((4 << 24) | (0 << 16) | 1)


/*
 * DLI is defined in some configurations (Windows, in the conf include file)
 * to be a declaration modifyer which must be applied to data objects being
 * referenced from a dynamically loaded DLL.
 *
 * If it hasn't been defined yet, define it to be null. Most system don't
 * need it.
 */
#ifndef DLI
#define DLI
#endif

/*
 * The character which seperates directories in a path list on this
 * architecture. This is the default value, it may have been set in
 * the config file.
 */
#ifndef ICI_PATH_SEP
#define ICI_PATH_SEP    ':'
#endif

/*
 * The character which seperates segments in a path on this
 * architecture. This is the default value, it may have been set in
 * the config file.
 */
#ifndef ICI_DIR_SEP
#define ICI_DIR_SEP    '/'
#endif

/*
 * The string which is the extension of a dynamicly loaded library on this
 * architecture. This is the default value, it may have been set in
 * the config file.
 */
#ifndef ICI_DLL_EXT
#define ICI_DLL_EXT     ".so"
#endif

/*
 * A hash function for pointers. This is used in a few places. Notably in the
 * hash of object addresses for struct lookup. It is a balance between
 * effectiveness, speed, and machine knowledge. It may or may not be right
 * for a given machine, so we allow it to be defined in the configuration.
 * But if it wasn't, this is what we use.
 */
#ifndef ICI_PTR_HASH
/*
#define ICI_PTR_HASH(p) (((unsigned long)(p) >> 12) * 31 ^ ((unsigned long)(p) >> 4) * 17)
*/
#define ICI_PTR_HASH(p) \
     (ici_crc_table[((size_t)(p) >>  4) & 0xFF] \
    ^ ici_crc_table[((size_t)(p) >> 12) & 0xFF])


#endif
 
#define nels(a)         (sizeof a / sizeof a[0])

typedef struct array    array_t;
typedef struct catchs   catch_t;
typedef struct slot     slot_t;
typedef struct sets     set_t;
typedef struct structs  struct_t;
typedef struct exec     exec_t;
typedef struct floats   float_t;
typedef struct file     file_t;
typedef struct func     func_t;
typedef struct cfunc    cfunc_t;
typedef struct method   method_t;
typedef struct ints     int_t;
typedef struct marks    mark_t;
typedef struct null     null_t;
typedef struct object   object_t;
typedef struct objwsup  objwsup_t;
typedef struct op       op_t;
typedef struct pc       pc_t;
typedef struct ptr      ptr_t;
typedef struct regexpo  regexp_t;
typedef struct src      src_t;
typedef struct string   string_t;
typedef struct type     type_t;
typedef struct wrap     wrap_t;
typedef struct ftype    ftype_t;
typedef struct forall   forall_t;
typedef struct parse    parse_t;
typedef struct mem      mem_t;
typedef struct expr     expr_t;
typedef union  ostemp   ostemp_t;
typedef struct ici_handle ici_handle_t;
#ifndef NODEBUGGING
typedef struct debug    debug_t;
#endif

extern DLI object_t     **objs;
extern DLI object_t     **objs_top;
extern DLI object_t     **objs_limit;
extern DLI object_t     **atoms;
extern DLI int          atomsz;
extern DLI int_t        *ici_zero;
extern DLI int_t        *ici_one;
extern DLI char         *ici_error;
extern DLI exec_t       *ici_execs;
extern DLI exec_t       *ici_exec;
extern DLI array_t      ici_xs;
extern DLI array_t      ici_os;
extern DLI array_t      ici_vs;

extern DLI long         ici_vsver;

#define NSUBEXP         (10)
extern DLI int  re_bra[(NSUBEXP + 1) * 3];
extern DLI int  re_nbra;

extern DLI volatile int ici_aborted;            /* See exec.c */

extern DLI int  ici_dont_record_line_nums;      /* See lex.c */
extern DLI char *ici_buf;                       /* See buf.h */
extern DLI int  ici_bufz;                       /* See buf.h */

extern DLI type_t       ici_array_type;
extern DLI type_t       ici_catch_type;
extern DLI type_t       ici_exec_type;
extern DLI type_t       set_type;
extern DLI type_t       struct_type;
extern DLI type_t       float_type;
extern DLI type_t       file_type;
extern DLI type_t       ici_func_type;
extern DLI type_t       ici_cfunc_type;
extern DLI type_t       ici_method_type;
extern DLI type_t       forall_type;
extern DLI type_t       int_type;
extern DLI type_t       mark_type;
extern DLI type_t       null_type;
extern DLI type_t       op_type;
extern DLI type_t       pc_type;
extern DLI type_t       ptr_type;
extern DLI type_t       regexp_type;
extern DLI type_t       src_type;
extern DLI type_t       string_type;
extern DLI type_t       parse_type;
extern DLI type_t       ostemp_type;
extern DLI type_t       ici_handle_type;
extern DLI type_t       profilecall_type;
extern DLI type_t       mem_type;

extern DLI ftype_t      stdio_ftype;
extern DLI ftype_t      ici_popen_ftype;
extern DLI ftype_t      ici_parse_ftype;

extern DLI null_t       o_null;
extern DLI mark_t       o_mark;


#ifndef NODEBUGGING
extern DLI debug_t *ici_debug;
#endif

extern char     ici_version_string[];

extern unsigned long const ici_crc_table[256];
extern int              ici_exec_count;

#define ici_null_ret() ici_ret_no_decref(objof(&o_null))

extern char     **smash(char *, int);
extern char     **ssmash(char *, char *);
extern char     *ici_binop_name(int);
extern object_t *ici_evaluate(object_t *, int);
extern object_t *copy_simple(object_t *);
extern object_t *ici_fetch_fail(object_t *, object_t *);
extern slot_t   *find_slot(struct_t **, object_t *);
extern slot_t   *find_raw_slot(struct_t *, object_t *);
extern object_t *get_token(file_t *);
extern object_t *get_value(struct_t *, object_t *);
extern object_t *ici_atom(object_t *, int);
extern object_t *atom_probe(object_t *, object_t ***);
extern int_t    *atom_int(long);
extern int      parse_exec(void);
extern int      parse_module(file_t *, objwsup_t *);
extern int      parse_file(char *, char *, ftype_t *);
extern parse_t  *new_parse(file_t *);
extern array_t  *ici_array_new(ptrdiff_t);
extern mem_t    *ici_mem_new(void *, size_t, int, void (*)());
extern catch_t  *new_catch(object_t *, int, int, int);
extern string_t *ici_str_new_nul_term(char *);
extern string_t *ici_str_get_nul_term(char *);
extern unsigned long    ici_hash_string(object_t *);
extern set_t    *ici_set_new(void);
extern struct_t *ici_struct_new(void);
extern exec_t   *new_exec(void);
extern float_t  *ici_float_new(double);
extern file_t   *new_file(void *, ftype_t *, string_t *, object_t *);
extern func_t   *new_func(void);
extern int_t    *ici_int_new(long);
extern int      ici_interface_check(unsigned long ver, char const *name);
extern string_t *new_string(int);
extern string_t *ici_str_new(char *, int);
extern op_t     *new_op(int (*)(), int, int);
extern pc_t     *new_pc(void);
extern ptr_t    *ici_ptr_new(object_t *, object_t *);
extern src_t    *new_src(int, string_t *);
extern regexp_t *ici_regexp_new(string_t *, int);
extern int      ici_assign_fail(object_t *, object_t *, object_t *);
extern file_t   *ici_sopen(char *, int, object_t *);
extern catch_t  *ici_unwind(void);
extern void     collect(void);
extern unsigned long hash_unique(object_t *);
extern int      cmp_unique(object_t *, object_t *);
extern int      exec_simple(void);
extern void     free_simple(object_t *);
extern int      ici_growarray(array_t *, ptrdiff_t);
extern int      ici_op_binop(void);
extern int      ici_op_mklvalue(void);
extern int      ici_op_onerror(void);
extern int      ici_op_for(void);
extern int      ici_op_andand(void);
extern int      ici_op_switcher(void);
extern int      ici_op_switch(void);
extern int      ici_op_forall(void);
extern int      ici_op_return(void);
extern int      ici_op_call(void);
extern int      ici_op_mkptr(void);
extern int      ici_op_openptr(void);
extern int      ici_op_fetch(void);
extern int      ici_op_unary(void);
extern int      ici_get_last_errno(char *, char *);
extern int      ici_argcount(int);
extern int      ici_argerror(int);
extern array_t  *mk_strarray(char **);
extern void     ici_struct_unassign(struct_t *, object_t *);
extern int      unassign_set(set_t *, object_t *);
extern void     grow_objs(object_t *);
extern char     *objname(char *, object_t *);
extern void     expand_error(int, string_t *);
extern int      lex(parse_t *, array_t *);
extern struct_t *statics_struct(struct_t *, struct_t *);
extern int      f_close(file_t *f);
extern int      ici_ret_with_decref(object_t *);
extern int      ici_int_ret(long);
extern int      ici_ret_no_decref(object_t *);
extern void     uninit_cfunc(void);
extern int      ici_typecheck(char *, ...);
extern int      ici_retcheck(char *, ...);
extern int      ici_op_call(void);
extern int      nptrs(char **);
extern int      ici_badindex(void);
extern int      mkstream(char *, FILE *);
extern int      ici_init(void);
extern void     ici_uninit(void);
extern int      exec_forall(void);
extern exec_t   *ici_new_exec(void);
extern int      compile_expr(array_t *, expr_t *, int);
extern void     uninit_compile(void);
extern file_t   *ici_need_stdin(void);
extern file_t   *ici_need_stdout(void);
extern array_t  *ici_need_path(void);
extern int      set_issubset(set_t *, set_t *);
extern int      set_ispropersubset(set_t *, set_t *);
extern void     ici_reclaim(void);
extern int      ici_str_ret(char *);
extern int      ici_float_ret(double);
extern int      ici_func(object_t *, char *, ...);
extern int      ici_method(object_t *, string_t *, char *, ...);
extern int      ici_funcv(object_t *, object_t *, char *, va_list);
extern int      ici_call(string_t *, char *, ...);
extern int      ici_callv(string_t *, char *, va_list);
extern int      ici_cmkvar(objwsup_t *, char *, int, void *);
extern int      ici_set_val(objwsup_t *, string_t *, int, void *);
extern int      ici_fetch_num(object_t *, object_t *, double *);
extern int      ici_fetch_int(object_t *, object_t *, long *);
extern long     ici_strtol(char const *, char **, int);
extern int      ici_load(string_t *);
extern int      ici_init_path(objwsup_t *externs);
extern int      ici_find_on_path(char [FILENAME_MAX], char *);
extern int      ici_assign_cfuncs(objwsup_t *, cfunc_t *);
extern int      def_cfuncs(cfunc_t *);
extern int      ici_main(int, char **);
extern int      ici_init_sstrings(void);
extern method_t *ici_method_new(object_t *, object_t *);
extern int      ici_get_foreign_source_code(parse_t *, array_t *, int, int, int, int, int, unsigned long *);
extern ici_handle_t *ici_handle_new(void *, string_t *, objwsup_t *);
extern int      ici_register_type(type_t *t);
extern ptrdiff_t ici_array_nels(array_t *);
extern int      ici_grow_stack(array_t *, ptrdiff_t);
extern int      ici_fault_stack(array_t *, ptrdiff_t);
extern void     ici_array_gather(object_t **, array_t *, ptrdiff_t, ptrdiff_t);
extern int      ici_array_push(array_t *, object_t *);
extern int      ici_array_rpush(array_t *, object_t *);
extern object_t *ici_array_pop(array_t *);
extern object_t *ici_array_rpop(array_t *);
extern object_t *ici_array_get(array_t *, ptrdiff_t);
extern void     ici_invalidate_struct_lookaside(struct_t *);
extern void     ici_drop_all_small_allocations(void);
extern int      ici_engine_stack_check(void);
extern void     get_pc(array_t *code, object_t **xs);
extern void     ici_atexit(void (*)(void), wrap_t *);
extern objwsup_t *ici_outermost_writeable_struct(void);
extern objwsup_t *ici_class_new(cfunc_t *cf, objwsup_t *super);
extern objwsup_t *ici_module_new(cfunc_t *cf);
extern int      ici_handle_method_check(object_t *, string_t *, ici_handle_t **, void **);
extern int      ici_method_check(object_t *o, int tcode);
extern unsigned long ici_crc(unsigned long, unsigned char const *, ptrdiff_t);

extern exec_t   *ici_leave(void);
extern void     ici_enter(exec_t *);
extern void     ici_yield(void);
extern int      ici_waitfor(object_t *);
extern int      ici_wakeup(object_t *);
extern int      ici_init_thread_stuff(void);

#ifndef NODEBUGGING
extern int      ici_maind(int, char **, int);
extern DLI int  ici_debug_enabled;
extern int  ici_debug_ign_err;
extern DLI void ici_debug_ignore_errors(void);
extern DLI void ici_debug_respect_errors(void);
#else
/*
 * We let the compiler use it's sense to remove a lot of debug
 * code base on a constant expression for ici_debug_enabled. Just
 * to save on lots of ifdefs.
 */
#define ici_debug_enabled 0
#endif

#ifndef NOSIGNALS
#ifdef SUNOS5
extern volatile sigset_t ici_signals_pending;
#else
extern volatile long    ici_signals_pending;
#endif
extern volatile long    ici_signals_count[];
extern void     ici_signals_init(void);
extern int      ici_signals_invoke_handlers(void);
extern int      ici_signals_blocking_syscall(int);
#else
/*
 * Let compiler remove code without resorting to ifdefs as for debug.
 */
#define ici_signals_pending 0
#define ici_signals_blocking_syscall(x)
#define ici_signals_invoke_handlers()
#endif

#ifdef BSD
extern int      select();
#endif

#ifdef  sun
double strtod(const char *ptr, char **endptr);
#endif

#ifndef NOTRACE
extern void     trace_pcall(object_t *);
#endif

/*
 * End of ici.h export. --ici.h-end--
 */

extern int              ici_natoms;
extern void             ici_grow_atoms(ptrdiff_t newz);
extern int              ici_supress_collect;

#include "alloc.h"

#if defined(_WIN32) && !defined(NDEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#endif /* ICI_FWD_H */
