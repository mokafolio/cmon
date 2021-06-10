#ifndef CMON_CMON_ARGPARSE_H
#define CMON_CMON_ARGPARSE_H

#include <cmon/cmon_allocator.h>

#define CMON_ARG_KEY_MAX CMON_FILENAME_MAX
#define CMON_ARG_VAL_MAX CMON_PATH_MAX
#define CMON_ARG_HLP_MAX CMON_ARG_VAL_MAX

// typedef char cmon_arg_val[CMON_ARG_VAL_MAX];

// typedef struct
// {
//     cmon_allocator * alloc;
//     char key_short[CMON_ARG_KEY_MAX];
//     char key_long[CMON_ARG_KEY_MAX];
//     char help[CMON_ARG_VAL_MAX];
//     cmon_bool expects_value;
//     cmon_bool accepts_any;
//     cmon_dyn_arr(char*) possible_vals;
//     cmon_arg_val val;
//     int default_idx;
//     cmon_bool was_set;
// } cmon_arg;

// typedef struct
// {
//     cmon_allocator * alloc;
//     cmon_dyn_arr(cmon_arg *) args;
//     char name[CMON_ARG_KEY_MAX];
// } cmon_argparse;

typedef struct cmon_argparse cmon_argparse;

// for simplicity, all argparse functions will simply panic on error.
CMON_API cmon_argparse * cmon_argparse_create(cmon_allocator * _alloc, const char * _cmd_name);
CMON_API void cmon_argparse_destroy(cmon_argparse * _a);
CMON_API void cmon_argparse_parse(cmon_argparse * _a, const char ** _args, size_t _count);
CMON_API cmon_idx cmon_argparse_add_arg(cmon_argparse * _a,
                                        const char * _key_short,
                                        const char * _key_long,
                                        cmon_bool _expects_value,
                                        const char * _help);
CMON_API void cmon_argparse_add_possible_val(cmon_argparse * _a, cmon_idx _arg, const char * _val, cmon_bool _is_default);
CMON_API const char * cmon_argparse_value(cmon_argparse * _a, const char * _key);
CMON_API cmon_bool cmon_argparse_is_set(cmon_argparse * _a, const char * _key);
CMON_API cmon_idx cmon_argparse_find(cmon_argparse * _a, const char * _key);
CMON_API void cmon_argparse_print_help(cmon_argparse * _a);

#endif // CMON_CMON_ARGPARSE_H
