#ifndef CMON_CMON_RESOLVER_H
#define CMON_CMON_RESOLVER_H

#include <cmon/cmon_ast.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_symbols.h>
#include <cmon/cmon_types.h>

typedef struct cmon_resolver cmon_resolver;

CMON_API cmon_resolver * cmon_resolver_create(cmon_allocator * _alloc);
CMON_API void cmon_resolver_destroy(cmon_resolver * _r);
CMON_API void cmon_resolver_set_input(cmon_resolver * _r,
                                      cmon_types * _types,
                                      cmon_symbols * symbols,
                                      cmon_modules * _mods,
                                      cmon_idx _mod_idx);
CMON_API cmon_bool cmon_resolver_top_lvl_pass(cmon_resolver * _r, cmon_idx _file_idx);
CMON_API cmon_bool cmon_resolver_circ_pass(cmon_resolver * _r);
CMON_API cmon_bool cmon_resolver_globals_pass(cmon_resolver * _r);
CMON_API cmon_bool cmon_resolver_main_pass(cmon_resolver * _r, cmon_idx _file_idx);
CMON_API cmon_bool cmon_resolver_errors(cmon_resolver * _r,
                                        cmon_err_report * _out_errs,
                                        size_t * _out_count);

#endif // CMON_CMON_RESOLVER_H
