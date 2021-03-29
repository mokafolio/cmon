#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_resolver.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tokens.h>
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
                  ##__VA_ARGS__);                                                                  \
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

static inline cmon_src * _fr_src(_file_resolver * _fr)
{
    return cmon_modules_src(_fr->mods);
}

static inline cmon_tokens * _fr_tokens(_file_resolver * _fr)
{
    return cmon_src_tokens(_fr_src(_fr), _fr->src_file_idx);
}

static inline cmon_bool _check_redec(_file_resolver * _fr, cmon_idx _scope, cmon_idx _name_tok)
{
    cmon_idx s;
    cmon_str_view str_view;

    str_view = cmon_tokens_str_view(_fr_tokens(_fr), _name_tok);
    if (cmon_is_valid_idx(s = cmon_symbols_find(_fr->symbols, _scope, str_view)))
    {
        _fr_err(_fr,
                _name_tok,
                "redeclaration of '%.*s'",
                str_view.end - str_view.begin,
                str_view.begin);
        return cmon_true;
    }
    return cmon_false;
}

static inline cmon_idx _add_global_var_name(cmon_resolver * _r,
                                            _file_resolver * _fr,
                                            cmon_idx _name_tok,
                                            cmon_bool _is_pub,
                                            cmon_bool _is_mut,
                                            cmon_idx _ast_idx)
{
    if (!_check_redec(_fr, _fr->file_scope, _name_tok))
    {
        return cmon_symbols_scope_add_var(_r->symbols,
                                          _r->global_scope,
                                          cmon_tokens_str_view(_fr_tokens(_fr), _name_tok),
                                          CMON_INVALID_IDX,
                                          _is_pub,
                                          _is_mut,
                                          _fr->src_file_idx,
                                          _ast_idx);
    }
    return CMON_INVALID_IDX;
}

cmon_bool cmon_resolver_top_lvl_pass(cmon_resolver * _r, cmon_idx _file_idx)
{
    cmon_src * src;
    cmon_idx src_file_idx;
    cmon_ast * ast;
    cmon_tokens * tokens;
    cmon_ast_iter it;
    cmon_idx idx, root_block;
    cmon_bool is_first_stmt;
    _file_resolver * fr;

    src = cmon_modules_src(_r->mods);
    src_file_idx = cmon_modules_src_file(_r->mods, _r->mod_idx, _file_idx);
    ast = cmon_src_ast(src, src_file_idx);
    tokens = cmon_src_tokens(src, src_file_idx);
    is_first_stmt = cmon_true;
    fr = &_r->file_resolvers[_file_idx];

    assert(ast);
    assert(tokens);
    root_block = cmon_ast_root_block(ast);

    it = cmon_ast_block_iter(ast, root_block);
    while (cmon_is_valid_idx(idx = cmon_ast_iter_next(ast, &it)))
    {
        if (is_first_stmt)
        {
            is_first_stmt = cmon_false;
            // make sure every file declares the module its part of at the top
            if (cmon_ast_kind(ast, idx) != cmon_astk_module)
            {
                _fr_err(fr, cmon_ast_token(ast, idx), "missing module statement");
            }
            else
            {
                // make sure the name matches the currently compiling module
                cmon_idx mod_name_tok;
                cmon_str_view name_str_view;
                mod_name_tok = cmon_ast_module_name_tok(ast, idx);
                name_str_view = cmon_tokens_str_view(tokens, mod_name_tok);
                if (cmon_str_view_c_str_cmp(name_str_view,
                                            cmon_modules_name(_r->mods, _r->mod_idx)) != 0)
                {
                    _fr_err(fr,
                            mod_name_tok,
                            "module '%s' expected, got '%.*s'",
                            cmon_modules_name(_r->mods, _r->mod_idx),
                            name_str_view.end - name_str_view.begin,
                            name_str_view.begin);
                }
            }
        }
        else
        {
            cmon_astk kind;
            kind = cmon_ast_kind(ast, idx);
            if (kind == cmon_astk_import)
            {
                cmon_idx ipp_idx, imod_idx, alias_tok_idx;
                cmon_ast_iter pit = cmon_ast_import_iter(ast, idx);
                while (cmon_is_valid_idx(ipp_idx = cmon_ast_iter_next(ast, &pit)))
                {
                    cmon_str_view path = cmon_ast_import_pair_path(ast, ipp_idx);

                    if (!cmon_is_valid_idx(imod_idx = cmon_modules_find(fr->mods, path)))
                    {
                        _fr_err(fr,
                                cmon_ast_import_pair_path_begin(ast, ipp_idx),
                                "could not find module '%.*s'",
                                path.end - path.begin,
                                path.begin);
                    }
                    else if (!_check_redec(fr,
                                           fr->file_scope,
                                           alias_tok_idx =
                                               cmon_ast_import_pair_alias(ast, ipp_idx)))
                    {
                        cmon_modules_add_dep(fr->mods, fr->mod_idx, imod_idx);
                        cmon_symbols_scope_add_import(fr->symbols,
                                                      fr->file_scope,
                                                      cmon_tokens_str_view(tokens, alias_tok_idx),
                                                      imod_idx,
                                                      fr->src_file_idx,
                                                      idx);
                    }
                }
            }
            else if (kind == cmon_astk_var_decl)
            {
                _add_global_var_name(_r,
                                     fr,
                                     cmon_ast_var_decl_name_tok(ast, idx),
                                     cmon_ast_var_decl_is_pub(ast, idx),
                                     cmon_ast_var_decl_is_mut(ast, idx),
                                     idx);
            }
            else if (kind == cmon_astk_var_decl_list)
            {
                cmon_ast_iter name_it;
                cmon_idx name_tok_idx, expr_idx;
                cmon_bool is_pub, is_mut;

                name_it = cmon_ast_var_decl_list_names_iter(ast, idx);
                is_pub = cmon_ast_var_decl_list_is_pub(ast, idx);
                is_mut = cmon_ast_var_decl_list_is_mut(ast, idx);
                while (cmon_is_valid_idx(name_tok_idx = cmon_ast_iter_next(ast, &name_it)))
                {
                    _add_global_var_name(_r, fr, name_tok_idx, is_pub, is_mut, idx);
                }
            }
            else if (kind == cmon_astk_struct_decl)
            {
            }
            else
            {
                assert(cmon_false);
                //@TODO: Unexpected top lvl statement? (I guess it would have already failed
                // parsing?)
            }
        }
    }

    return cmon_false;
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
