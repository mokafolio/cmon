#ifndef CMON_CMON_RESOLVER_H
#define CMON_CMON_RESOLVER_H

#include <cmon/cmon_ast.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_ir.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_symbols.h>
#include <cmon/cmon_types.h>

typedef struct cmon_resolver cmon_resolver;

typedef struct cmon_file_ast_pair
{
    cmon_idx src_file_idx;
    cmon_idx ast_idx;
} cmon_file_ast_pair;

CMON_API cmon_resolver * cmon_resolver_create(cmon_allocator * _alloc, size_t _max_errors);
CMON_API void cmon_resolver_destroy(cmon_resolver * _r);
CMON_API void cmon_resolver_set_input(cmon_resolver * _r,
                                      cmon_src * _src,
                                      cmon_types * _types,
                                      cmon_symbols * _symbols,
                                      cmon_modules * _mods,
                                      cmon_idx _mod_idx);

// these are the individual steps to compile the whole module in the order they need to be executed
CMON_API cmon_bool cmon_resolver_top_lvl_pass(cmon_resolver * _r, cmon_idx _file_idx);
CMON_API cmon_bool cmon_resolver_finalize_top_lvl_names(cmon_resolver * _r);
CMON_API cmon_bool cmon_resolver_usertypes_pass(cmon_resolver * _r, cmon_idx _file_idx);
CMON_API cmon_bool cmon_resolver_globals_pass(cmon_resolver * _r);
CMON_API cmon_bool cmon_resolver_usertypes_def_expr_pass(cmon_resolver * _r, cmon_idx _file_idx);
CMON_API cmon_bool cmon_resolver_circ_pass(cmon_resolver * _r);
CMON_API cmon_bool cmon_resolver_main_pass(cmon_resolver * _r, cmon_idx _file_idx);
CMON_API cmon_ir * cmon_resolver_finalize(cmon_resolver * _r);

// retrieve errors
CMON_API cmon_bool cmon_resolver_has_errors(cmon_resolver * _r);
CMON_API cmon_bool cmon_resolver_errors(cmon_resolver * _r,
                                        cmon_err_report ** _out_errs,
                                        size_t * _out_count);

#endif // CMON_CMON_RESOLVER_H
