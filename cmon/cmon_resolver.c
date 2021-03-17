#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_resolver.h>
#include <cmon/cmon_str_builder.h>
#include <setjmp.h>
#include <stdarg.h>

typedef struct
{
    cmon_symbols * symbols;
    cmon_modules * mods;
    cmon_idx file_scope;
    cmon_idx mod_idx;
    cmon_idx src_file_idx;
    cmon_str_builder * str_builder;
    cmon_dyn_arr(cmon_err_report) errs;
    size_t max_errors;
    jmp_buf err_jmp;
} _file_resolver;

typedef struct cmon_resolver
{
    cmon_allocator * alloc;
    cmon_symbols * symbols;
    cmon_idx global_scope;
    cmon_types * types;
    cmon_modules * mods;
    cmon_idx mod_idx;
    cmon_dyn_arr(_file_resolver) file_resolvers;
    cmon_dyn_arr(void *) dep_buffer;
    cmon_dyn_arr(cmon_err_report) errs;
} cmon_resolver;

static inline void _emit_err(cmon_str_builder * _str_builder,
                             cmon_dyn_arr(cmon_err_report) * _out_errs,
                             cmon_src * _src,
                             cmon_idx _src_file_idx,
                             cmon_idx _tok_idx,
                             size_t _max_errors,
                             jmp_buf * _jmp,
                             const char * _fmt,
                             ...)
{
    va_list args;
    cmon_tokens * toks;
    va_start(args, _fmt);
    cmon_str_builder_clear(_str_builder);

    toks = cmon_src_tokens(_src, _src_file_idx);
    cmon_str_builder_append_fmt_v(_str_builder, _fmt, args);
    cmon_dyn_arr_append(_out_errs,
                        cmon_err_report_make(cmon_src_filename(_src, _src_file_idx),
                                             cmon_tokens_line(toks, _tok_idx),
                                             cmon_tokens_line_offset(toks, _tok_idx),
                                             cmon_str_builder_c_str(_str_builder)));
    va_end(args);
    if (cmon_dyn_arr_count(_out_errs) >= _max_errors)
        longjmp(*_jmp, 1);
}

#define _fr_err(_fr, _tok, _fmt, ...)                                                              \
    do                                                                                             \
    {                                                                                              \
        _emit_err(_fr->str_builder,                                                                \
                  &_fr->errs,                                                                      \
                  cmon_modules_src(_fr->mods),                                                     \
                  _fr->src_file_idx,                                                               \
                  _tok,                                                                            \
                  _fr->max_errors,                                                                 \
                  &_fr->err_jmp,                                                                   \
                  _fmt,                                                                            \
                  ...);                                                                            \
    } while (0)

cmon_resolver * cmon_resolver_create(cmon_allocator * _alloc)
{
    cmon_resolver * ret;
    ret = CMON_CREATE(_alloc, cmon_resolver);
    ret->alloc = _alloc;
    ret->symbols = NULL;
    ret->global_scope = CMON_INVALID_IDX;
    ret->types = NULL;
    ret->mods = NULL;
    ret->mod_idx = CMON_INVALID_IDX;
    cmon_dyn_arr_init(&ret->file_resolvers, _alloc, 8);
    cmon_dyn_arr_init(&ret->dep_buffer, _alloc, 32);
    cmon_dyn_arr_init(&ret->errs, _alloc, 16);
    return ret;
}

void cmon_resolver_destroy(cmon_resolver * _r)
{
    size_t i;

    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
    }
    cmon_dyn_arr_dealloc(&_r->errs);
    cmon_dyn_arr_dealloc(&_r->dep_buffer);
    cmon_dyn_arr_dealloc(&_r->file_resolvers);
    CMON_DESTROY(_r->alloc, _r);
}

void cmon_resolver_set_input(cmon_resolver * _r,
                             cmon_types * _types,
                             cmon_symbols * _symbols,
                             cmon_modules * _mods,
                             cmon_idx _mod_idx)
{
    assert(!cmon_is_valid_idx(_r->global_scope));
    _r->types = _types;
    _r->symbols = _symbols;
    _r->mods = _mods;
    _r->mod_idx = _mod_idx;
    _r->global_scope = cmon_symbols_scope_begin(_r->symbols, CMON_INVALID_IDX);
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
