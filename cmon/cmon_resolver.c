#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_handler.h>
#include <cmon/cmon_idx_buf_mng.h>
#include <cmon/cmon_resolver.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tokens.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>

typedef struct
{
    cmon_idx ast_idx;
    cmon_idx type_idx;
} _ast_type_pair;

typedef struct
{
    cmon_resolver * resolver;
    cmon_idx file_scope;
    cmon_idx src_file_idx;
    cmon_idx_buf_mng * idx_buf_mng;
    cmon_dyn_arr(_ast_type_pair) type_decls; // types declared in the file
    // these are all symbol indices
    cmon_dyn_arr(cmon_idx) global_var_decls;
    cmon_dyn_arr(cmon_idx) global_alias_decls;
    cmon_dyn_arr(cmon_idx) external_variables; // external variables and functions used
    // all functions declarations at file/global lvl
    cmon_dyn_arr(cmon_idx) global_fns;
    // all local function declarations in the file (i.e. not top lvl)
    cmon_dyn_arr(cmon_idx) local_fns;
    cmon_idx * resolved_types; // maps ast expr idx to type
    cmon_idx main_fn_sym;
    cmon_err_handler * err_handler;
    jmp_buf err_jmp;
} _file_resolver;

typedef struct cmon_resolver
{
    cmon_allocator * alloc;
    cmon_src * src;
    cmon_symbols * symbols;
    cmon_idx global_scope;
    cmon_types * types;
    cmon_modules * mods;
    cmon_idx mod_idx;
    cmon_dep_graph * dep_graph;
    cmon_idx_buf_mng * idx_buf_mng;
    cmon_str_builder * str_builder;
    cmon_dyn_arr(_file_resolver) file_resolvers;
    cmon_dyn_arr(cmon_idx) dep_buffer;
    // all types used by the module in dependency order
    cmon_dyn_arr(cmon_idx) sorted_types;
    cmon_dyn_arr(cmon_idx) symbol_ir_map;
    cmon_dyn_arr(cmon_err_report) errs;
    size_t max_errors;
    cmon_err_handler * err_handler;
    // flag that is set to true during global variable type resolve pass
    cmon_bool global_type_pass;
    cmon_dyn_arr(cmon_idx) globals_pass_stack;
    // ir builder to generate IR
    cmon_irb * ir_builder;
    cmon_idx main_fn_sym;
} cmon_resolver;

// static inline void _emit_err(cmon_str_builder * _str_builder,
//                              cmon_dyn_arr(cmon_err_report) * _out_errs,
//                              cmon_src * _src,
//                              cmon_idx _src_file_idx,
//                              cmon_idx _tok_idx,
//                              size_t _max_errors,
//                              jmp_buf * _jmp,
//                              const char * _fmt,
//                              ...)
// {
//     va_list args;
//     cmon_tokens * toks;
//     va_start(args, _fmt);
//     cmon_str_builder_clear(_str_builder);

//     toks = cmon_src_tokens(_src, _src_file_idx);
//     cmon_str_builder_append_fmt_v(_str_builder, _fmt, args);
//     cmon_dyn_arr_append(_out_errs,
//                         cmon_err_report_make(cmon_src_filename(_src, _src_file_idx),
//                                              cmon_tokens_line(toks, _tok_idx),
//                                              cmon_tokens_line_offset(toks, _tok_idx),
//                                              cmon_str_builder_c_str(_str_builder)));
//     va_end(args);

//     if (cmon_dyn_arr_count(_out_errs) >= _max_errors)
//     {
//         longjmp(*_jmp, 1);
//     }
// }

// #define _fr_err(_fr, _tok, _fmt, ...)                                                              \
//     do                                                                                             \
//     {                                                                                              \
//         _emit_err(_fr->str_builder,                                                                \
//                   &_fr->errs,                                                                      \
//                   _fr->resolver->src,                                                              \
//                   _fr->src_file_idx,                                                               \
//                   _tok,                                                                            \
//                   _fr->resolver->max_errors,                                                       \
//                   &_fr->err_jmp,                                                                   \
//                   _fmt,                                                                            \
//                   ##__VA_ARGS__);                                                                  \
//     } while (0)

// #define _err(_fr, _tok, _fmt, ...) do { cmon_err_handler_err(_fr->err_handler, _tok, _fmt,
// ##__VA_ARGS__); } while(0)

#define _fr_err(_fr, _tok_first, _tok_of_interest, _tok_last, _fmt, ...)                                             \
    do                                                                                             \
    {                                                                                              \
        cmon_err_handler_err(_fr->err_handler,                                                     \
                             cmon_true,                                                            \
                             _fr->src_file_idx,                                                    \
                             _tok_first,                                                           \
                             _tok_of_interest,                                                           \
                             _tok_last,                                                            \
                             _fmt,                                                                 \
                             ##__VA_ARGS__);                                                       \
    } while (0)

static inline void _unexpected_ast_panic()
{
    assert(0);
    cmon_panic("Unexpected ast node. This is a bug :(");
}

static inline cmon_tokens * _fr_tokens(_file_resolver * _fr)
{
    return cmon_src_tokens(_fr->resolver->src, _fr->src_file_idx);
}

static inline cmon_ast * _fr_ast(_file_resolver * _fr)
{
    return cmon_src_ast(_fr->resolver->src, _fr->src_file_idx);
}

static inline cmon_idx _resolve_expr(_file_resolver * _fr,
                                     cmon_idx _scope,
                                     cmon_idx _ast_idx,
                                     cmon_idx _lh_type);
static inline void _resolve_stmt(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx);
static inline void _resolve_var_decl(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx);
static inline void _resolve_alias(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx);

static inline cmon_idx _remove_paran(_file_resolver * _fr, cmon_idx _ast_idx)
{
    while (cmon_ast_kind(_fr_ast(_fr), _ast_idx) == cmon_astk_paran_expr)
        _ast_idx = cmon_ast_paran_expr(_fr_ast(_fr), _ast_idx);

    return _ast_idx;
}

static inline cmon_bool _is_literal(_file_resolver * _fr, cmon_idx _ast_idx)
{
    cmon_astk kind;

    _ast_idx = _remove_paran(_fr, _ast_idx);
    kind = cmon_ast_kind(_fr_ast(_fr), _ast_idx);

    if (kind == cmon_astk_float_literal || kind == cmon_astk_string_literal ||
        kind == cmon_astk_int_literal || kind == cmon_astk_bool_literal)
    {
        return cmon_true;
    }

    if (kind == cmon_astk_binary)
        return _is_literal(_fr, cmon_ast_binary_left(_fr_ast(_fr), _ast_idx)) &&
               _is_literal(_fr, cmon_ast_binary_right(_fr_ast(_fr), _ast_idx));
    else if (kind == cmon_astk_prefix)
        return _is_literal(_fr, cmon_ast_prefix_expr(_fr_ast(_fr), _ast_idx));

    return cmon_false;
}

static inline cmon_bool _is_indexable(_file_resolver * _fr, cmon_idx _type)
{
    cmon_typek kind =
        cmon_types_kind(_fr->resolver->types, cmon_types_remove_ptr(_fr->resolver->types, _type));
    return kind == cmon_typek_array || kind == cmon_typek_view || kind == cmon_typek_tuple;
}

static inline cmon_bool _check_redec(_file_resolver * _fr, cmon_idx _scope, cmon_idx _name_tok)
{
    cmon_idx s;
    cmon_str_view str_view;

    str_view = cmon_tokens_str_view(_fr_tokens(_fr), _name_tok);
    if (cmon_is_valid_idx(s = cmon_symbols_find(_fr->resolver->symbols, _scope, str_view)))
    {
        _fr_err(_fr,
                _name_tok,
                _name_tok,
                _name_tok,
                "redeclaration of '%.*s'",
                str_view.end - str_view.begin,
                str_view.begin);
        return cmon_true;
    }
    return cmon_false;
}

static inline cmon_bool _validate_conversion(
    _file_resolver * _fr, cmon_idx _tok_first, cmon_idx _tok_last, cmon_idx _from, cmon_idx _to)
{
    cmon_types * t = _fr->resolver->types;

    if (cmon_types_is_float(t, _from) && cmon_types_is_int(t, _to))
    {
        _fr_err(_fr,
            _tok_first,
                _tok_first,
                _tok_last,
                "truncating '%s' to '%s'",
                cmon_types_name(t, _from),
                cmon_types_name(t, _to));
        return cmon_true;
    }

    // assert(0);
    cmon_typek fkind = cmon_types_kind(t, _from);
    cmon_typek tkind = cmon_types_kind(t, _to);

    // allow implicit conversion of pointer types that only differ in mutability. i.e. mut to not
    // mut works implicitly but not the other way around.
    if (fkind == cmon_typek_ptr && tkind == cmon_typek_ptr)
    {
        cmon_idx ft = cmon_types_ptr_type(t, _from);
        cmon_idx tt = cmon_types_ptr_type(t, _to);
        cmon_bool fmut = cmon_types_ptr_is_mut(t, _from);
        cmon_bool tmut = cmon_types_ptr_is_mut(t, _to);

        if ((ft == tt || tt == cmon_types_builtin_void(t)) && (fmut || (!fmut && !tmut)))
            return cmon_false;
    }

    //@TODO: do the same we did for ptrs for views once we get there
    //@TODO: Handle noinit case for pointers? (i.e. should noinit be allowed for ptrs?)

    // otherwise we simply check for equality of the type infos
    if (_from != _to)
    {
        _fr_err(_fr,
            _tok_first,
                _tok_first,
                _tok_last,
                "cannot convert '%s' to '%s'",
                cmon_types_unique_name(t, _from),
                cmon_types_unique_name(t, _to));
        return cmon_true;
    }

    return cmon_false;
}

static inline cmon_bool _validate_lvalue_expr(_file_resolver * _fr,
                                              cmon_idx _expr_idx,
                                              cmon_idx _type_idx,
                                              cmon_bool _needs_to_be_mut,
                                              cmon_bool * _out_is_mut)
{
    cmon_astk kind;
    cmon_symk skind;
    cmon_idx sym;
    cmon_str_view name;

    _expr_idx = _remove_paran(_fr, _expr_idx);
    kind = cmon_ast_kind(_fr_ast(_fr), _expr_idx);

    if (kind == cmon_astk_ident)
    {
        sym = cmon_ast_ident_sym(_fr_ast(_fr), _expr_idx);
        // the expression must have been resolved and the symbol hence set
        assert(cmon_is_valid_idx(sym));
        skind = cmon_symbols_kind(_fr->resolver->symbols, sym);
        if (skind == cmon_symk_var)
        {
            if (_out_is_mut || _needs_to_be_mut)
            {
                cmon_bool is_mut;

                is_mut = cmon_symbols_var_is_mut(_fr->resolver->symbols, sym);
                if (_out_is_mut)
                    *_out_is_mut = is_mut;

                if (_needs_to_be_mut && !is_mut)
                {
                    name = cmon_ast_ident_name(_fr_ast(_fr), _expr_idx);
                    _fr_err(_fr,
                            cmon_ast_token(_fr_ast(_fr), _expr_idx),
                            cmon_ast_token(_fr_ast(_fr), _expr_idx),
                            cmon_ast_token_last(_fr_ast(_fr), _expr_idx),
                            "variable '%.*s' is not mutable",
                            name.end - name.begin,
                            name.begin);
                    return cmon_true;
                }
            }
        }
        else
        {
            const char * kind;
            if (skind == cmon_symk_type)
            {
                kind = "type identifier";
            }
            else if (skind == cmon_symk_import)
            {
                kind = "module identifier";
            }
            else
            {
                assert(cmon_false);
            }

            _fr_err(_fr,
                    cmon_ast_token(_fr_ast(_fr), _expr_idx),
                    cmon_ast_token(_fr_ast(_fr), _expr_idx),
                    cmon_ast_token_last(_fr_ast(_fr), _expr_idx),
                    "lvalue expression expected, got %s",
                    kind);
            return cmon_true;
        }
    }
    else if (kind == cmon_astk_deref)
    {
        if (_needs_to_be_mut || _out_is_mut)
        {
            cmon_idx type;
            cmon_typek tkind;
            cmon_bool is_mut;

            // do
            // {
            // _expr_idx = cmon_ast_deref_expr(_fr_ast(_fr), _expr_idx);
            // type = _fr->resolved_types[_expr_idx];
            // assert(cmon_is_valid_idx(type));
            // tkind = cmon_types_kind(_fr->resolver->types, type);
            // assert(tkind == cmon_typek_ptr);
            // if (!cmon_types_ptr_is_mut(_fr->resolver->types, type))
            // {
            //     _fr_err(_fr,
            //             cmon_ast_token(_fr_ast(_fr), _expr_idx),
            //             "dereferenced pointer in lvalue expression is not mutable");
            //     return cmon_true;
            // }
            //     kind = cmon_ast_kind(_fr_ast(_fr), _expr_idx);
            // } while (kind == cmon_astk_deref);

            // mut a := 1
            // b := &a
            // c := &b //* *mut a
            // **c = 2; //resolved type for **c should be

            // get the innermost deref and check that the final deref dereferences a mutable ptr
            do
            {
                _expr_idx = cmon_ast_deref_expr(_fr_ast(_fr), _expr_idx);
                type = _fr->resolved_types[_expr_idx];
                assert(cmon_is_valid_idx(type));
                tkind = cmon_types_kind(_fr->resolver->types, type);
                assert(tkind == cmon_typek_ptr);
                kind = cmon_ast_kind(_fr_ast(_fr), _expr_idx);
            } while (kind == cmon_astk_deref);

            is_mut = cmon_types_ptr_is_mut(_fr->resolver->types, type);

            if (_out_is_mut)
                *_out_is_mut = is_mut;

            if (_needs_to_be_mut && !is_mut)
            {
                _fr_err(_fr,
                        cmon_ast_token(_fr_ast(_fr), _expr_idx),
                        cmon_ast_token(_fr_ast(_fr), _expr_idx),
                        cmon_ast_token_last(_fr_ast(_fr), _expr_idx),
                        "dereferenced pointer in lvalue expression is not mutable");
                return cmon_true;
            }
        }
    }
    else
    {
        //@TODO: add more context to error...
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), _expr_idx),
                cmon_ast_token(_fr_ast(_fr), _expr_idx),
                cmon_ast_token_last(_fr_ast(_fr), _expr_idx),
                "lvalue expression expected");
        return cmon_true;
    }

    return cmon_false;
}

static inline void _add_unique_idx(cmon_dyn_arr(cmon_idx) * _arr, cmon_idx _idx)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(_arr); ++i)
    {
        if ((*_arr)[i] == _idx)
        {
            return;
        }
    }
    cmon_dyn_arr_append(_arr, _idx);
}

// static inline void _add_implicit_type(_file_resolver * _fr, cmon_idx _type_idx)
// {
//     _add_unique_idx(&_fr->implicit_types, _type_idx);
// }

static inline cmon_idx _resolve_parsed_type(_file_resolver * _fr,
                                            cmon_idx _scope,
                                            cmon_idx _ast_idx)
{
    cmon_astk kind;
    cmon_symk symk;
    cmon_idx ret, mod_tok, mod_sym, mod_idx, name_tok, type_sym, lookup_scope;
    cmon_str_view mod_str_view, name_str_view;
    cmon_ast * ast;

    ast = _fr_ast(_fr);
    kind = cmon_ast_kind(ast, _ast_idx);
    if (kind == cmon_astk_type_named)
    {
        mod_tok = cmon_ast_type_named_module_tok(ast, _ast_idx);
        if (cmon_is_valid_idx(mod_tok))
        {
            mod_str_view = cmon_tokens_str_view(_fr_tokens(_fr), mod_tok);
            mod_sym = cmon_symbols_find(_fr->resolver->symbols, _scope, mod_str_view);
            if (cmon_is_valid_idx(mod_sym))
            {
                symk = cmon_symbols_kind(_fr->resolver->symbols, mod_sym);
                if (symk == cmon_symk_import)
                {
                    mod_idx = cmon_symbols_import_module(_fr->resolver->symbols, mod_sym);
                }
                else
                {
                    _fr_err(_fr,
                            mod_tok,
                            mod_tok,
                            mod_tok,
                            "'%s' is not a module",
                            cmon_symbols_name(_fr->resolver->symbols, mod_sym));
                }
            }
            else
            {
                _fr_err(_fr,
                        mod_tok,
                        mod_tok,
                        mod_tok,
                        "module '%.*s' is not defined",
                        mod_str_view.end - mod_str_view.begin,
                        mod_str_view.begin);
                return CMON_INVALID_IDX;
            }
        }
        else
        {
            mod_idx = _fr->resolver->mod_idx;
        }

        name_tok = cmon_ast_type_named_name_tok(ast, _ast_idx);
        name_str_view = cmon_tokens_str_view(_fr_tokens(_fr), name_tok);
        // if the type has no module specified, look in the current scope for the type
        if (mod_idx == _fr->resolver->mod_idx)
        {
            lookup_scope = _scope;
        }
        // otherwise look in the global scope of the specified module
        else
        {
            lookup_scope = cmon_modules_global_scope(_fr->resolver->mods, mod_idx);
            // sanity checks (These are a little too insane maybe?)
            assert(cmon_is_valid_idx(lookup_scope));
            assert(cmon_symbols_scope_is_global(_fr->resolver->symbols, lookup_scope));
        }

        type_sym = cmon_symbols_find(_fr->resolver->symbols, lookup_scope, name_str_view);
        if (cmon_is_valid_idx(type_sym))
        {
            cmon_symk sk = cmon_symbols_kind(_fr->resolver->symbols, type_sym);
            // check if a global alias currently being resolved is recursive
            if (_fr->resolver->global_type_pass && sk == cmon_symk_alias &&
                cmon_symbols_scope_is_file(_fr->resolver->symbols, _scope))
            {
                size_t i;
                for (i = 0; i < cmon_dyn_arr_count(&_fr->resolver->globals_pass_stack); ++i)
                {
                    cmon_str_view a = cmon_symbols_name(_fr->resolver->symbols,
                                                        _fr->resolver->globals_pass_stack[i]);
                    cmon_str_view b = cmon_symbols_name(_fr->resolver->symbols, type_sym);
                    if (_fr->resolver->globals_pass_stack[i] == type_sym)
                    {
                        cmon_str_view name = cmon_symbols_name(_fr->resolver->symbols, type_sym);
                        _fr_err(_fr,
                            name_tok,
                                name_tok,
                                name_tok,
                                "invalid recursive type alias '%.*s'",
                                name.end - name.begin,
                                name.begin);
                        return CMON_INVALID_IDX;
                    }
                }

                // this might be another global alias that is not resolved yet, if that's the case,
                // do it.
                if (!cmon_is_valid_idx(cmon_symbols_type(_fr->resolver->symbols, type_sym)))
                {
                    _resolve_alias(_fr, _scope, cmon_symbols_ast(_fr->resolver->symbols, type_sym));
                }
            }

            if ((sk == cmon_symk_type || sk == cmon_symk_alias) &&
                (cmon_symbols_is_pub(_fr->resolver->symbols, type_sym) ||
                 mod_idx == _fr->resolver->mod_idx))
            {
                ret = cmon_symbols_type(_fr->resolver->symbols, type_sym);
                cmon_types_set_used_in_module(_fr->resolver->types, ret, _fr->resolver->mod_idx);
            }
            else
            {
                //@TODO: print what it is?
                _fr_err(_fr,
                    name_tok,
                        name_tok,
                        name_tok,
                        "'%s' is not a type",
                        cmon_symbols_name(_fr->resolver->symbols, type_sym));
                return CMON_INVALID_IDX;
            }
        }
        else
        {
            _fr_err(_fr,
                name_tok,
                    name_tok,
                    name_tok,
                    "'%.*s' is not defined in module %s",
                    name_str_view.end - name_str_view.begin,
                    name_str_view.begin,
                    cmon_modules_name(_fr->resolver->mods, mod_idx));
            return CMON_INVALID_IDX;
        }
    }
    else if (kind == cmon_astk_type_ptr)
    {
        cmon_idx rt = _resolve_parsed_type(_fr, _scope, cmon_ast_type_ptr_type(ast, _ast_idx));
        ret = cmon_is_valid_idx(rt) ? cmon_types_find_ptr(_fr->resolver->types,
                                                          rt,
                                                          cmon_ast_type_ptr_is_mut(ast, _ast_idx),
                                                          _fr->resolver->mod_idx)
                                    : CMON_INVALID_IDX;
    }
    else if (kind == cmon_astk_type_view)
    {
        cmon_idx rt = _resolve_parsed_type(_fr, _scope, cmon_ast_type_view_type(ast, _ast_idx));
        ret = cmon_is_valid_idx(rt) ? cmon_types_find_view(_fr->resolver->types,
                                                           rt,
                                                           cmon_ast_type_view_is_mut(ast, _ast_idx),
                                                           _fr->resolver->mod_idx)
                                    : CMON_INVALID_IDX;
    }
    else if (kind == cmon_astk_type_array)
    {
        cmon_idx rt = _resolve_parsed_type(_fr, _scope, cmon_ast_type_array_type(ast, _ast_idx));
        ret = cmon_is_valid_idx(rt)
                  ? cmon_types_find_array(_fr->resolver->types,
                                          rt,
                                          cmon_ast_type_array_count(ast, _ast_idx),
                                          _fr->resolver->mod_idx)
                  : CMON_INVALID_IDX;
    }
    else if (kind == cmon_astk_type_fn)
    {
        cmon_idx ret_parsed_type, ret_type, idx, idx_buf;

        ret_parsed_type = cmon_ast_type_fn_return_type(ast, _ast_idx);
        if (cmon_is_valid_idx(ret_parsed_type))
        {
            ret_type = _resolve_parsed_type(_fr, _scope, ret_parsed_type);
        }
        else
        {
            ret_type = cmon_types_builtin_void(_fr->resolver->types);
        }

        idx_buf = cmon_idx_buf_mng_get(_fr->idx_buf_mng);

        // cmon_ast_iter param_it;
        // param_it = cmon_ast_type_fn_params_iter(ast, _ast_idx);
        // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(ast, &param_it)))
        size_t i;
        for (i = 0; i < cmon_ast_type_fn_params_count(ast, _ast_idx); ++i)
        {
            //@TODO: Check if resolve_parsed_type returns a valid idx and early out if not?
            cmon_idx_buf_append(
                _fr->idx_buf_mng,
                idx_buf,
                _resolve_parsed_type(_fr, _scope, cmon_ast_type_fn_param(ast, _ast_idx, i)));
        }

        ret = cmon_types_find_fn(_fr->resolver->types,
                                 ret_type,
                                 cmon_idx_buf_ptr(_fr->idx_buf_mng, idx_buf),
                                 cmon_idx_buf_count(_fr->idx_buf_mng, idx_buf),
                                 _fr->resolver->mod_idx);
        cmon_idx_buf_mng_return(_fr->idx_buf_mng, idx_buf);
    }

    // cmon_typek rk = cmon_types_kind(_fr->resolver->types, ret);
    // if (rk == cmon_typek_ptr || rk == cmon_typek_array || rk == cmon_typek_view ||
    //     rk == cmon_typek_fn)
    // {
    //     _add_implicit_type(_fr, ret);
    // }

    _fr->resolved_types[_ast_idx] = ret;

    return ret;
}

static inline cmon_idx _resolve_ident(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_str_view name;
    cmon_idx sym;
    cmon_symk skind;

    name = cmon_ast_ident_name(_fr_ast(_fr), _ast_idx);
    sym = cmon_symbols_find(_fr->resolver->symbols, _scope, name);
    if (cmon_is_valid_idx(sym))
    {
        cmon_ast_ident_set_sym(_fr_ast(_fr), _ast_idx, sym);
        skind = cmon_symbols_kind(_fr->resolver->symbols, sym);
        if (skind == cmon_symk_type || skind == cmon_symk_alias)
        {
            return cmon_types_builtin_typeident(_fr->resolver->types);
        }
        else if (skind == cmon_symk_import)
        {
            return cmon_types_builtin_modident(_fr->resolver->types);
        }
        else if (skind == cmon_symk_var)
        {
            cmon_idx ret = cmon_symbols_var_type(_fr->resolver->symbols, sym);
            if (!cmon_is_valid_idx(ret) && _fr->resolver->global_type_pass)
            {
                size_t i;
                for (i = 0; i < cmon_dyn_arr_count(&_fr->resolver->globals_pass_stack); ++i)
                {
                    if (sym == _fr->resolver->globals_pass_stack[i])
                    {
                        _fr_err(_fr,
                                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                                cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                                "typechecking loop");
                        return CMON_INVALID_IDX;
                    }
                }

                _resolve_var_decl(
                    _fr, _fr->file_scope, cmon_symbols_ast(_fr->resolver->symbols, sym));
                ret = cmon_symbols_var_type(_fr->resolver->symbols, sym);
            }

            assert(cmon_is_valid_idx(ret));
            return ret;
        }
    }

    _fr_err(_fr,
            cmon_ast_token(_fr_ast(_fr), _ast_idx),
            cmon_ast_token(_fr_ast(_fr), _ast_idx),
            cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
            "undeclared identifier %.*s",
            name.end - name.begin,
            name.begin);

    return CMON_INVALID_IDX;
}

static inline cmon_idx _resolve_int_literal(_file_resolver * _fr,
                                            cmon_idx _scope,
                                            cmon_idx _ast_idx,
                                            cmon_idx _lh_type)
{
    cmon_idx ret, tok;
    uintmax_t v;
    char * endptr;
    cmon_typek kind;

    tok = cmon_ast_token(_fr_ast(_fr), _ast_idx);
    ret = cmon_types_builtin_s32(_fr->resolver->types);
    if (cmon_is_valid_idx(_lh_type))
    {
        kind = cmon_types_kind(_fr->resolver->types, _lh_type);
        if (kind == cmon_typek_u8 || kind == cmon_typek_s8 || kind == cmon_typek_u16 ||
            kind == cmon_typek_s16 || kind == cmon_typek_u32 || kind == cmon_typek_s32 ||
            kind == cmon_typek_u64 || kind == cmon_typek_s64)
        {
            ret = _lh_type;
        }
    }

    errno = 0;
    // if (cmon_types_is_unsigned_int(_fr->resolver->types, ret))
    // {
    //     if (_is_negated)
    //     {
    //         _fr_err(_fr,
    //                 tok,
    //                 "negated int literal overflows %s",
    //                 cmon_types_name(_fr->resolver->types, ret));
    //         return ret;
    //     }
    // }

    v = strtoumax(cmon_tokens_str_view(_fr_tokens(_fr), tok).begin, &endptr, 0);
    if (errno == ERANGE)
    {
        _fr_err(_fr,
                tok,
                tok,
                tok,
                "int literal out of range for '%s'",
                cmon_types_name(_fr->resolver->types, ret));
    }
    else
    {
        // _expr->data.int_lit.value = v;
        kind = cmon_types_kind(_fr->resolver->types, ret);

        //@TODO: Better (more portable) way to get the max/min values for each of these??
        // if (_is_negated)
        // {
        //     if ((kind == cmon_typek_s8 && -(int64_t)v < SCHAR_MIN) ||
        //         (kind == cmon_typek_s16 && -(int64_t)v < SHRT_MIN) ||
        //         (kind == cmon_typek_s32 && -(int64_t)v < INT_MIN) ||
        //         (kind == cmon_typek_s64 && v > -(uint64_t)LONG_MIN))
        //     {
        //         _fr_err(_fr,
        //                 tok,
        //                 "int literal out of range for '%s'",
        //                 cmon_types_name(_fr->resolver->types, ret));
        //     }
        // }
        // else
        {
            if ((kind == cmon_typek_u8 && v > UCHAR_MAX) ||
                (kind == cmon_typek_s8 && v > SCHAR_MAX) ||
                (kind == cmon_typek_u16 && v > USHRT_MAX) ||
                (kind == cmon_typek_s16 && v > SHRT_MAX) ||
                (kind == cmon_typek_u32 && v > UINT_MAX) ||
                (kind == cmon_typek_s32 && v > INT_MAX) ||
                (kind == cmon_typek_u64 && v > ULONG_MAX) ||
                (kind == cmon_typek_s64 && v > LONG_MAX))
            {
                _fr_err(_fr,
                        tok,
                        tok,
                        tok,
                        "int literal out of range for '%s'",
                        cmon_types_name(_fr->resolver->types, ret));
            }
        }
    }
    return ret;
}

static inline cmon_idx _resolve_float_literal(_file_resolver * _fr,
                                              cmon_idx _scope,
                                              cmon_idx _ast_idx,
                                              cmon_idx _lh_type)
{
    cmon_idx ret;
    char * endptr;
    double v;

    if (cmon_is_valid_idx(_lh_type) &&
        (cmon_types_kind(_fr->resolver->types, _lh_type) == cmon_typek_f32 ||
         cmon_types_kind(_fr->resolver->types, _lh_type) == cmon_typek_f64))
    {
        ret = _lh_type;
    }
    else
    {
        ret = cmon_types_builtin_f64(_fr->resolver->types);
    }

    v = strtod(cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_token(_fr_ast(_fr), _ast_idx)).begin,
               &endptr);
    // _expr->data.float_lit.value = v;

    // NOTE: We only check for f32 because if the literal is out of range for f64, strtod must have
    // failed already.
    if ((/*v == HUGE_VAL && */ errno == ERANGE) ||
        (cmon_types_kind(_fr->resolver->types, ret) == cmon_typek_f32 &&
         (v > FLT_MAX || v < FLT_MIN)))
    {
        //@TODO: print the literal too?
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                "float literal out of range for '%s'",
                cmon_types_name(_fr->resolver->types, ret));
    }
    return ret;
}

static inline cmon_idx _resolve_addr(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx expr, type;

    expr = cmon_ast_addr_expr(_fr_ast(_fr), _ast_idx);

    //@TODO: check that the expression is not a literal/temporary value (i.e. &3.5 should not
    // compile)
    type = _resolve_expr(_fr, _scope, expr, CMON_INVALID_IDX);
    if (cmon_is_valid_idx(type))
    {
        // if (cmon_types_kind(_fr->resolver->types, type) == cmon_typek_fn)
        // {
        //     _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), _ast_idx), "can't take address of
        //     function"); return CMON_INVALID_IDX;
        // }
        // else if (_is_literal(_fr, _remove_paran(_fr, expr)))
        // {
        //     _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), _ast_idx), "can't take address of
        //     literal"); return CMON_INVALID_IDX;
        // }
        cmon_bool is_mut;
        if (!_validate_lvalue_expr(_fr, expr, type, cmon_false, &is_mut))
        {
            return cmon_types_find_ptr(_fr->resolver->types, type, is_mut, _fr->resolver->mod_idx);
        }
    }
    return CMON_INVALID_IDX;
}

static inline cmon_idx _resolve_deref(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx type, expr;
    expr = cmon_ast_deref_expr(_fr_ast(_fr), _ast_idx);
    type = _resolve_expr(_fr, _scope, expr, CMON_INVALID_IDX);
    if (cmon_is_valid_idx(type))
    {
        if (cmon_types_kind(_fr->resolver->types, type) != cmon_typek_ptr)
        {
            _fr_err(_fr,
                    cmon_ast_token(_fr_ast(_fr), expr),
                    cmon_ast_token(_fr_ast(_fr), expr),
                    cmon_ast_token_last(_fr_ast(_fr), expr),
                    "can't dereference non-pointer type");
            return CMON_INVALID_IDX;
        }
        return cmon_types_ptr_type(_fr->resolver->types, type);
    }
    return CMON_INVALID_IDX;
}

static inline cmon_idx _resolve_prefix(_file_resolver * _fr,
                                       cmon_idx _scope,
                                       cmon_idx _ast_idx,
                                       cmon_idx _lh_type)
{
    // cmon_typek lh_kind;
    // cmon_idx op_tok;

    //@TODO: Add overflow error if an unsigned int constant expression is negated
    // if (cmon_is_valid_idx(_lh_type))
    // {
    //     if(cmon_types_is_unsigned_int(_fr->resolver->types, _lh_type))
    //     {
    //         lh_kind = cmon_types_kind(_fr->resolver->types, _lh_type);

    //     }
    // }
    // cmon_idx lht = _lh_type;
    // if(!cmon_is_valid_idx(_lh_type) && cmon_tokens_kind(_fr_tokens(_fr),
    // cmon_ast_prefix_op_tok()))
    // {
    //     lht = cmon_types_builtin_s32()
    // }
    cmon_idx ret =
        _resolve_expr(_fr, _scope, cmon_ast_prefix_expr(_fr_ast(_fr), _ast_idx), _lh_type);

    if (cmon_is_valid_idx(ret) &&
        *cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_prefix_op_tok(_fr_ast(_fr), _ast_idx))
                .begin == '-' &&
        !cmon_types_is_numeric(_fr->resolver->types, ret))
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                "prefix minus with non-numeric type '%s'",
                cmon_types_name(_fr->resolver->types, ret));
    }
    return ret;
}

static inline cmon_idx _is_arithmetic(_file_resolver * _fr, cmon_idx _ast_idx)
{
    assert(cmon_ast_kind(_fr_ast(_fr), _ast_idx) == cmon_astk_binary);
    cmon_idx op_tok = cmon_ast_binary_op_tok(_fr_ast(_fr), _ast_idx);

    return cmon_tokens_is(_fr_tokens(_fr),
                          op_tok,
                          cmon_tokk_plus_assign,
                          cmon_tokk_minus_assign,
                          cmon_tokk_mult_assign,
                          cmon_tokk_div_assign,
                          cmon_tokk_mod_assign,
                          cmon_tokk_plus,
                          cmon_tokk_minus,
                          cmon_tokk_mult,
                          cmon_tokk_div,
                          cmon_tokk_mod);
}

static inline void _invalid_operands_to_binary_err(_file_resolver * _fr,
                                                   cmon_idx _ast_idx,
                                                   cmon_idx _type_left,
                                                   cmon_idx _type_right)
{
    cmon_str_view op_tok_sv =
        cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_binary_op_tok(_fr_ast(_fr), _ast_idx));
    _fr_err(_fr,
            cmon_ast_token_first(_fr_ast(_fr), _ast_idx),
            cmon_ast_token(_fr_ast(_fr), _ast_idx),
            cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
            "invalid operands to binary %.*s ('%s' and '%s')",
            op_tok_sv.end - op_tok_sv.begin,
            op_tok_sv.begin,
            cmon_types_name(_fr->resolver->types, _type_left),
            cmon_types_name(_fr->resolver->types, _type_right));
}

static inline cmon_idx _resolve_binary(_file_resolver * _fr,
                                       cmon_idx _scope,
                                       cmon_idx _ast_idx,
                                       cmon_idx _lh_type)
{
    cmon_idx left_expr, right_expr;
    cmon_idx left_type, right_type;

    left_expr = cmon_ast_binary_left(_fr_ast(_fr), _ast_idx);
    right_expr = cmon_ast_binary_right(_fr_ast(_fr), _ast_idx);

    left_type = _resolve_expr(_fr, _scope, left_expr, _lh_type);
    right_type = _resolve_expr(_fr, _scope, right_expr, left_type);

    if (!cmon_is_valid_idx(left_type) || !cmon_is_valid_idx(right_type))
        return CMON_INVALID_IDX;

    if (cmon_ast_binary_is_assignment(_fr_ast(_fr), _ast_idx))
    {
        _validate_lvalue_expr(_fr, left_expr, left_type, cmon_true, NULL);
    }

    // if (_is_arithmetic(_fr, _ast_idx))
    // {
    cmon_typek ltk = cmon_types_kind(_fr->resolver->types, left_type);
    cmon_idx op_tok = cmon_ast_binary_op_tok(_fr_ast(_fr), _ast_idx);
    // ptr arithmetic
    if (ltk == cmon_typek_ptr)
    {
        if (cmon_tokens_is(_fr_tokens(_fr),
                           op_tok,
                           cmon_tokk_mult,
                           cmon_tokk_div,
                           cmon_tokk_mod,
                           cmon_tokk_mult_assign,
                           cmon_tokk_div_assign,
                           cmon_tokk_mod_assign))
        {
            _invalid_operands_to_binary_err(_fr, _ast_idx, left_type, right_type);
            return CMON_INVALID_IDX;
        }
        else if (!cmon_types_is_int(_fr->resolver->types, right_type))
        {
            _invalid_operands_to_binary_err(_fr, _ast_idx, left_type, right_type);
            return CMON_INVALID_IDX;
        }

        return left_type;
    }
    // // modulus and bitwise operator case
    // else if (cmon_tokens_is(_fr_tokens(_fr),
    //                         op_tok,
    //                         cmon_tokk_mod,
    //                         cmon_tokk_mod_assign,
    //                         cmon_tokk_bw_left_assign,
    //                         cmon_tokk_bw_right_assign,
    //                         cmon_tokk_bw_and_assign,
    //                         cmon_tokk_bw_xor_assign,
    //                         cmon_tokk_bw_or_assign,
    //                         cmon_tokk_bw_left,
    //                         cmon_tokk_bw_right,
    //                         cmon_tokk_bw_and,
    //                         cmon_tokk_bw_xor,
    //                         cmon_tokk_bw_or))
    // {
    //     if (!cmon_types_is_int(_fr->resolver->types, left_type) ||
    //         !cmon_types_is_int(_fr->resolver->types, right_type))
    //     {
    //         _invalid_operands_to_binary_err(_fr, _ast_idx, left_type, right_type);
    //         return CMON_INVALID_IDX;
    //     }

    //     return left_type;
    // }
    // }

    if (left_type != right_type)
    {
        _invalid_operands_to_binary_err(_fr, _ast_idx, left_type, right_type);
        return CMON_INVALID_IDX;
    }

    return left_type;
}

static inline cmon_idx _resolve_selector(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx left_expr, tlhs, name_tok, import_sym, mod, selected_sym;
    cmon_typek tkind;
    cmon_str_view name_str_view;

    left_expr = _remove_paran(_fr, cmon_ast_selector_left(_fr_ast(_fr), _ast_idx));
    tlhs = _resolve_expr(_fr, _scope, left_expr, CMON_INVALID_IDX);

    name_tok = cmon_ast_selector_name_tok(_fr_ast(_fr), _ast_idx);
    name_str_view = cmon_tokens_str_view(_fr_tokens(_fr), name_tok);

    if (cmon_is_valid_idx(tlhs))
    {
        tkind = cmon_types_kind(_fr->resolver->types, tlhs);
        if (tkind == cmon_typek_modident)
        {
            assert(cmon_ast_kind(_fr_ast(_fr), left_expr) == cmon_astk_ident);
            import_sym = cmon_ast_ident_sym(_fr_ast(_fr), left_expr);
            assert(cmon_is_valid_idx(import_sym));
            mod = cmon_symbols_import_module(_fr->resolver->symbols, import_sym);
            selected_sym = cmon_symbols_find(_fr->resolver->symbols,
                                             cmon_modules_global_scope(_fr->resolver->mods, mod),
                                             name_str_view);
            if (cmon_is_valid_idx(selected_sym))
            {
                // we are only interested in global variables here, types from other modules are
                // resolved in resolve_parsed_type
                cmon_symk skind;

                skind = cmon_symbols_kind(_fr->resolver->symbols, selected_sym);
                if (skind == cmon_symk_var)
                {
                    cmon_dyn_arr_append(&_fr->external_variables, selected_sym);
                    cmon_ast_selector_set_sym(_fr_ast(_fr), _ast_idx, selected_sym);
                    return cmon_symbols_var_type(_fr->resolver->symbols, selected_sym);
                }
            }
            _fr_err(_fr,
                name_tok,
                    name_tok,
                    name_tok,
                    "could not find '%.*s' in module %s",
                    name_str_view.end - name_str_view.begin,
                    name_str_view.begin,
                    cmon_modules_path(_fr->resolver->mods, mod));
            return CMON_INVALID_IDX;
        }
        else if (tkind == cmon_typek_struct)
        {
            cmon_idx sym, type_idx, field_idx;

            // sym = cmon_ast_ident_sym(_fr_ast(_fr), left_expr);
            // assert(cmon_is_valid_idx(sym) &&
            //        cmon_symbols_kind(_fr->resolver->symbols, sym) == cmon_symk_var);
            // type_idx = cmon_symbols_var_type(_fr->resolver->symbols, sym);
            field_idx = cmon_types_struct_findv_field(_fr->resolver->types, tlhs, name_str_view);
            if (cmon_is_valid_idx(field_idx))
            {
                return cmon_types_struct_field_type(_fr->resolver->types, tlhs, field_idx);
            }
            else
            {
                _fr_err(_fr,
                    name_tok,
                        name_tok,
                        name_tok,
                        "struct '%s' has no field '%.*s'",
                        cmon_types_name(_fr->resolver->types, tlhs),
                        name_str_view.end - name_str_view.begin,
                        name_str_view.begin);
                return CMON_INVALID_IDX;
            }
        }
    }

    _fr_err(_fr,
        name_tok,
            name_tok,
            name_tok,
            "selector '%.*s' requested in something not an object or module identifier",
            name_str_view.end - name_str_view.begin,
            name_str_view.begin);

    return CMON_INVALID_IDX;
}

static inline cmon_idx _resolve_call(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx left_ast = cmon_ast_call_left(_fr_ast(_fr), _ast_idx);
    cmon_idx left_type = _resolve_expr(_fr, _scope, left_ast, CMON_INVALID_IDX);
    if (!cmon_is_valid_idx(left_type))
    {
        return CMON_INVALID_IDX;
    }

    if (cmon_types_kind(_fr->resolver->types, left_type) != cmon_typek_fn)
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), left_ast),
                cmon_ast_token(_fr_ast(_fr), left_ast),
                cmon_ast_token_last(_fr_ast(_fr), left_ast),
                "not callable");
        return CMON_INVALID_IDX;
    }

    size_t arg_count = cmon_ast_call_args_count(_fr_ast(_fr), _ast_idx);
    size_t param_count = cmon_types_fn_param_count(_fr->resolver->types, left_type);

    if (arg_count != param_count)
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                "argument count mismatch (%lu expected, got %lu)",
                param_count,
                arg_count);
        return CMON_INVALID_IDX;
    }

    // cmon_idx idx;
    // cmon_idx param_idx = 0;
    // cmon_ast_iter it = cmon_ast_call_args_iter(_fr_ast(_fr), _ast_idx);
    // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
    size_t i;
    for (i = 0; i < cmon_ast_call_args_count(_fr_ast(_fr), _ast_idx); ++i)
    {
        cmon_idx idx = cmon_ast_call_arg(_fr_ast(_fr), _ast_idx, i);
        cmon_idx param_type = cmon_types_fn_param(_fr->resolver->types, left_type, i);
        cmon_idx arg_type = _resolve_expr(_fr, _scope, idx, param_type);
        _validate_conversion(_fr,
                             cmon_ast_token(_fr_ast(_fr), idx),
                             cmon_ast_token_last(_fr_ast(_fr), idx),
                             arg_type,
                             param_type);
    }

    return cmon_types_fn_return_type(_fr->resolver->types, left_type);
}

static inline cmon_idx _resolve_index(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx lexpr = cmon_ast_index_left(_fr_ast(_fr), _ast_idx);
    cmon_idx idx_expr = cmon_ast_index_expr(_fr_ast(_fr), _ast_idx);
    cmon_idx left_type = _resolve_expr(_fr, _scope, lexpr, CMON_INVALID_IDX);

    // assert(0);
    if (!cmon_is_valid_idx(left_type))
        return CMON_INVALID_IDX;

    if (!_is_indexable(_fr, left_type))
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), lexpr),
                cmon_ast_token(_fr_ast(_fr), lexpr),
                cmon_ast_token_last(_fr_ast(_fr), lexpr),
                "'%s' type is not indexable",
                cmon_types_name(_fr->resolver->types, left_type));
        return CMON_INVALID_IDX;
    }

    cmon_idx idx_expr_type = _resolve_expr(_fr, _scope, idx_expr, CMON_INVALID_IDX);

    if (!cmon_is_valid_idx(idx_expr_type))
        return CMON_INVALID_IDX;

    //@TODO: Evaluate constant expressions and make sure they are valid indices
    if (!cmon_types_is_int(_fr->resolver->types, idx_expr_type))
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), idx_expr),
                cmon_ast_token(_fr_ast(_fr), idx_expr),
                cmon_ast_token_last(_fr_ast(_fr), idx_expr),
                "non-integer index");
        return CMON_INVALID_IDX;
    }

    cmon_typek lkind = cmon_types_kind(_fr->resolver->types, left_type);
    if (lkind == cmon_typek_array)
    {
        return cmon_types_array_type(_fr->resolver->types, left_type);
    }
    //@TODO: Handle views/tuples once we get there. die for now
    assert(0);
    return CMON_INVALID_IDX;
}

static inline cmon_idx _resolve_array_init(_file_resolver * _fr,
                                           cmon_idx _scope,
                                           cmon_idx _ast_idx,
                                           cmon_idx _lh_type)
{
    // early out if its an empty init, i.e. []
    if (!cmon_ast_array_init_exprs_count(_fr_ast(_fr), _ast_idx))
        return CMON_INVALID_IDX;

    cmon_idx type_suggestion = CMON_INVALID_IDX;
    if (cmon_is_valid_idx(_lh_type) &&
        cmon_types_kind(_fr->resolver->types, _lh_type) == cmon_typek_array)
    {
        type_suggestion = cmon_types_array_type(_fr->resolver->types, _lh_type);
    }

    // cmon_ast_iter it = cmon_ast_array_init_exprs_iter(_fr_ast(_fr), _ast_idx);
    // cmon_idx idx = cmon_ast_iter_next(_fr_ast(_fr), &it);

    size_t expr_count = cmon_ast_array_init_exprs_count(_fr_ast(_fr), _ast_idx);
    size_t i = 0;
    cmon_idx type = _resolve_expr(
        _fr, _scope, cmon_ast_array_init_expr(_fr_ast(_fr), _ast_idx, i++), type_suggestion);

    if (!cmon_is_valid_idx(type))
        return CMON_INVALID_IDX;

    // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
    for (; i < expr_count; ++i)
    {
        cmon_idx idx = cmon_ast_array_init_expr(_fr_ast(_fr), _ast_idx, i);
        cmon_idx rti = _resolve_expr(_fr, _scope, idx, type);
        if (!cmon_is_valid_idx(rti))
            continue;

        _validate_conversion(_fr,
                             cmon_ast_token(_fr_ast(_fr), idx),
                             cmon_ast_token_last(_fr_ast(_fr), idx),
                             rti,
                             type);
    }

    return cmon_types_find_array(_fr->resolver->types, type, expr_count, _fr->resolver->mod_idx);
}

static inline cmon_idx _resolve_struct_init(_file_resolver * _fr,
                                            cmon_idx _scope,
                                            cmon_idx _ast_idx)
{
    cmon_idx type =
        _resolve_parsed_type(_fr, _scope, cmon_ast_struct_init_parsed_type(_fr_ast(_fr), _ast_idx));

    if (!cmon_is_valid_idx(type))
        return CMON_INVALID_IDX;

    if (_fr->resolver->global_type_pass)
    {
        return type;
    }

    if (cmon_types_kind(_fr->resolver->types, type) != cmon_typek_struct)
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token(_fr_ast(_fr), _ast_idx),
                cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                "'%s' is not a struct type",
                cmon_types_name(_fr->resolver->types, type));
        return CMON_INVALID_IDX;
    }
    cmon_idx field_initialized_buf = cmon_idx_buf_mng_get(_fr->idx_buf_mng);
    cmon_idx field_count = cmon_types_struct_field_count(_fr->resolver->types, type);
    size_t i;
    for (i = 0; i < field_count; ++i)
    {
        cmon_idx_buf_append(_fr->idx_buf_mng, field_initialized_buf, CMON_INVALID_IDX);
    }

    cmon_bool expect_field_names;
    for (size_t i = 0; i < cmon_ast_struct_init_fields_count(_fr_ast(_fr), _ast_idx); ++i)
    {
        cmon_idx idx = cmon_ast_struct_init_field(_fr_ast(_fr), _ast_idx, i);
        cmon_idx fname_tok = cmon_ast_struct_init_field_name_tok(_fr_ast(_fr), idx);
        cmon_idx expr = cmon_ast_struct_init_field_expr(_fr_ast(_fr), idx);
        if (i == 0)
        {
            expect_field_names = cmon_is_valid_idx(fname_tok);
        }

        if (i >= field_count)
        {
            _fr_err(_fr,
                    cmon_ast_token(_fr_ast(_fr), _ast_idx),
                    cmon_ast_token(_fr_ast(_fr), _ast_idx),
                    cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                    "too many expressions in '%s' literal",
                    cmon_types_name(_fr->resolver->types, type));
            break;
        }

        if ((expect_field_names && !cmon_is_valid_idx(fname_tok)) ||
            (!expect_field_names && cmon_is_valid_idx(fname_tok)))
        {
            _fr_err(_fr,
                    cmon_ast_token(_fr_ast(_fr), _ast_idx),
                    cmon_ast_token(_fr_ast(_fr), _ast_idx),
                    cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                    "mixture of field: value and value initializers");
            continue;
        }

        cmon_idx field_idx;
        if (cmon_is_valid_idx(fname_tok))
        {
            cmon_str_view name_str_view = cmon_tokens_str_view(_fr_tokens(_fr), fname_tok);
            field_idx = cmon_types_struct_findv_field(_fr->resolver->types, type, name_str_view);
            if (!cmon_is_valid_idx(field_idx))
            {
                _fr_err(_fr,
                    fname_tok,
                        fname_tok,
                        fname_tok,
                        "no field '%.*s' in '%s'",
                        name_str_view.end - name_str_view.begin,
                        name_str_view.begin,
                        cmon_types_name(_fr->resolver->types, type));
                continue;
            }
        }
        else
        {
            field_idx = i;
        }

        cmon_idx field_type = cmon_types_struct_field_type(_fr->resolver->types, type, field_idx);
        assert(cmon_is_valid_idx(field_type));
        cmon_idx expr_type = _resolve_expr(_fr, _scope, expr, field_type);
        _validate_conversion(_fr,
                             cmon_ast_token(_fr_ast(_fr), expr),
                             cmon_ast_token_last(_fr_ast(_fr), expr),
                             expr_type,
                             field_type);

        cmon_idx_buf_set(_fr->idx_buf_mng, field_initialized_buf, field_idx, expr);
    }

    for (i = 0; i < cmon_idx_buf_count(_fr->idx_buf_mng, field_initialized_buf); ++i)
    {
        if (!cmon_is_valid_idx(cmon_idx_buf_at(_fr->idx_buf_mng, field_initialized_buf, i)))
        {
            cmon_idx def_expr = cmon_types_struct_field_def_expr(_fr->resolver->types, type, i);
            if (!cmon_is_valid_idx(def_expr))
            {
                _fr_err(_fr,
                        cmon_ast_token(_fr_ast(_fr), _ast_idx),
                        cmon_ast_token(_fr_ast(_fr), _ast_idx),
                        cmon_ast_token_last(_fr_ast(_fr), _ast_idx),
                        "field '%s' is not initialized",
                        cmon_types_struct_field_name(_fr->resolver->types, type, i));
            }
            else
            {
                cmon_idx_buf_set(_fr->idx_buf_mng, field_initialized_buf, i, def_expr);
            }
        }
    }

    // instead of returning the field idx buffer, we cache it for IR generation
    cmon_ast_struct_init_set_resolved_field_idx_buf(_fr_ast(_fr), _ast_idx, field_initialized_buf);
    // cmon_idx_buf_mng_return(_fr->idx_buf_mng, field_initialized_buf);

    return type;
}

static inline cmon_idx _resolve_fn_sig(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx fn_parsed_ret = cmon_ast_fn_ret_type(_fr_ast(_fr), _ast_idx);
    cmon_idx ret_type;

    if (cmon_is_valid_idx(fn_parsed_ret))
    {
        ret_type = _resolve_parsed_type(_fr, _scope, fn_parsed_ret);
    }
    else
    {
        ret_type = cmon_types_builtin_void(_fr->resolver->types);
    }

    assert(cmon_is_valid_idx(ret_type));

    cmon_idx idx_buf = cmon_idx_buf_mng_get(_fr->idx_buf_mng);

    cmon_bool param_err = cmon_false;
    for (size_t i = 0; i < cmon_ast_fn_params_count(_fr_ast(_fr), _ast_idx); ++i)
    {
        cmon_idx idx = cmon_ast_fn_param(_fr_ast(_fr), _ast_idx, i);
        cmon_astk kind = cmon_ast_kind(_fr_ast(_fr), idx);
        if (kind == cmon_astk_var_decl)
        {
            cmon_idx pt =
                _resolve_parsed_type(_fr, _scope, cmon_ast_var_decl_type(_fr_ast(_fr), idx));
            _fr->resolved_types[idx] = pt;
            if (!cmon_is_valid_idx(pt))
                param_err = cmon_true;
            cmon_idx_buf_append(_fr->idx_buf_mng, idx_buf, pt);
        }
        else
        {
            _unexpected_ast_panic();
        }
    }

    cmon_idx ret;
    if (param_err)
    {
        ret = CMON_INVALID_IDX;
    }
    else
    {
        ret = cmon_types_find_fn(_fr->resolver->types,
                                 ret_type,
                                 cmon_idx_buf_ptr(_fr->idx_buf_mng, idx_buf),
                                 cmon_idx_buf_count(_fr->idx_buf_mng, idx_buf),
                                 _fr->resolver->mod_idx);
    }
    cmon_idx_buf_mng_return(_fr->idx_buf_mng, idx_buf);
    _fr->resolved_types[_ast_idx] = ret;
    return ret;
}

static inline cmon_bool _add_fn_param_sym(
    _file_resolver * _fr, cmon_idx _scope, cmon_idx _name_tok, cmon_bool _is_mut, cmon_idx _ast_idx)
{
    cmon_idx sym;
    cmon_str_view str_view = cmon_tokens_str_view(_fr_tokens(_fr), _name_tok);
    if (cmon_is_valid_idx(sym = cmon_symbols_find_local(_fr->resolver->symbols, _scope, str_view)))
    {
        _fr_err(_fr,
            _name_tok,
                _name_tok,
                _name_tok,
                "redeclaration of parameter '%.*s'",
                str_view.end - str_view.begin,
                str_view.begin);
        return cmon_true;
    }
    else
    {
        //@NOTE: The type of the parameter must have been resolved in _resolve_fn_sig
        assert(cmon_is_valid_idx(_fr->resolved_types[_ast_idx]));
        cmon_idx sym = cmon_symbols_scope_add_var(_fr->resolver->symbols,
                                                  _scope,
                                                  str_view,
                                                  _fr->resolved_types[_ast_idx],
                                                  cmon_false,
                                                  _is_mut,
                                                  _fr->src_file_idx,
                                                  _ast_idx);
        cmon_ast_var_decl_set_sym(_fr_ast(_fr), _ast_idx, sym);
    }
    return cmon_false;
}

static inline void _resolve_fn_body(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    //@NOTE: Functions only see file scope variables
    cmon_idx scope =
        cmon_symbols_scope_begin(_fr->resolver->symbols, _fr->file_scope, _fr->resolver->mod_idx);
    // cmon_ast_iter param_it = cmon_ast_fn_params_iter(_fr_ast(_fr), _ast_idx);
    // cmon_idx idx;
    // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &param_it)))
    size_t i;
    for (i = 0; i < cmon_ast_fn_params_count(_fr_ast(_fr), _ast_idx); ++i)
    {
        cmon_idx idx = cmon_ast_fn_param(_fr_ast(_fr), _ast_idx, i);
        cmon_astk kind = cmon_ast_kind(_fr_ast(_fr), idx);
        if (kind == cmon_astk_var_decl)
        {
            _add_fn_param_sym(_fr,
                              scope,
                              cmon_ast_var_decl_name_tok(_fr_ast(_fr), idx),
                              cmon_ast_var_decl_is_mut(_fr_ast(_fr), idx),
                              idx);
        }
        else
        {
            _unexpected_ast_panic();
        }
    }

    // resolve the function body block
    _resolve_stmt(_fr, scope, cmon_ast_fn_block(_fr_ast(_fr), _ast_idx));
}

static inline cmon_idx _resolve_fn(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx ret = _resolve_fn_sig(_fr, _scope, _ast_idx);
    if (!_fr->resolver->global_type_pass)
    {
        _resolve_fn_body(_fr, _scope, _ast_idx);
    }

    return ret;
}

static inline cmon_idx _resolve_expr(_file_resolver * _fr,
                                     cmon_idx _scope,
                                     cmon_idx _ast_idx,
                                     cmon_idx _lh_type)
{
    cmon_ast * ast;
    cmon_astk kind;
    cmon_idx ret;

    // an expression might have already been resolved during globals pass (to infer a global
    // variables type). In that case we early out.
    //@TODO: Double check if this makes sense?? I have a feeling this should be checked at the level
    // of the global variable instead.
    // if (cmon_is_valid_idx(_fr->resolved_types[_ast_idx]))
    //     return _fr->resolved_types[_ast_idx];

    _ast_idx = _remove_paran(_fr, _ast_idx);
    ast = _fr_ast(_fr);
    kind = cmon_ast_kind(ast, _ast_idx);

    if (kind == cmon_astk_int_literal)
    {
        ret = _resolve_int_literal(_fr, _scope, _ast_idx, _lh_type);
    }
    else if (kind == cmon_astk_float_literal)
    {
        ret = _resolve_float_literal(_fr, _scope, _ast_idx, _lh_type);
    }
    else if (kind == cmon_astk_bool_literal)
    {
        ret = cmon_types_builtin_bool(_fr->resolver->types);
    }
    else if (kind == cmon_astk_string_literal)
    {
        ret = cmon_types_find_view(_fr->resolver->types,
                                   cmon_types_builtin_u8(_fr->resolver->types),
                                   cmon_false,
                                   _fr->resolver->mod_idx);
    }
    else if (kind == cmon_astk_ident)
    {
        ret = _resolve_ident(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_addr)
    {
        ret = _resolve_addr(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_deref)
    {
        ret = _resolve_deref(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_prefix)
    {
        ret = _resolve_prefix(_fr, _scope, _ast_idx, _lh_type);
    }
    else if (kind == cmon_astk_binary)
    {
        ret = _resolve_binary(_fr, _scope, _ast_idx, _lh_type);
    }
    else if (kind == cmon_astk_selector)
    {
        ret = _resolve_selector(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_call)
    {
        ret = _resolve_call(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_index)
    {
        ret = _resolve_index(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_array_init)
    {
        ret = _resolve_array_init(_fr, _scope, _ast_idx, _lh_type);
    }
    else if (kind == cmon_astk_fn_decl)
    {
        ret = _resolve_fn(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_struct_init)
    {
        ret = _resolve_struct_init(_fr, _scope, _ast_idx);
    }
    // else if (kind == cmon_astk_paran_expr)
    // {
    //     return _resolve_expr(_fr, _scope, cmon_ast_paran_expr(_fr_ast(_fr), _ast_idx), _lh_type);
    // }
    else
    {
        ret = CMON_INVALID_IDX;
        assert(0);
    }

    _fr->resolved_types[_ast_idx] = ret;
    return ret;
}

static inline void _resolve_var_decl(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx expr_type_idx, ptype_idx;
    cmon_idx parsed_type_idx = cmon_ast_var_decl_type(_fr_ast(_fr), _ast_idx);

    cmon_bool is_global = cmon_symbols_scope_is_file(_fr->resolver->symbols, _scope);

    cmon_str_view sv =
        cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_token(_fr_ast(_fr), _ast_idx));

    ptype_idx = CMON_INVALID_IDX;
    if (cmon_is_valid_idx(parsed_type_idx))
    {
        if (!cmon_is_valid_idx(_fr->resolved_types[parsed_type_idx]))
        {
            ptype_idx = _resolve_parsed_type(_fr, _scope, parsed_type_idx);
            if (is_global && _fr->resolver->global_type_pass)
            {
                // set the globals symbol type
                cmon_idx sym = cmon_ast_var_decl_sym(_fr_ast(_fr), _ast_idx);
                assert(cmon_is_valid_idx(sym));
                cmon_symbols_var_set_type(_fr->resolver->symbols, sym, ptype_idx);
                return;
            }
        }
        else
        {
            // this should only ever happen for a global variable that is revisited a second time to
            // resolve its expression.
            assert(is_global);
            ptype_idx = _fr->resolved_types[parsed_type_idx];
        }
    }

    if (is_global && _fr->resolver->global_type_pass)
    {
        cmon_idx sym = cmon_ast_var_decl_sym(_fr_ast(_fr), _ast_idx);
        cmon_dyn_arr_append(&_fr->resolver->globals_pass_stack, sym);
    }

    cmon_idx expr_idx = cmon_ast_var_decl_expr(_fr_ast(_fr), _ast_idx);
    assert(cmon_is_valid_idx(expr_idx));
    expr_type_idx = _resolve_expr(_fr, _scope, expr_idx, ptype_idx);

    if (is_global && _fr->resolver->global_type_pass)
    {
        CMON_UNUSED(cmon_dyn_arr_pop(&_fr->resolver->globals_pass_stack));
    }

    if (!cmon_is_valid_idx(expr_type_idx))
        return;

    // if its not a global being resolved, create the symbol.
    cmon_idx sym;
    if (!is_global)
    {
        cmon_idx sym = cmon_symbols_scope_add_var(
            _fr->resolver->symbols,
            _scope,
            cmon_tokens_str_view(_fr_tokens(_fr),
                                 cmon_ast_var_decl_name_tok(_fr_ast(_fr), _ast_idx)),
            cmon_is_valid_idx(parsed_type_idx) ? ptype_idx : expr_type_idx,
            cmon_false,
            cmon_ast_var_decl_is_mut(_fr_ast(_fr), _ast_idx),
            _fr->src_file_idx,
            _ast_idx);

        cmon_ast_var_decl_set_sym(_fr_ast(_fr), _ast_idx, sym);
        if (cmon_ast_kind(_fr_ast(_fr), expr_idx) == cmon_astk_fn_decl)
        {
            cmon_dyn_arr_append(&_fr->local_fns, sym);
        }
    }
    // only set the globals symbol type if it was not set above yet
    else if (!cmon_is_valid_idx(parsed_type_idx))
    {
        cmon_idx sym = cmon_ast_var_decl_sym(_fr_ast(_fr), _ast_idx);
        assert(cmon_is_valid_idx(sym));
        assert(cmon_is_valid_idx(expr_type_idx));
        cmon_symbols_var_set_type(_fr->resolver->symbols, sym, expr_type_idx);
    }

    if (cmon_is_valid_idx(parsed_type_idx))
    {
        _validate_conversion(_fr,
                             cmon_ast_token(_fr_ast(_fr), expr_idx),
                             cmon_ast_token_last(_fr_ast(_fr), expr_idx),
                             expr_type_idx,
                             ptype_idx);
    }
}

static inline void _resolve_alias(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_bool is_global = cmon_symbols_scope_is_file(_fr->resolver->symbols, _scope);

    if (is_global)
    {
        cmon_idx sym = cmon_ast_alias_sym(_fr_ast(_fr), _ast_idx);
        assert(cmon_is_valid_idx(sym));
        cmon_dyn_arr_append(&_fr->resolver->globals_pass_stack, sym);
    }

    cmon_idx type =
        _resolve_parsed_type(_fr, _scope, cmon_ast_alias_parsed_type(_fr_ast(_fr), _ast_idx));

    if (is_global)
    {
        CMON_UNUSED(cmon_dyn_arr_pop(&_fr->resolver->globals_pass_stack));
        cmon_idx sym = cmon_ast_alias_sym(_fr_ast(_fr), _ast_idx);
        cmon_symbols_alias_set_type(_fr->resolver->symbols, sym, type);
    }
    else
    {
        cmon_idx name_tok = cmon_ast_alias_name_tok(_fr_ast(_fr), _ast_idx);
        assert(!cmon_ast_alias_is_pub(_fr_ast(_fr), _ast_idx));
        cmon_symbols_scope_add_alias(_fr->resolver->symbols,
                                     _scope,
                                     cmon_tokens_str_view(_fr_tokens(_fr), name_tok),
                                     type,
                                     cmon_ast_alias_is_pub(_fr_ast(_fr), _ast_idx),
                                     _fr->src_file_idx,
                                     _ast_idx);
    }
}

static inline void _resolve_stmt(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_astk kind = cmon_ast_kind(_fr_ast(_fr), _ast_idx);

    if (kind == cmon_astk_block)
    {
        cmon_idx scope =
            cmon_symbols_scope_begin(_fr->resolver->symbols, _scope, _fr->resolver->mod_idx);
        // cmon_ast_iter it = cmon_ast_block_iter(_fr_ast(_fr), _ast_idx);
        // cmon_idx idx;
        // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
        size_t i;
        for (i = 0; i < cmon_ast_block_child_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            _resolve_stmt(_fr, scope, cmon_ast_block_child(_fr_ast(_fr), _ast_idx, i));
        }
    }
    else if (kind == cmon_astk_var_decl)
    {
        _resolve_var_decl(_fr, _scope, _ast_idx);
    }
    else if (kind == cmon_astk_alias)
    {
        _resolve_alias(_fr, _scope, _ast_idx);
    }
    else
    {
        _resolve_expr(_fr, _scope, _ast_idx, CMON_INVALID_IDX);
    }
}

cmon_resolver * cmon_resolver_create(cmon_allocator * _alloc, size_t _max_errors)
{
    cmon_resolver * ret;
    ret = CMON_CREATE(_alloc, cmon_resolver);
    ret->alloc = _alloc;
    ret->symbols = NULL;
    ret->global_scope = CMON_INVALID_IDX;
    ret->types = NULL;
    ret->mods = NULL;
    ret->mod_idx = CMON_INVALID_IDX;
    ret->dep_graph = cmon_dep_graph_create(_alloc);
    ret->idx_buf_mng = cmon_idx_buf_mng_create(_alloc);
    ret->str_builder = cmon_str_builder_create(_alloc, 256);
    ret->max_errors = _max_errors;
    ret->global_type_pass = cmon_false;
    cmon_dyn_arr_init(&ret->file_resolvers, _alloc, 8);
    cmon_dyn_arr_init(&ret->dep_buffer, _alloc, 32);
    cmon_dyn_arr_init(&ret->sorted_types, _alloc, 32);
    cmon_dyn_arr_init(&ret->symbol_ir_map, _alloc, 4);
    // cmon_dyn_arr_init(&ret->global_fns, _alloc, 16);
    // cmon_dyn_arr_init(&ret->global_vars, _alloc, 16);
    // cmon_dyn_arr_init(&ret->local_fns, _alloc, 32);
    cmon_dyn_arr_init(&ret->errs, _alloc, 16);
    ret->err_handler = cmon_err_handler_create(_alloc, NULL, _max_errors);
    cmon_dyn_arr_init(&ret->globals_pass_stack, _alloc, 8);
    ret->ir_builder = NULL;
    ret->main_fn_sym = CMON_INVALID_IDX;
    return ret;
}

void cmon_resolver_destroy(cmon_resolver * _r)
{
    size_t i;

    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];
        cmon_allocator_free(
            _r->alloc,
            (cmon_mem_blk){ fr->resolved_types, sizeof(cmon_idx) * cmon_ast_count(_fr_ast(fr)) });
        cmon_dyn_arr_dealloc(&fr->local_fns);
        cmon_dyn_arr_dealloc(&fr->global_fns);
        cmon_dyn_arr_dealloc(&fr->external_variables);
        cmon_dyn_arr_dealloc(&fr->global_alias_decls);
        cmon_dyn_arr_dealloc(&fr->global_var_decls);
        cmon_dyn_arr_dealloc(&fr->type_decls);
        cmon_idx_buf_mng_destroy(fr->idx_buf_mng);
        cmon_err_handler_destroy(fr->err_handler);
    }
    cmon_irb_destroy(_r->ir_builder);
    cmon_dyn_arr_dealloc(&_r->globals_pass_stack);
    cmon_err_handler_destroy(_r->err_handler);
    cmon_dyn_arr_dealloc(&_r->errs);
    // cmon_dyn_arr_dealloc(&_r->local_fns);
    // cmon_dyn_arr_dealloc(&_r->global_vars);
    // cmon_dyn_arr_dealloc(&_r->global_fns);
    cmon_dyn_arr_dealloc(&_r->symbol_ir_map);
    cmon_dyn_arr_dealloc(&_r->sorted_types);
    cmon_dyn_arr_dealloc(&_r->dep_buffer);
    cmon_dyn_arr_dealloc(&_r->file_resolvers);
    cmon_str_builder_destroy(_r->str_builder);
    cmon_idx_buf_mng_destroy(_r->idx_buf_mng);
    cmon_dep_graph_destroy(_r->dep_graph);
    CMON_DESTROY(_r->alloc, _r);
}

void cmon_resolver_set_input(cmon_resolver * _r,
                             cmon_src * _src,
                             cmon_types * _types,
                             cmon_symbols * _symbols,
                             cmon_modules * _mods,
                             cmon_idx _mod_idx)
{
    size_t i;
    assert(!cmon_is_valid_idx(_r->global_scope));
    _r->src = _src;
    cmon_err_handler_set_src(_r->err_handler, _src);
    _r->types = _types;
    _r->symbols = _symbols;
    _r->mods = _mods;
    _r->mod_idx = _mod_idx;
    _r->global_scope = cmon_symbols_scope_begin(_r->symbols, CMON_INVALID_IDX, _r->mod_idx);
    cmon_modules_set_global_scope(_r->mods, _r->mod_idx, _r->global_scope);
    cmon_modules_set_resolver(_r->mods, _r->mod_idx, _r);

    // add all builtins to the global scope
    for (i = 0; i < cmon_types_builtin_count(_r->types); ++i)
    {
        cmon_idx idx = cmon_types_builtin(_r->types, i);
        assert(cmon_is_valid_idx(idx));
        cmon_symbols_scope_add_type(_r->symbols,
                                    _r->global_scope,
                                    cmon_str_view_make(cmon_types_name(_r->types, idx)),
                                    idx,
                                    cmon_false,
                                    CMON_INVALID_IDX,
                                    CMON_INVALID_IDX);
    }

    size_t file_count = cmon_modules_src_file_count(_r->mods, _r->mod_idx);
    cmon_dyn_arr_reserve(&_r->file_resolvers, file_count);
    for (i = 0; i < file_count; ++i)
    {
        _file_resolver fr;
        cmon_ast * ast;
        cmon_idx src_file_idx;

        src_file_idx = cmon_modules_src_file(_r->mods, _r->mod_idx, i);
        cmon_is_valid_idx(src_file_idx);
        ast = cmon_src_ast(_r->src, src_file_idx);

        fr.idx_buf_mng = cmon_idx_buf_mng_create(_r->alloc);
        fr.src_file_idx = src_file_idx;
        fr.resolver = _r;
        fr.file_scope = cmon_symbols_scope_begin(_r->symbols, _r->global_scope, _r->mod_idx);
        //@TODO: More educated guesses based on the ast to allocated correcty to begin with?
        cmon_dyn_arr_init(&fr.type_decls, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.global_var_decls, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.global_alias_decls, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.external_variables, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.global_fns, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.local_fns, _r->alloc, 16);
        fr.resolved_types =
            cmon_allocator_alloc(_r->alloc, sizeof(cmon_idx) * cmon_ast_count(ast)).ptr;
        fr.main_fn_sym = CMON_INVALID_IDX;
        memset(fr.resolved_types, (int)CMON_INVALID_IDX, sizeof(cmon_idx) * cmon_ast_count(ast));
        fr.err_handler = cmon_err_handler_create(_r->alloc, _r->src, _r->max_errors);
        cmon_dyn_arr_append(&_r->file_resolvers, fr);
    }
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
        cmon_idx ret = cmon_symbols_scope_add_var(_r->symbols,
                                                  _r->global_scope,
                                                  cmon_tokens_str_view(_fr_tokens(_fr), _name_tok),
                                                  CMON_INVALID_IDX,
                                                  _is_pub,
                                                  _is_mut,
                                                  _fr->src_file_idx,
                                                  _ast_idx);
        cmon_ast_var_decl_set_sym(_fr_ast(_fr), _ast_idx, ret);
        cmon_dyn_arr_append(&_fr->global_var_decls, ret);
        return ret;
    }
    return CMON_INVALID_IDX;
}

#define _set_err_jmp_goto(_fr, _label)                                                             \
    do                                                                                             \
    {                                                                                              \
        if (setjmp(_fr->err_jmp))                                                                  \
        {                                                                                          \
            goto _label;                                                                           \
        }                                                                                          \
        cmon_err_handler_set_jump(_fr->err_handler, &_fr->err_jmp);                                \
    } while (0)

cmon_bool cmon_resolver_top_lvl_pass(cmon_resolver * _r, cmon_idx _file_idx)
{
    cmon_src * src = _r->src;
    cmon_idx src_file_idx = cmon_modules_src_file(_r->mods, _r->mod_idx, _file_idx);
    cmon_ast * ast = cmon_src_ast(src, src_file_idx);
    cmon_tokens * tokens = cmon_src_tokens(src, src_file_idx);
    cmon_bool is_first_stmt = cmon_true;
    _file_resolver * fr = &_r->file_resolvers[_file_idx];

    assert(ast);
    assert(tokens);

    _set_err_jmp_goto(fr, err_end);

    cmon_idx root_block = cmon_ast_root_block(ast);

    // cmon_idx idx;
    // cmon_ast_iter it = cmon_ast_block_iter(ast, root_block);
    // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(ast, &it)))
    size_t i;
    for (i = 0; i < cmon_ast_block_child_count(ast, root_block); ++i)
    {
        cmon_idx idx = cmon_ast_block_child(ast, root_block, i);
        if (is_first_stmt)
        {
            is_first_stmt = cmon_false;
            // make sure every file declares the module its part of at the top
            if (cmon_ast_kind(ast, idx) != cmon_astk_module)
            {
                _fr_err(fr,
                        cmon_ast_token(ast, idx),
                        cmon_ast_token(ast, idx),
                        cmon_ast_token_last(ast, idx),
                        "missing module statement");
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
                            mod_name_tok,
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
                cmon_idx imod_idx, alias_tok_idx;
                // cmon_ast_iter pit = cmon_ast_import_iter(ast, idx);
                // while (cmon_is_valid_idx(ipp_idx = cmon_ast_iter_next(ast, &pit)))
                size_t j;
                for (j = 0; j < cmon_ast_import_pairs_count(ast, idx); ++j)
                {
                    cmon_idx ipp_idx = cmon_ast_import_pair(ast, idx, j);
                    cmon_str_view path = cmon_ast_import_pair_path(ast, ipp_idx);
                    imod_idx = cmon_modules_find(fr->resolver->mods, path);

                    if (!cmon_is_valid_idx(imod_idx))
                    {
                        _fr_err(fr,
                                cmon_ast_import_pair_path_token(ast, ipp_idx, 0),
                                cmon_ast_import_pair_path_token(ast, ipp_idx, 0),
                                cmon_ast_import_pair_path_token(
                                    ast,
                                    ipp_idx,
                                    cmon_ast_import_pair_path_token_count(ast, ipp_idx) - 1),
                                "could not find module '%.*s'",
                                path.end - path.begin,
                                path.begin);
                    }
                    else if (!_check_redec(fr,
                                           fr->file_scope,
                                           alias_tok_idx =
                                               cmon_ast_import_pair_ident(ast, ipp_idx)))
                    {
                        cmon_modules_add_dep(fr->resolver->mods,
                                             fr->resolver->mod_idx,
                                             imod_idx,
                                             fr->src_file_idx,
                                             cmon_ast_import_pair_path_first_tok(ast, ipp_idx));
                        cmon_symbols_scope_add_import(fr->resolver->symbols,
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
                cmon_idx sym = _add_global_var_name(_r,
                                                    fr,
                                                    cmon_ast_var_decl_name_tok(ast, idx),
                                                    cmon_ast_var_decl_is_pub(ast, idx),
                                                    cmon_ast_var_decl_is_mut(ast, idx),
                                                    idx);
                cmon_astk kind =
                    cmon_ast_kind(ast, _remove_paran(fr, cmon_ast_var_decl_expr(ast, idx)));
                if (kind == cmon_astk_fn_decl)
                {
                    cmon_dyn_arr_append(&fr->global_fns, sym);
                    cmon_str_view name =
                        cmon_tokens_str_view(_fr_tokens(fr), cmon_ast_var_decl_name_tok(ast, idx));
                    if (cmon_str_view_c_str_cmp(name, "main") == 0)
                    {
                        fr->main_fn_sym = sym;
                    }
                }
                // else
                // {
                //     cmon_dyn_arr_append(&_r->global_vars, sym);
                // }
            }
            else if (kind == cmon_astk_alias)
            {
                cmon_idx name_tok = cmon_ast_alias_name_tok(ast, idx);
                if (!_check_redec(fr, fr->file_scope, name_tok))
                {
                    cmon_idx sym =
                        cmon_symbols_scope_add_alias(_r->symbols,
                                                     _r->global_scope,
                                                     cmon_tokens_str_view(_fr_tokens(fr), name_tok),
                                                     CMON_INVALID_IDX,
                                                     cmon_ast_alias_is_pub(ast, idx),
                                                     fr->src_file_idx,
                                                     idx);
                    cmon_ast_alias_set_sym(ast, idx, sym);
                    cmon_dyn_arr_append(&fr->global_alias_decls, sym);
                }
            }
            else if (kind == cmon_astk_struct_decl)
            {
                cmon_idx name_tok_idx, tidx;
                cmon_str_view name;
                name_tok_idx = cmon_ast_struct_name(ast, idx);

                if (!_check_redec(fr, fr->file_scope, name_tok_idx))
                {
                    name = cmon_tokens_str_view(tokens, name_tok_idx);
                    tidx = cmon_types_add_struct(
                        _r->types, _r->mod_idx, name, src_file_idx, name_tok_idx);
                    cmon_dyn_arr_append(&fr->type_decls, ((_ast_type_pair){ idx, tidx }));
                    cmon_symbols_scope_add_type(_r->symbols,
                                                _r->global_scope,
                                                name,
                                                tidx,
                                                cmon_ast_struct_is_pub(ast, idx),
                                                fr->src_file_idx,
                                                idx);
                    cmon_ast_struct_set_type(_fr_ast(fr), idx, tidx);
                }
            }
            else
            {
                assert(cmon_false);
                //@TODO: Unexpected top lvl statement? (I guess it would have already failed
                // parsing?) panic?
            }
        }
    }

    return cmon_false;
err_end:
    return cmon_true;
}

cmon_bool cmon_resolver_finalize_top_lvl_names(cmon_resolver * _r)
{
    _r->global_type_pass = cmon_true;
    // first resolve the types of all global aliases and make sure there is no recursion.
    size_t i, j;
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];

        _set_err_jmp_goto(fr, err_end);

        if (cmon_is_valid_idx(fr->main_fn_sym))
        {
            // sanity check, the main fn sym should only ever be set once, error should have been
            // caught way before this.
            assert(!cmon_is_valid_idx(_r->main_fn_sym));
            _r->main_fn_sym = fr->main_fn_sym;
        }

        for (j = 0; j < cmon_dyn_arr_count(&fr->global_alias_decls); ++j)
        {
            cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_alias_decls[j]);
            _resolve_alias(fr, fr->file_scope, ast_idx);
        }
    }
err_end:
    _r->global_type_pass = cmon_false;
    return cmon_resolver_has_errors(_r);
}

static inline cmon_bool _check_field_name(_file_resolver * _fr,
                                          cmon_idx _name_tok_buf,
                                          cmon_idx _name_tok)
{
    //@NOTE: Using index buffers to compare field names via token index because we already have them
    // in place.
    cmon_str_view str_view = cmon_tokens_str_view(_fr_tokens(_fr), _name_tok);
    size_t i;
    for (i = 0; i < cmon_idx_buf_count(_fr->idx_buf_mng, _name_tok_buf); ++i)
    {
        if (cmon_str_view_cmp(
                str_view,
                cmon_tokens_str_view(_fr_tokens(_fr),
                                     cmon_idx_buf_at(_fr->idx_buf_mng, _name_tok_buf, i))) == 0)
        {
            _fr_err(_fr,
                _name_tok,
                    _name_tok,
                    _name_tok,
                    "duplicate field named '%.*s'",
                    str_view.end - str_view.begin,
                    str_view.begin);
            return cmon_true;
        }
    }
    cmon_idx_buf_append(_fr->idx_buf_mng, _name_tok_buf, _name_tok);
    return cmon_false;
}

cmon_bool cmon_resolver_usertypes_pass(cmon_resolver * _r, cmon_idx _file_idx)
{
    size_t i, j;
    _file_resolver * fr = &_r->file_resolvers[_file_idx];
    _set_err_jmp_goto(fr, err_end);

    // first resolve the types of all global aliases and make sure there is no recursion.
    // for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    // {
    //     _file_resolver * fr = &_r->file_resolvers[i];

    //     _set_err_jmp_goto(fr, err_end);

    //     for (j = 0; j < cmon_dyn_arr_count(&fr->global_alias_decls); ++j)
    //     {
    //         cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_alias_decls[j]);
    //         _resolve_alias(fr, fr->file_scope, ast_idx);
    //     }
    // }

    // for (i = 0; i < cmon_dyn_arr_count(&fr->global_alias_decls); ++i)
    // {
    //     cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_alias_decls[i]);
    //     _resolve_alias(fr, fr->file_scope, ast_idx);
    // }

    cmon_idx name_tok_buf = cmon_idx_buf_mng_get(fr->idx_buf_mng);
    for (i = 0; i < cmon_dyn_arr_count(&fr->type_decls); ++i)
    {
        assert(cmon_types_kind(_r->types, fr->type_decls[i].type_idx) == cmon_typek_struct);
        cmon_idx_buf_clear(fr->idx_buf_mng, name_tok_buf);
        // cmon_ast_iter field_it =
        //     cmon_ast_struct_fields_iter(_fr_ast(fr), fr->type_decls[i].ast_idx);
        // cmon_idx ast_idx;
        // while (cmon_is_valid_idx(ast_idx = cmon_ast_iter_next(_fr_ast(fr), &field_it)))
        for (j = 0; j < cmon_ast_struct_fields_count(_fr_ast(fr), fr->type_decls[i].ast_idx); ++j)
        {
            cmon_idx ast_idx = cmon_ast_struct_field(_fr_ast(fr), fr->type_decls[i].ast_idx, j);
            cmon_astk kind = cmon_ast_kind(_fr_ast(fr), ast_idx);
            cmon_idx parsed_type_idx, def_expr;
            if (kind == cmon_astk_struct_field)
            {
                parsed_type_idx = cmon_ast_struct_field_type(_fr_ast(fr), ast_idx);
                def_expr = cmon_ast_struct_field_expr(_fr_ast(fr), ast_idx);
            }
            else
            {
                _unexpected_ast_panic();
            }

            cmon_idx type = _resolve_parsed_type(fr, fr->file_scope, parsed_type_idx);
            if (kind == cmon_astk_struct_field)
            {
                cmon_idx field_name_tok = cmon_ast_struct_field_name(_fr_ast(fr), ast_idx);
                if (!_check_field_name(fr, name_tok_buf, field_name_tok))
                {
                    cmon_types_struct_add_field(
                        _r->types,
                        fr->type_decls[i].type_idx,
                        cmon_tokens_str_view(_fr_tokens(fr), field_name_tok),
                        type,
                        def_expr);
                }
            }
        }
    }

err_end:
    cmon_idx_buf_mng_return(fr->idx_buf_mng, name_tok_buf);
    return cmon_err_handler_count(fr->err_handler) > 0;
}

cmon_bool cmon_resolver_usertypes_def_expr_pass(cmon_resolver * _r, cmon_idx _file_idx)
{
    _file_resolver * fr = &_r->file_resolvers[_file_idx];

    _set_err_jmp_goto(fr, err_end);

    size_t i, j;
    for (i = 0; i < cmon_dyn_arr_count(&fr->type_decls); ++i)
    {
        cmon_idx struct_type_idx = cmon_ast_struct_type(_fr_ast(fr), fr->type_decls[i].ast_idx);
        for (j = 0; j < cmon_types_struct_field_count(_r->types, struct_type_idx); ++j)
        {
            cmon_idx def_expr = cmon_types_struct_field_def_expr(_r->types, struct_type_idx, j);
            cmon_idx field_type = cmon_types_struct_field_type(_r->types, struct_type_idx, j);
            assert(cmon_is_valid_idx(field_type));
            if (cmon_is_valid_idx(def_expr))
            {
                cmon_idx expr_type = _resolve_expr(fr, fr->file_scope, def_expr, field_type);
                if (cmon_is_valid_idx(expr_type))
                {
                    _validate_conversion(fr,
                                         cmon_ast_token(_fr_ast(fr), def_expr),
                                         cmon_ast_token_last(_fr_ast(fr), def_expr),
                                         expr_type,
                                         field_type);
                }
            }
        }

        // while (cmon_is_valid_idx(ast_idx = cmon_ast_iter_next(_fr_ast(fr), &field_it)))
        // {
        //     cmon_astk kind = cmon_ast_kind(_fr_ast(fr), ast_idx);
        //     assert(kind == cmon_astk_struct_field);

        //     cmon_idx def_expr = cmon_ast_struct_field_expr(_fr_ast(fr), ast_idx);
        //     cmon_idx field_type = cmon_types_struct_field_type(_r->types, struct_type_idx, j);
        //     assert(cmon_is_valid_idx(field_type));

        //     cmon_idx expr_type = _resolve_expr(fr, fr->file_scope, def_expr, field_type);
        //     if (cmon_is_valid_idx(expr_type))
        //     {
        //         _validate_conversion(
        //             fr, cmon_ast_token(_fr_ast(fr), def_expr), expr_type, field_type);
        //     }
        //     ++j;
        // }
    }

err_end:
    return cmon_err_handler_count(fr->err_handler) > 0;
}

static void _add_type_dep(cmon_resolver * _r, cmon_dyn_arr(cmon_idx) * _deps, cmon_idx _dep)
{
    cmon_typek kind = cmon_types_kind(_r->types, _dep);
    //@TODO: add optional and variant etc.
    if (kind == cmon_typek_array)
    {
        _add_type_dep(_r, _deps, cmon_types_array_type(_r->types, _dep));
    }
    // else if (_dep->kind == cmon_type_tuple)
    // {
    //     size_t i;
    //     cmon_type_info_tuple * tti = _dep->data;
    //     for (i = 0; i < cmon_dyn_arr_count(tti->types); ++i)
    //         _add_type_dep(_mod, _arr, tti->types[i]);
    // }
    else
    {
        _add_unique_idx(_deps, _dep);
    }
}

cmon_bool cmon_resolver_circ_pass(cmon_resolver * _r)
{
    size_t i, j, k;
    _file_resolver * fr;
    cmon_dep_graph_result result;

    cmon_dep_graph_clear(_r->dep_graph);
    // for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    // {
    //     fr = &_r->file_resolvers[i];
    //     for (j = 0; j < cmon_dyn_arr_count(&fr->type_decls); ++j)
    //     {
    //         // the only user defined type can be a struct for now
    //         assert(cmon_types_kind(_r->types, fr->type_decls[j].type_idx) == cmon_typek_struct);
    //         cmon_dyn_arr_clear(&_r->dep_buffer);
    //         for (k = 0; k < cmon_types_struct_field_count(_r->types, fr->type_decls[j].type_idx);
    //              ++k)
    //         {
    //             _add_type_dep(
    //                 _r,
    //                 &_r->dep_buffer,
    //                 cmon_types_struct_field_type(_r->types, fr->type_decls[j].type_idx, k));
    //         }
    //         cmon_dep_graph_add(_r->dep_graph,
    //                            fr->type_decls[j].type_idx,
    //                            &_r->dep_buffer[0],
    //                            cmon_dyn_arr_count(&_r->dep_buffer));
    //     }
    // }

    for (i = 0; i < cmon_types_count(_r->types); ++i)
    {
        if (!cmon_types_is_builtin(_r->types, (cmon_idx)i) &&
            cmon_types_is_used_in_module(_r->types, (cmon_idx)i, _r->mod_idx))
        {
            cmon_dyn_arr_clear(&_r->dep_buffer);
            if (cmon_types_is_implicit(_r->types, (cmon_idx)i))
            {
                // implicit types are just added without any dependencies
                cmon_dep_graph_add(_r->dep_graph,
                                   (cmon_idx)i,
                                   &_r->dep_buffer[0],
                                   cmon_dyn_arr_count(&_r->dep_buffer));
            }
            else
            {
                // only user defined type can be a struct so far
                assert(cmon_types_kind(_r->types, (cmon_idx)i) == cmon_typek_struct);

                // if the usertype is defined in another module, the circular dep resolution already
                // happened during its own resolve pass. We only add the usertype without any
                // dependencies.
                if (cmon_types_module(_r->types, (cmon_idx)i) == _r->mod_idx)
                {
                    for (j = 0; j < cmon_types_struct_field_count(_r->types, (cmon_idx)i); ++j)
                    {
                        _add_type_dep(_r,
                                      &_r->dep_buffer,
                                      cmon_types_struct_field_type(_r->types, (cmon_idx)i, j));
                    }
                }
                cmon_dep_graph_add(_r->dep_graph,
                                   (cmon_idx)i,
                                   &_r->dep_buffer[0],
                                   cmon_dyn_arr_count(&_r->dep_buffer));
            }
        }
    }

    result = cmon_dep_graph_resolve(_r->dep_graph);
    if (!result.array)
    {
        cmon_idx a = cmon_dep_graph_conflict_a(_r->dep_graph);
        cmon_idx b = cmon_dep_graph_conflict_b(_r->dep_graph);
        cmon_idx src_file_idx = cmon_types_src_file(_r->types, a);
        assert(cmon_is_valid_idx(src_file_idx));
        cmon_idx mod_src_idx = cmon_src_mod_src_idx(_r->src, src_file_idx);
        assert(cmon_is_valid_idx(mod_src_idx));
        _file_resolver * fr = &_r->file_resolvers[mod_src_idx];

        _set_err_jmp_goto(fr, err_end);

        if (a != b)
        {
            _fr_err(fr,
                    cmon_types_name_tok(_r->types, a),
                    cmon_types_name_tok(_r->types, a),
                    cmon_types_name_tok(_r->types, a),
                    "circular dependency between types '%s' and '%s'",
                    cmon_types_name(_r->types, a),
                    cmon_types_name(_r->types, b));
        }
        else
        {
            _fr_err(fr,
                    cmon_types_name_tok(_r->types, a),
                    cmon_types_name_tok(_r->types, a),
                    cmon_types_name_tok(_r->types, a),
                    "recursive type '%s'",
                    cmon_types_name(_r->types, a));
        }
    err_end:
        return cmon_true;
    }

    // // do a second pass with all types, including implicit types to figure out the order of all
    // // types used by the module
    // cmon_dep_graph_clear(_r->dep_graph);
    // for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    // {
    //     fr = &_r->file_resolvers[i];
    //     for (j = 0; j < cmon_dyn_arr_count(&fr->type_decls); ++j)
    //     {
    //         // the only user defined type can be a struct for now
    //         assert(cmon_types_kind(_r->types, fr->type_decls[j].type_idx) == cmon_typek_struct);
    //         cmon_dyn_arr_clear(&_r->dep_buffer);
    //         for (k = 0; k < cmon_types_struct_field_count(_r->types, fr->type_decls[j].type_idx);
    //              ++k)
    //         {
    //             //@NOTE: We are not using add_type_dep here because we want to add the actual,
    //             // unmodified types
    //             _add_unique_idx(
    //                 &_r->dep_buffer,
    //                 cmon_types_struct_field_type(_r->types, fr->type_decls[j].type_idx, k));
    //         }
    //         cmon_dep_graph_add(_r->dep_graph,
    //                            fr->type_decls[j].type_idx,
    //                            &_r->dep_buffer[0],
    //                            cmon_dyn_arr_count(&_r->dep_buffer));
    //     }

    //     for (j = 0; j < cmon_dyn_arr_count(&fr->implicit_types); ++j)
    //     {
    //         if (cmon_types_kind(_r->types, fr->implicit_types[j]) == cmon_typek_array)
    //         {
    //             cmon_dyn_arr_clear(&_r->dep_buffer);
    //             cmon_dyn_arr_append(&_r->dep_buffer,
    //                                 cmon_types_array_type(_r->types, fr->implicit_types[j]));
    //             cmon_dep_graph_add(_r->dep_graph,
    //                                fr->implicit_types[j],
    //                                &_r->dep_buffer[0],
    //                                cmon_dyn_arr_count(&_r->dep_buffer));
    //         }
    //         else
    //         {
    //             cmon_dep_graph_add(_r->dep_graph, fr->implicit_types[j], NULL, 0);
    //         }
    //     }
    // }

    // // this should never fail during the second pass.
    // result = cmon_dep_graph_resolve(_r->dep_graph);
    // assert(result.array);

    // store the sorted types
    cmon_dyn_arr_resize(&_r->sorted_types, result.count);
    for (i = 0; i < result.count; ++i)
    {
        _r->sorted_types[i] = result.array[i];
    }

    return cmon_false;
}

cmon_bool cmon_resolver_globals_pass(cmon_resolver * _r)
{
    size_t i, j;
    _r->global_type_pass = cmon_true;

    //@NOTE: resolve var_decl will no what to do during this pass based on global_type_pass being
    // set.
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr;
        cmon_ast * ast;
        fr = &_r->file_resolvers[i];
        ast = _fr_ast(fr);

        _set_err_jmp_goto(fr, err_end);

        for (j = 0; j < cmon_dyn_arr_count(&fr->global_var_decls); ++j)
        {
            cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_var_decls[j]);
            _resolve_var_decl(fr, fr->file_scope, ast_idx);
        }
    }

err_end:
    _r->global_type_pass = cmon_false;
    return cmon_resolver_has_errors(_r);
}

// @TODO: Add a nested name find to symbols scope and an easy way to get the root scope of an
// expression? Then we could simplify this mess a lot by 01. getting the root scope of a globals
// expression and find its name occurances in there. If any of them matches the global symbol in
// question, we have an init loop. This will do the trick for now.
static inline void _add_global_init_dep(_file_resolver * _fr,
                                        cmon_idx _global_sym,
                                        cmon_idx _ast_idx,
                                        cmon_dyn_arr(cmon_idx) * _out_deps)
{
    cmon_astk kind = cmon_ast_kind(_fr_ast(_fr), _ast_idx);
    if (kind == cmon_astk_ident)
    {
        cmon_idx sym = cmon_ast_ident_sym(_fr_ast(_fr), _ast_idx);
        assert(cmon_is_valid_idx(sym));

        cmon_idx ast = cmon_symbols_ast(_fr->resolver->symbols, sym);
        assert(cmon_is_valid_idx(ast));

        // ignore non variable symbols
        if (cmon_ast_kind(_fr_ast(_fr), ast) == cmon_astk_var_decl)
        {
            if (cmon_ast_kind(_fr_ast(_fr), cmon_ast_var_decl_expr(_fr_ast(_fr), ast)) !=
                cmon_astk_fn_decl)
            {
                cmon_dyn_arr_append(_out_deps, sym);
            }
            if (sym != _global_sym)
            {
                cmon_idx expr_idx = cmon_ast_var_decl_expr(_fr_ast(_fr), ast);
                assert(cmon_is_valid_idx(expr_idx));
                _add_global_init_dep(_fr, _global_sym, expr_idx, _out_deps);
            }
        }
    }
    else if (kind == cmon_astk_fn_decl)
    {
        // check all expressions in the function body
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_fn_block(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_addr)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_addr_expr(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_deref)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_deref_expr(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_prefix)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_prefix_expr(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_binary)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_binary_left(_fr_ast(_fr), _ast_idx), _out_deps);
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_binary_right(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_call)
    {
        size_t i;
        for (i = 0; i < cmon_ast_call_args_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            _add_global_init_dep(
                _fr, _global_sym, cmon_ast_call_arg(_fr_ast(_fr), _ast_idx, i), _out_deps);
        }
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_call_left(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_index)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_index_expr(_fr_ast(_fr), _ast_idx), _out_deps);
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_index_left(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_selector)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_selector_left(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_array_init)
    {
        // cmon_idx idx;
        // cmon_ast_iter it = cmon_ast_array_init_exprs_iter(_fr_ast(_fr), _ast_idx);
        // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
        size_t i;
        for (i = 0; i < cmon_ast_array_init_exprs_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            _add_global_init_dep(
                _fr, _global_sym, cmon_ast_array_init_expr(_fr_ast(_fr), _ast_idx, i), _out_deps);
        }
    }
    else if (kind == cmon_astk_struct_init)
    {
        // cmon_idx idx;
        // cmon_ast_iter it = cmon_ast_struct_init_fields_iter(_fr_ast(_fr), _ast_idx);
        // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
        size_t i;
        for (i = 0; i < cmon_ast_struct_init_fields_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            _add_global_init_dep(
                _fr,
                _global_sym,
                cmon_ast_struct_init_field_expr(
                    _fr_ast(_fr), cmon_ast_struct_init_field(_fr_ast(_fr), _ast_idx, i)),
                _out_deps);
        }
    }
    // stmts
    else if (kind == cmon_astk_block)
    {
        for (size_t i = 0; i < cmon_ast_block_child_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            _add_global_init_dep(
                _fr, _global_sym, cmon_ast_block_child(_fr_ast(_fr), _ast_idx, i), _out_deps);
        }
    }
    else if (kind == cmon_astk_var_decl)
    {
        _add_global_init_dep(
            _fr, _global_sym, cmon_ast_var_decl_expr(_fr_ast(_fr), _ast_idx), _out_deps);
    }
    else if (kind == cmon_astk_int_literal || kind == cmon_astk_float_literal ||
             kind == cmon_astk_bool_literal || kind == cmon_astk_string_literal)
    {
        // these are the ones nothing needs to be done for.
    }
    else
    {
        // make sure nothing unexpected gets passed in
        assert(0);
    }
}

cmon_bool cmon_resolver_main_pass(cmon_resolver * _r, cmon_idx _file_idx)
{
    _file_resolver * fr = &_r->file_resolvers[_file_idx];
    _set_err_jmp_goto(fr, err_end);

    size_t i;
    // iterate over all global variables again and resolve their expressions (for the ones that were
    // not already done doing globals type pass).
    for (i = 0; i < cmon_dyn_arr_count(&fr->global_var_decls); ++i)
    {
        cmon_idx var_ast_idx = cmon_symbols_ast(_r->symbols, fr->global_var_decls[i]);
        assert(cmon_is_valid_idx(var_ast_idx));
        _resolve_var_decl(fr, fr->file_scope, var_ast_idx);

        // validate main function signature
        //@TODO: Better error message, showing which signatures are valid?
        if (fr->global_var_decls[i] == fr->main_fn_sym)
        {
            cmon_idx sig = fr->resolved_types[cmon_ast_var_decl_expr(_fr_ast(fr), var_ast_idx)];
            assert(cmon_types_kind(_r->types, sig) == cmon_typek_fn);
            cmon_idx ret_type = cmon_types_fn_return_type(_r->types, sig);
            if ((cmon_types_kind(_r->types, ret_type) != cmon_typek_s32 &&
                 cmon_types_kind(_r->types, ret_type) != cmon_typek_void) ||
                (cmon_types_fn_param_count(_r->types, sig) != 0 &&
                 cmon_types_fn_param_count(_r->types, sig) != 1))
            {
                _fr_err(fr,
                        cmon_ast_token(_fr_ast(fr), var_ast_idx),
                        cmon_ast_token(_fr_ast(fr), var_ast_idx),
                        cmon_ast_token_last(_fr_ast(fr), var_ast_idx),
                        "bad main function signature");
            }
        }
    }

    // cmon_dep_graph_clear(_r->dep_graph);
    // for (i = 0; i < cmon_dyn_arr_count(&fr->global_var_decls); ++i)
    // {
    //     cmon_idx var_decl_ast = cmon_symbols_ast(fr->resolver->symbols, fr->global_var_decls[i]);
    //     assert(cmon_is_valid_idx(var_decl_ast));
    //     cmon_dyn_arr_clear(&_r->dep_buffer);
    //     _add_global_init_dep(fr,
    //                          fr->global_var_decls[i],
    //                          cmon_ast_var_decl_expr(_fr_ast(fr), var_decl_ast),
    //                          &_r->dep_buffer);

    //     cmon_dep_graph_add(_r->dep_graph,
    //                        fr->global_var_decls[i],
    //                        &_r->dep_buffer[0],
    //                        cmon_dyn_arr_count(&_r->dep_buffer));
    // }

    // cmon_dep_graph_result res = cmon_dep_graph_resolve(_r->dep_graph);
    // if (!res.array)
    // {
    //     cmon_idx a = cmon_dep_graph_conflict_a(_r->dep_graph);
    //     // cmon_idx b = cmon_dep_graph_conflict_b(_r->dep_graph);
    //     cmon_idx file_idx = cmon_src_mod_src_idx(_r->src, cmon_symbols_src_file(_r->symbols, a));
    //     assert(cmon_is_valid_idx(file_idx));
    //     _file_resolver * fr = &_r->file_resolvers[file_idx];
    //     cmon_str_view name = cmon_symbols_name(_r->symbols, a);
    //     _fr_err(fr,
    //             cmon_ast_token(_fr_ast(fr), cmon_symbols_ast(_r->symbols, a)),
    //             "initialization loop for '%.*s'",
    //             name.end - name.begin,
    //             name.begin);
    // }

err_end:
    return cmon_err_handler_count(fr->err_handler) > 0;
}

// functions to generate the IR after everything is done
static inline cmon_idx _ir_add(cmon_resolver * _r, _file_resolver * _fr, cmon_idx _ast_idx);
static inline cmon_idx _ir_add_block(cmon_resolver * _r, _file_resolver * _fr, cmon_idx _ast_idx);

static inline cmon_idx _ir_for_sym(cmon_resolver * _r, cmon_idx _sym)
{
    assert(cmon_is_valid_idx(_r->symbol_ir_map[_sym]));
    return _r->symbol_ir_map[_sym];
}

static inline cmon_idx _ir_add_var_decl_impl(cmon_resolver * _r,
                                             _file_resolver * _fr,
                                             const char * _name,
                                             cmon_idx _ast_idx,
                                             cmon_bool _is_global,
                                             cmon_bool _has_expr)
{
    cmon_short_str name_buf = cmon_short_str_make(_r->alloc, _name);
    cmon_idx sym = cmon_ast_var_decl_sym(_fr_ast(_fr), _ast_idx);
    cmon_idx expr_idx = !_has_expr
                            ? CMON_INVALID_IDX
                            : _ir_add(_r, _fr, cmon_ast_var_decl_expr(_fr_ast(_fr), _ast_idx));

    cmon_idx ret;

    if (_is_global)
    {
        ret = cmon_irb_add_global_var_decl(_r->ir_builder,
                                           cmon_short_str_c_str(&name_buf),
                                           cmon_ast_var_decl_is_pub(_fr_ast(_fr), _ast_idx),
                                           cmon_ast_var_decl_is_mut(_fr_ast(_fr), _ast_idx),
                                           cmon_symbols_var_type(_r->symbols, sym),
                                           expr_idx);
    }
    else
    {
        ret = cmon_irb_add_var_decl(_r->ir_builder,
                                    cmon_short_str_c_str(&name_buf),
                                    cmon_ast_var_decl_is_mut(_fr_ast(_fr), _ast_idx),
                                    cmon_symbols_var_type(_r->symbols, sym),
                                    expr_idx);
    }

    _r->symbol_ir_map[sym] = ret;

    cmon_short_str_dealloc(&name_buf);
    return ret;
}

static inline cmon_idx _ir_add_local_var_decl(cmon_resolver * _r,
                                              _file_resolver * _fr,
                                              cmon_idx _ast_idx)
{
    cmon_idx sym = cmon_ast_var_decl_sym(_fr_ast(_fr), _ast_idx);
    assert(cmon_is_valid_idx(sym));
    return _ir_add_var_decl_impl(_r,
                                 _fr,
                                 cmon_symbols_unique_name(_r->symbols, sym),
                                 _ast_idx,
                                 cmon_false,
                                 cmon_is_valid_idx(cmon_ast_var_decl_expr(_fr_ast(_fr), _ast_idx)));
}

static inline cmon_idx _ir_add_global_var_decl(cmon_resolver * _r,
                                               cmon_idx _sym,
                                               const char * _prefix,
                                               cmon_bool _is_external)
{
    cmon_idx mod_idx = cmon_symbols_module(_r->symbols, _sym);
    cmon_idx src_file_idx = cmon_symbols_src_file(_r->symbols, _sym);
    cmon_resolver * r = cmon_modules_resolver(_r->mods, mod_idx);
    _file_resolver * fr = &r->file_resolvers[cmon_src_mod_src_idx(_r->src, src_file_idx)];
    return _ir_add_var_decl_impl(
        _r,
        fr,
        cmon_str_builder_tmp_str(
            _r->str_builder, "%s_%s", _prefix, cmon_symbols_unique_name(_r->symbols, _sym)),
        cmon_symbols_ast(_r->symbols, _sym),
        cmon_true,
        !_is_external);
}

static inline cmon_idx _ir_add(cmon_resolver * _r, _file_resolver * _fr, cmon_idx _ast_idx)
{
    cmon_astk kind = cmon_ast_kind(_fr_ast(_fr), _ast_idx);

    if (kind == cmon_astk_block)
    {
        return _ir_add_block(_r, _fr, _ast_idx);
    }
    else if (kind == cmon_astk_var_decl)
    {
        return _ir_add_local_var_decl(_r, _fr, _ast_idx);
    }
    else if (kind == cmon_astk_int_literal)
    {
        cmon_str_view sv =
            cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_token(_fr_ast(_fr), _ast_idx));
        return cmon_irb_add_int_lit(
            _r->ir_builder,
            cmon_str_builder_tmp_str(_r->str_builder, "%.*s", sv.end - sv.begin, sv.begin));
    }
    else if (kind == cmon_astk_float_literal)
    {
        cmon_str_view sv =
            cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_token(_fr_ast(_fr), _ast_idx));
        return cmon_irb_add_float_lit(
            _r->ir_builder,
            cmon_str_builder_tmp_str(_r->str_builder, "%.*s", sv.end - sv.begin, sv.begin));
    }
    else if (kind == cmon_astk_bool_literal)
    {
        return cmon_irb_add_bool_lit(
            _r->ir_builder,
            cmon_str_view_c_str_cmp(
                cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_token(_fr_ast(_fr), _ast_idx)),
                "true") == 0
                ? cmon_true
                : cmon_false);
    }
    else if (kind == cmon_astk_string_literal)
    {
        cmon_str_view sv =
            cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_token(_fr_ast(_fr), _ast_idx));
        return cmon_irb_add_string_lit(
            _r->ir_builder,
            cmon_str_builder_tmp_str(_r->str_builder, "%.*s", sv.end - sv.begin, sv.begin));
    }
    else if (kind == cmon_astk_ident)
    {
        cmon_idx sym = cmon_ast_ident_sym(_fr_ast(_fr), _ast_idx);
        assert(cmon_is_valid_idx(sym));
        return cmon_irb_add_ident(_r->ir_builder, _ir_for_sym(_r, sym));
    }
    else if (kind == cmon_astk_addr)
    {
        return cmon_irb_add_addr(_r->ir_builder,
                                 _ir_add(_r, _fr, cmon_ast_addr_expr(_fr_ast(_fr), _ast_idx)));
    }
    else if (kind == cmon_astk_deref)
    {
        return cmon_irb_add_deref(_r->ir_builder,
                                  _ir_add(_r, _fr, cmon_ast_deref_expr(_fr_ast(_fr), _ast_idx)));
    }
    else if (kind == cmon_astk_prefix)
    {
        return cmon_irb_add_prefix(
            _r->ir_builder,
            cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_prefix_op_tok(_fr_ast(_fr), _ast_idx))
                .begin[0],
            _ir_add(_r, _fr, cmon_ast_prefix_expr(_fr_ast(_fr), _ast_idx)));
    }
    else if (kind == cmon_astk_binary)
    {
        return cmon_irb_add_binary(
            _r->ir_builder,
            cmon_tokens_str_view(_fr_tokens(_fr), cmon_ast_binary_op_tok(_fr_ast(_fr), _ast_idx))
                .begin[0],
            _ir_add(_r, _fr, cmon_ast_binary_left(_fr_ast(_fr), _ast_idx)),
            _ir_add(_r, _fr, cmon_ast_binary_right(_fr_ast(_fr), _ast_idx)));
    }
    else if (kind == cmon_astk_selector)
    {
        cmon_typek left_kind = cmon_types_kind(
            _r->types, _fr->resolved_types[cmon_ast_selector_left(_fr_ast(_fr), _ast_idx)]);
        if (left_kind == cmon_typek_modident)
        {
            //@NOTE: If the symbol is in a different module, simply add an ident referring to it to
            // the IR
            //@TODO: More sanity checks/asserts?
            return cmon_irb_add_ident(
                _r->ir_builder, _ir_for_sym(_r, cmon_ast_selector_sym(_fr_ast(_fr), _ast_idx)));
        }
        else
        {
            cmon_str_view sv = cmon_tokens_str_view(
                _fr_tokens(_fr), cmon_ast_selector_name_tok(_fr_ast(_fr), _ast_idx));
            return cmon_irb_add_selector(
                _r->ir_builder,
                _ir_add(_r, _fr, cmon_ast_selector_left(_fr_ast(_fr), _ast_idx)),
                cmon_str_builder_tmp_str(_r->str_builder, "%.*s", sv.end - sv.begin, sv.begin));
        }
    }
    else if (kind == cmon_astk_call)
    {
        cmon_idx idx_buf = cmon_idx_buf_mng_get(_r->idx_buf_mng);
        for (size_t i = 0; i < cmon_ast_call_args_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            cmon_idx_buf_append(_r->idx_buf_mng,
                                idx_buf,
                                _ir_add(_r, _fr, cmon_ast_call_arg(_fr_ast(_fr), _ast_idx, i)));
        }
        cmon_idx ret =
            cmon_irb_add_call(_r->ir_builder,
                              _ir_add(_r, _fr, cmon_ast_call_left(_fr_ast(_fr), _ast_idx)),
                              cmon_idx_buf_ptr(_r->idx_buf_mng, idx_buf),
                              cmon_idx_buf_count(_r->idx_buf_mng, idx_buf));
        cmon_idx_buf_mng_return(_r->idx_buf_mng, idx_buf);
        return ret;
    }
    else if (kind == cmon_astk_index)
    {
        return cmon_irb_add_index(_r->ir_builder,
                                  _ir_add(_r, _fr, cmon_ast_index_left(_fr_ast(_fr), _ast_idx)),
                                  _ir_add(_r, _fr, cmon_ast_index_expr(_fr_ast(_fr), _ast_idx)));
    }
    else if (kind == cmon_astk_array_init)
    {
        cmon_idx idx_buf = cmon_idx_buf_mng_get(_r->idx_buf_mng);
        for (size_t i = 0; i < cmon_ast_array_init_exprs_count(_fr_ast(_fr), _ast_idx); ++i)
        {
            cmon_idx_buf_append(
                _r->idx_buf_mng,
                idx_buf,
                _ir_add(_r, _fr, cmon_ast_array_init_expr(_fr_ast(_fr), _ast_idx, i)));
        }
        cmon_idx ret = cmon_irb_add_array_init(_r->ir_builder,
                                               _fr->resolved_types[_ast_idx],
                                               cmon_idx_buf_ptr(_r->idx_buf_mng, idx_buf),
                                               cmon_idx_buf_count(_r->idx_buf_mng, idx_buf));
        cmon_idx_buf_mng_return(_r->idx_buf_mng, idx_buf);
        return ret;
    }
    else if (kind == cmon_astk_struct_init)
    {
        cmon_idx idx_buf = cmon_idx_buf_mng_get(_fr->idx_buf_mng);
        cmon_idx field_expr_idx_buf =
            cmon_ast_struct_init_resolved_field_idx_buf(_fr_ast(_fr), _ast_idx);
        for (size_t i = 0; i < cmon_idx_buf_count(_fr->idx_buf_mng, field_expr_idx_buf); ++i)
        {
            cmon_idx_buf_append(
                _fr->idx_buf_mng,
                idx_buf,
                _ir_add(_r, _fr, cmon_idx_buf_at(_fr->idx_buf_mng, field_expr_idx_buf, i)));
        }

        cmon_idx ret = cmon_irb_add_struct_init(_r->ir_builder,
                                                _fr->resolved_types[_ast_idx],
                                                cmon_idx_buf_ptr(_fr->idx_buf_mng, idx_buf),
                                                cmon_idx_buf_count(_fr->idx_buf_mng, idx_buf));
        cmon_idx_buf_mng_return(_fr->idx_buf_mng, idx_buf);
        return ret;
    }
    else if (kind == cmon_astk_paran_expr)
    {
        return cmon_irb_add_paran(_r->ir_builder,
                                  _ir_add(_r, _fr, cmon_ast_paran_expr(_fr_ast(_fr), _ast_idx)));
    }
    else if (kind == cmon_astk_alias)
    {
        // nothing to do for these
    }
    else
    {
        assert(0);
    }
    return CMON_INVALID_IDX;
}

static inline cmon_idx _ir_add_block(cmon_resolver * _r, _file_resolver * _fr, cmon_idx _ast_idx)
{
    cmon_idx idx_buf = cmon_idx_buf_mng_get(_r->idx_buf_mng);
    for (size_t i = 0; i < cmon_ast_block_child_count(_fr_ast(_fr), _ast_idx); ++i)
    {
        cmon_idx_buf_append(_r->idx_buf_mng,
                            idx_buf,
                            _ir_add(_r, _fr, cmon_ast_block_child(_fr_ast(_fr), _ast_idx, i)));
    }
    cmon_idx ret = cmon_irb_add_block(_r->ir_builder,
                                      cmon_idx_buf_ptr(_r->idx_buf_mng, idx_buf),
                                      cmon_idx_buf_count(_r->idx_buf_mng, idx_buf));
    cmon_idx_buf_mng_return(_r->idx_buf_mng, idx_buf);
    return ret;
}

static inline cmon_idx _ir_add_fn_from_sym(cmon_resolver * _r,
                                           cmon_idx _var_sym,
                                           const char * _prefix)
{
    cmon_idx idx_buf = cmon_idx_buf_mng_get(_r->idx_buf_mng);
    cmon_idx mod_idx = cmon_symbols_module(_r->symbols, _var_sym);
    cmon_idx src_file_idx = cmon_symbols_src_file(_r->symbols, _var_sym);
    cmon_resolver * r = cmon_modules_resolver(_r->mods, mod_idx);
    _file_resolver * fr = &r->file_resolvers[cmon_src_mod_src_idx(_r->src, src_file_idx)];
    cmon_idx fn_ast = cmon_ast_var_decl_expr(_fr_ast(fr), cmon_symbols_ast(_r->symbols, _var_sym));

    // generate params IR
    for (size_t i = 0; i < cmon_ast_fn_params_count(_fr_ast(fr), fn_ast); ++i)
    {
        cmon_idx_buf_append(
            _r->idx_buf_mng,
            idx_buf,
            _ir_add_local_var_decl(_r, fr, cmon_ast_fn_param(_fr_ast(fr), fn_ast, i)));
    }

    // add the function
    cmon_idx sig = fr->resolved_types[fn_ast];
    cmon_idx ret = cmon_irb_add_fn(
        _r->ir_builder,
        cmon_str_builder_tmp_str(
            _r->str_builder, "%s_%s", _prefix, cmon_symbols_unique_name(_r->symbols, _var_sym)),
        cmon_types_fn_return_type(_r->types, sig),
        cmon_idx_buf_ptr(_r->idx_buf_mng, idx_buf),
        cmon_idx_buf_count(_r->idx_buf_mng, idx_buf),
        cmon_is_valid_idx(_r->main_fn_sym) && _r->main_fn_sym == _var_sym);
    _r->symbol_ir_map[_var_sym] = ret;

    cmon_idx_buf_mng_return(_r->idx_buf_mng, idx_buf);

    return ret;
}

static inline void _ir_add_fn_body(cmon_resolver * _r, _file_resolver * _fr, cmon_idx _sym)
{
    assert(cmon_is_valid_idx(_sym));
    cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, _sym);
    cmon_idx fn_ast = cmon_ast_var_decl_expr(_fr_ast(_fr), ast_idx);
    cmon_idx fn_ir = _ir_for_sym(_r, _sym);
    cmon_idx body = cmon_ast_fn_block(_fr_ast(_fr), fn_ast);
    cmon_irb_fn_set_body(_r->ir_builder, fn_ir, _ir_add_block(_r, _fr, body));
}

static inline void _ir_add_dep(cmon_resolver * _r, cmon_bool * _allready_added_lu, cmon_idx _dep)
{
    if (_allready_added_lu[_dep])
        return;

    for (size_t i = 0; i < cmon_modules_dep_count(_r->mods, _dep); ++i)
    {
        _ir_add_dep(_r, _allready_added_lu, cmon_modules_dep_mod_idx(_r->mods, _dep, (cmon_idx)i));
    }
    _allready_added_lu[_dep] = cmon_true;
    cmon_irb_add_dep(_r->ir_builder, _dep, cmon_modules_prefix(_r->mods, _dep));
}

cmon_ir * cmon_resolver_finalize(cmon_resolver * _r)
{
    size_t i, j;

    cmon_ir * ret = NULL;

    //@TODO: we could minimize the copying around to all these tmp arrays a bunch if we tried. We
    // could at least allocate the external_vars external_fns arrays to the correct size to begin
    // with :).
    cmon_dyn_arr(cmon_idx) external_vars;
    cmon_dyn_arr(cmon_idx) external_fns;
    cmon_dyn_arr(cmon_bool) dep_added_map;
    cmon_dyn_arr_init(&external_vars, _r->alloc, 16);
    cmon_dyn_arr_init(&external_fns, _r->alloc, 16);

    // count global vars and fns
    size_t globals_count = 0;
    size_t fns_count = 0;
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];
        for (j = 0; j < cmon_dyn_arr_count(&fr->global_var_decls); ++j)
        {
            cmon_idx tidx = cmon_symbols_var_type(_r->symbols, fr->global_var_decls[j]);
            assert(cmon_is_valid_idx(tidx));
            cmon_typek kind = cmon_types_kind(_r->types, tidx);
            if (kind == cmon_typek_fn)
            {
                ++fns_count;
            }
            else
            {
                ++globals_count;
            }
        }

        // uniquely merge all external variables into two arrays separated by functions vs regular
        // vars
        for (j = 0; j < cmon_dyn_arr_count(&fr->external_variables); ++j)
        {
            assert(cmon_symbols_kind(_r->symbols, fr->external_variables[j]) == cmon_symk_var);
            cmon_idx type_idx = cmon_symbols_var_type(_r->symbols, fr->external_variables[j]);
            if (cmon_types_kind(_r->types, type_idx) == cmon_typek_fn)
            {
                _add_unique_idx(&external_fns, fr->external_variables[j]);
            }
            else
            {
                _add_unique_idx(&external_vars, fr->external_variables[j]);
            }
        }

        fns_count += cmon_dyn_arr_count(&fr->local_fns);
    }

    fns_count += cmon_dyn_arr_count(&external_fns);
    globals_count += cmon_dyn_arr_count(&external_vars);

    // create the IR builder
    assert(!_r->ir_builder);
    _r->ir_builder = cmon_irb_create(_r->alloc,
                                     cmon_modules_dep_count(_r->mods, _r->mod_idx),
                                     cmon_dyn_arr_count(&_r->sorted_types),
                                     fns_count,
                                     globals_count,
                                     1024);

    // allocate symbol to IR map
    cmon_dyn_arr_resize(&_r->symbol_ir_map, cmon_symbols_count(_r->symbols));
    memset(&_r->symbol_ir_map[0],
           -1,
           cmon_dyn_arr_count(&_r->symbol_ir_map) * sizeof(cmon_idx));

    // // add external symbol type dependencies to sorted types
    // //@NOTE: as the external module is compiled before this one, we can just add the without dep
    // // graph resolving
    // for (i = 0; i < cmon_dyn_arr_count(&external_vars); ++i)
    // {
    //     _add_unique_idx(&_r->sorted_types, cmon_symbols_var_type(_r->symbols, external_vars[i]));
    // }

    // for (i = 0; i < cmon_dyn_arr_count(&external_fns); ++i)
    // {

    //     // cmon_irb_add_type(_r->ir_builder, _r->sorted_types[i]);
    // }

    // add all dependencies to IR in a sorted fashion, including indirect ones. (i.e. if foo depends
    // on bar, also add bars dependencies etc.)
    cmon_dyn_arr_init(&dep_added_map, _r->alloc, cmon_modules_count(_r->mods));
    cmon_dyn_arr_resize(&dep_added_map, cmon_modules_count(_r->mods));
    memset(&dep_added_map[0], cmon_false, cmon_dyn_arr_count(&dep_added_map) * sizeof(cmon_bool));
    for (i = 0; i < cmon_modules_dep_count(_r->mods, _r->mod_idx); ++i)
    {
        _ir_add_dep(
            _r, dep_added_map, cmon_modules_dep_mod_idx(_r->mods, _r->mod_idx, (cmon_idx)i));
    }

    // add sorted types to ir builder
    for (i = 0; i < cmon_dyn_arr_count(&_r->sorted_types); ++i)
    {
        cmon_irb_add_type(_r->ir_builder, _r->sorted_types[i]);
    }

    // add external symbols to IR
    for (i = 0; i < cmon_dyn_arr_count(&external_vars); ++i)
    {
        _ir_add_global_var_decl(
            _r,
            external_vars[i],
            cmon_modules_prefix(_r->mods, cmon_symbols_module(_r->symbols, external_vars[i])),
            cmon_true);
    }

    for (i = 0; i < cmon_dyn_arr_count(&external_fns); ++i)
    {
        _ir_add_fn_from_sym(
            _r,
            external_fns[i],
            cmon_modules_prefix(_r->mods, cmon_symbols_module(_r->symbols, external_fns[i])));
    }

    const char * mod_pref = cmon_modules_prefix(_r->mods, _r->mod_idx);

    // figure out the initialization order or all globals
    cmon_dep_graph_clear(_r->dep_graph);
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];
        _set_err_jmp_goto(fr, err_end);

        for (j = 0; j < cmon_dyn_arr_count(&fr->global_var_decls); ++j)
        {
            cmon_idx var_decl_ast =
                cmon_symbols_ast(fr->resolver->symbols, fr->global_var_decls[j]);
            assert(cmon_is_valid_idx(var_decl_ast));

            // ignore functions
            if (cmon_ast_kind(_fr_ast(fr), cmon_ast_var_decl_expr(_fr_ast(fr), var_decl_ast)) !=
                cmon_astk_fn_decl)
            {
                cmon_dyn_arr_clear(&_r->dep_buffer);
                _add_global_init_dep(fr,
                                     fr->global_var_decls[j],
                                     cmon_ast_var_decl_expr(_fr_ast(fr), var_decl_ast),
                                     &_r->dep_buffer);
                cmon_dep_graph_add(_r->dep_graph,
                                   fr->global_var_decls[j],
                                   &_r->dep_buffer[0],
                                   cmon_dyn_arr_count(&_r->dep_buffer));
            }
        }
    }

    cmon_dep_graph_result res = cmon_dep_graph_resolve(_r->dep_graph);
    if (!res.array)
    {
        cmon_idx a = cmon_dep_graph_conflict_a(_r->dep_graph);
        // cmon_idx b = cmon_dep_graph_conflict_b(_r->dep_graph);
        cmon_idx file_idx = cmon_src_mod_src_idx(_r->src, cmon_symbols_src_file(_r->symbols, a));
        _file_resolver * fr = &_r->file_resolvers[file_idx];
        cmon_str_view name = cmon_symbols_name(_r->symbols, a);
        _fr_err(fr,
                cmon_ast_token(_fr_ast(fr), cmon_symbols_ast(_r->symbols, a)),
                cmon_ast_token(_fr_ast(fr), cmon_symbols_ast(_r->symbols, a)),
                cmon_ast_token_last(_fr_ast(fr), cmon_symbols_ast(_r->symbols, a)),
                "initialization loop for '%.*s'",
                name.end - name.begin,
                name.begin);
    }

    // add global functions to ir builder
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        for (j = 0; j < cmon_dyn_arr_count(&_r->file_resolvers[i].global_fns); ++j)
        {
            _ir_add_fn_from_sym(_r, _r->file_resolvers[i].global_fns[j], mod_pref);
        }
    }

    // add local functions to ir builder
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        for (j = 0; j < cmon_dyn_arr_count(&_r->file_resolvers[i].local_fns); ++j)
        {
            cmon_idx sym = _r->file_resolvers[i].local_fns[j];
            _ir_add_fn_from_sym(
                _r,
                sym,
                cmon_str_builder_tmp_str(_r->str_builder,
                                         "_%s_lfn%lu_%s",
                                         mod_pref,
                                         j,
                                         cmon_symbols_unique_name(_r->symbols, sym)));
        }
    }

    // add sorted globals to ir builder
    for (i = 0; i < res.count; ++i)
    {
        _ir_add_global_var_decl(_r, res.array[i], mod_pref, cmon_false);
    }

    // generate IR for all (non-external) function bodies
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        for (j = 0; j < cmon_dyn_arr_count(&_r->file_resolvers[i].global_fns); ++j)
        {
            _ir_add_fn_body(_r, &_r->file_resolvers[i], _r->file_resolvers[i].global_fns[j]);
        }
    }

    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        for (j = 0; j < cmon_dyn_arr_count(&_r->file_resolvers[i].local_fns); ++j)
        {
            _ir_add_fn_body(_r, &_r->file_resolvers[i], _r->file_resolvers[i].local_fns[j]);
        }
    }

    ret = cmon_irb_ir(_r->ir_builder);

err_end:
    cmon_dyn_arr_dealloc(&dep_added_map);
    cmon_dyn_arr_dealloc(&external_fns);
    cmon_dyn_arr_dealloc(&external_vars);
    return ret;
}

cmon_bool cmon_resolver_has_errors(cmon_resolver * _r)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];
        if (cmon_err_handler_count(fr->err_handler))
            return cmon_true;
    }
    return cmon_false;
}

cmon_bool cmon_resolver_errors(cmon_resolver * _r,
                               cmon_err_report ** _out_errs,
                               size_t * _out_count)
{
    size_t i, j;
    cmon_dyn_arr_clear(&_r->errs);
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];
        for (j = 0; j < cmon_err_handler_count(fr->err_handler); ++j)
        {
            cmon_err_handler_add_err(
                _r->err_handler, cmon_false, cmon_err_handler_err_report(fr->err_handler, j));
        }
        cmon_err_handler_clear(fr->err_handler);
    }

    *_out_errs = cmon_err_handler_err_report(_r->err_handler, 0);
    *_out_count = cmon_err_handler_count(_r->err_handler);

    return cmon_err_handler_count(_r->err_handler) > 0;
}
