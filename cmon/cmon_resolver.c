#include <cmon/cmon_resolver.h>

typedef struct
{

} _file_resolver;

typedef struct cmon_resolver
{
    cmon_modules * mods;
    cmon_idx mod_idx;
    cmon_dyn_arr(_file_resolver) file_resolvers;
    cmon_dyn_arr(void *) dep_buffer;
    cmon_dyn_arr(cmon_err_report) errs;
} cmon_resolver;

cmon_resolver * cmon_resolver_create(cmon_allocator * _alloc)
{

}

void cmon_resolver_destroy(cmon_resolver * _r)
{

}

cmon_err_report cmon_resolver_err(cmon_resolver * _r)
{

}

void cmon_resolver_set_input(cmon_resolver * _r, cmon_modules * _mods, cmon_idx _mod_idx)
{

}

cmon_ast * cmon_resolver_top_lvl_pass(cmon_resolver * _r, cmon_idx _file_idx)
{

}

cmon_bool cmon_resolver_circ_pass(cmon_resolver * _r)
{

}

cmon_bool cmon_resolver_globals_pass(cmon_resolver * _r)
{

}

cmon_bool cmon_resolver_main_pass(cmon_resolver * _r, cmon_idx _file_idx)
{

}

cmon_bool cmon_resolver_errors(cmon_resolver * _r, cmon_err_report * _out_errs, size_t * _out_count)
{

}
