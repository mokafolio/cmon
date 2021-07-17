#ifndef CMON_CMON_ARGPARSE_H
#define CMON_CMON_ARGPARSE_H

#include <cmon/cmon_allocator.h>

#define CMON_ARG_KEY_MAX CMON_FILENAME_MAX
#define CMON_ARG_VAL_MAX CMON_PATH_MAX
#define CMON_ARG_HLP_MAX CMON_ARG_VAL_MAX

typedef struct cmon_argparse cmon_argparse;

// for simplicity, all argparse functions will simply panic on error.
CMON_API cmon_argparse * cmon_argparse_create(cmon_allocator * _alloc, const char * _cmd_name);
CMON_API void cmon_argparse_destroy(cmon_argparse * _a);
CMON_API void cmon_argparse_parse(cmon_argparse * _a, const char ** _args, size_t _count);

CMON_API cmon_idx cmon_argparse_add_cmd(cmon_argparse * _a, const char * _name, const char * _help);

CMON_API cmon_idx cmon_argparse_add_arg(cmon_argparse * _a,
                                        cmon_idx _cmd,
                                        const char * _key_short,
                                        const char * _key_long,
                                        const char * _help,
                                        cmon_bool _expects_value,
                                        cmon_bool _required);

CMON_API void cmon_argparse_cmd_add_arg(cmon_argparse * _a, cmon_idx _cmd, cmon_idx _arg);

CMON_API void cmon_argparse_add_possible_val(cmon_argparse * _a,
                                             cmon_idx _arg,
                                             const char * _val,
                                             cmon_bool _is_default);
CMON_API const char * cmon_argparse_value(cmon_argparse * _a, const char * _key);
CMON_API cmon_idx cmon_argparse_cmd(cmon_argparse * _a);
CMON_API cmon_bool cmon_argparse_is_arg_set(cmon_argparse * _a, const char * _key);
CMON_API cmon_idx cmon_argparse_find_arg(cmon_argparse * _a, const char * _key);
CMON_API void cmon_argparse_print_help(cmon_argparse * _a);

#endif // CMON_CMON_ARGPARSE_H
