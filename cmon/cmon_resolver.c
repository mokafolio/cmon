#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
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
    cmon_str_builder * str_builder;
    cmon_idx_buf_mng * idx_buf_mng;
    cmon_dyn_arr(_ast_type_pair) type_decls; // types declared in the file
    cmon_dyn_arr(cmon_err_report) errs;
    cmon_dyn_arr(cmon_idx) global_var_decls;
    cmon_dyn_arr(cmon_idx) global_alias_decls;
    cmon_idx * resolved_types; // maps ast expr idx to type
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
    cmon_dyn_arr(_file_resolver) file_resolvers;
    cmon_dyn_arr(cmon_idx) dep_buffer;
    cmon_dyn_arr(cmon_err_report) errs;
    size_t max_errors;
    // flag that is set to true during global variable type resolve pass
    cmon_bool global_type_pass;
    cmon_dyn_arr(cmon_idx) globals_pass_stack;
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
    {
        longjmp(*_jmp, 1);
    }
}

#define _fr_err(_fr, _tok, _fmt, ...)                                                              \
    do                                                                                             \
    {                                                                                              \
        _emit_err(_fr->str_builder,                                                                \
                  &_fr->errs,                                                                      \
                  _fr->resolver->src,                                                              \
                  _fr->src_file_idx,                                                               \
                  _tok,                                                                            \
                  _fr->resolver->max_errors,                                                       \
                  &_fr->err_jmp,                                                                   \
                  _fmt,                                                                            \
                  ##__VA_ARGS__);                                                                  \
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
                "redeclaration of '%.*s'",
                str_view.end - str_view.begin,
                str_view.begin);
        return cmon_true;
    }
    return cmon_false;
}

static inline cmon_bool _validate_conversion(_file_resolver * _fr,
                                             cmon_idx _tok,
                                             cmon_idx _from,
                                             cmon_idx _to)
{
    cmon_types * t = _fr->resolver->types;

    if (cmon_types_is_float(t, _from) && cmon_types_is_int(t, _to))
    {
        _fr_err(_fr,
                _tok,
                "truncating '%s' to '%s'",
                cmon_types_name(t, _from),
                cmon_types_name(t, _to));
        return cmon_true;
    }

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
                _tok,
                "cannot convert '%s' to '%s'",
                cmon_types_name(t, _from),
                cmon_types_name(t, _to));
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
                        "dereferenced pointer in lvalue expression is not mutable");
                return cmon_true;
            }
        }
    }
    else
    {
        //@TODO: add more context to error...
        _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), _expr_idx), "lvalue expression expected");
        return cmon_true;
    }

    return cmon_false;
}

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
                            "'%s' is not a module",
                            cmon_symbols_name(_fr->resolver->symbols, mod_sym));
                }
            }
            else
            {
                _fr_err(_fr,
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
            // sanity checks
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
                    printf("muck %lu %lu\n\n", _fr->resolver->globals_pass_stack[i], type_sym);
                    cmon_str_view a = cmon_symbols_name(_fr->resolver->symbols,
                                                        _fr->resolver->globals_pass_stack[i]);
                    cmon_str_view b = cmon_symbols_name(_fr->resolver->symbols, type_sym);
                    printf("%.*s %.*s\n\n", a.end - a.begin, a.begin, b.end - b.begin, b.begin);
                    if (_fr->resolver->globals_pass_stack[i] == type_sym)
                    {
                        cmon_str_view name = cmon_symbols_name(_fr->resolver->symbols, type_sym);
                        _fr_err(_fr,
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
            }
            else
            {
                //@TODO: separate error if it is defined but not pub?
                //@TODO: print what it is?
                _fr_err(_fr,
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
                    "'%.*s' is not defined in module %s",
                    name_str_view.end - name_str_view.begin,
                    name_str_view.begin,
                    cmon_modules_name(_fr->resolver->mods, mod_idx));
            return CMON_INVALID_IDX;
        }
    }
    else if (kind == cmon_astk_type_ptr)
    {
        cmon_idx pt = _resolve_parsed_type(_fr, _scope, cmon_ast_type_ptr_type(ast, _ast_idx));
        if (!cmon_is_valid_idx(pt))
            return CMON_INVALID_IDX;
        ret =
            cmon_types_find_ptr(_fr->resolver->types, pt, cmon_ast_type_ptr_is_mut(ast, _ast_idx));
    }
    else if (kind == cmon_astk_type_fn)
    {
        cmon_ast_iter param_it;
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
        param_it = cmon_ast_type_fn_params_iter(ast, _ast_idx);
        idx_buf = cmon_idx_buf_mng_get(_fr->idx_buf_mng);

        while (cmon_is_valid_idx(idx = cmon_ast_iter_next(ast, &param_it)))
        {
            //@TODO: Check if resolve_parsed_type returns a valid idx and early out if not?
            cmon_idx_buf_append(_fr->idx_buf_mng, idx_buf, _resolve_parsed_type(_fr, _scope, idx));
        }
        ret = cmon_types_find_fn(_fr->resolver->types,
                                 ret_type,
                                 cmon_idx_buf_ptr(_fr->idx_buf_mng, idx_buf),
                                 cmon_idx_buf_count(_fr->idx_buf_mng, idx_buf));
        cmon_idx_buf_mng_return(_fr->idx_buf_mng, idx_buf);
    }

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
            // if (!cmon_is_valid_idx(ret))
            // {
            //     assert(_fr->resolver->global_type_pass);
            //     _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), _ast_idx), "typechecking loop");
            // }
            if (!cmon_is_valid_idx(ret) && _fr->resolver->global_type_pass)
            {
                size_t i;
                for (i = 0; i < cmon_dyn_arr_count(&_fr->resolver->globals_pass_stack); ++i)
                {
                    if (sym == _fr->resolver->globals_pass_stack[i])
                    {
                        _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), _ast_idx), "typechecking loop");
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
            return cmon_types_find_ptr(_fr->resolver->types, type, is_mut);
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
            _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), expr), "can't dereference non-pointer type");
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
    return _resolve_expr(_fr, _scope, cmon_ast_prefix_expr(_fr_ast(_fr), _ast_idx), _lh_type);
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
            cmon_ast_token(_fr_ast(_fr), _ast_idx),
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
                    return cmon_symbols_var_type(_fr->resolver->symbols, selected_sym);
                }
            }
            _fr_err(_fr,
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

            sym = cmon_ast_ident_sym(_fr_ast(_fr), left_expr);
            assert(cmon_is_valid_idx(sym) &&
                   cmon_symbols_kind(_fr->resolver->symbols, sym) == cmon_symk_var);
            type_idx = cmon_symbols_var_type(_fr->resolver->symbols, sym);
            field_idx =
                cmon_types_struct_findv_field(_fr->resolver->types, type_idx, name_str_view);
            if (cmon_is_valid_idx(field_idx))
            {
                return cmon_types_struct_field_type(_fr->resolver->types, type_idx, field_idx);
            }
            else
            {
                _fr_err(_fr,
                        name_tok,
                        "struct '%s' has no field '%.*s'",
                        cmon_types_name(_fr->resolver->types, type_idx),
                        name_str_view.end - name_str_view.begin,
                        name_str_view.begin);
                return CMON_INVALID_IDX;
            }
        }
    }

    _fr_err(_fr,
            name_tok,
            "selector '%*.s' requested in something not an object or module identifier",
            name_str_view.end - name_str_view.begin,
            name_str_view.begin);

    return CMON_INVALID_IDX;
}

static inline cmon_idx _resolve_call(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{

}

static inline cmon_idx _resolve_index(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    cmon_idx lexpr = cmon_ast_index_left(_fr_ast(_fr), _ast_idx);
    cmon_idx idx_expr = cmon_ast_index_expr(_fr_ast(_fr), _ast_idx);
    cmon_idx left_type = _resolve_expr(_fr, _scope, lexpr, CMON_INVALID_IDX);

    if (!cmon_is_valid_idx(left_type))
        return CMON_INVALID_IDX;

    if (!_is_indexable(_fr, left_type))
    {
        _fr_err(_fr,
                cmon_ast_token(_fr_ast(_fr), lexpr),
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
        _fr_err(_fr, cmon_ast_token(_fr_ast(_fr), idx_expr), "non-integer index");
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
    if (cmon_ast_array_init_exprs_begin(_fr_ast(_fr), _ast_idx) ==
        cmon_ast_array_init_exprs_end(_fr_ast(_fr), _ast_idx))
        return CMON_INVALID_IDX;

    cmon_idx type_suggestion = CMON_INVALID_IDX;
    if (cmon_is_valid_idx(_lh_type) &&
        cmon_types_kind(_fr->resolver->types, _lh_type) == cmon_typek_array)
    {
        type_suggestion = cmon_types_array_type(_fr->resolver->types, _lh_type);
    }

    cmon_ast_iter it = cmon_ast_array_init_exprs_iter(_fr_ast(_fr), _ast_idx);
    cmon_idx idx = cmon_ast_iter_next(_fr_ast(_fr), &it);
    cmon_idx type = _resolve_expr(_fr, _scope, idx, type_suggestion);

    if (!cmon_is_valid_idx(type))
        return CMON_INVALID_IDX;

    size_t count = 0;
    while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
    {
        ++count;
        cmon_idx rti = _resolve_expr(_fr, _scope, idx, type);
        if (!cmon_is_valid_idx(rti))
            continue;

        _validate_conversion(_fr, cmon_ast_token(_fr_ast(_fr), idx), rti, type);
    }

    return cmon_types_find_array(_fr->resolver->types, type, count);
}

static inline cmon_idx _resolve_struct_init(_file_resolver * _fr,
                                            cmon_idx _scope,
                                            cmon_idx _ast_idx)
{
    printf("_resolve_struct_init asas\n");
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

    cmon_ast_iter field_it = cmon_ast_struct_init_fields_iter(_fr_ast(_fr), _ast_idx);
    cmon_bool first = cmon_true;
    cmon_bool expect_field_names;
    cmon_idx idx;
    cmon_idx counter = 0;
    while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &field_it)))
    {
        cmon_idx fname_tok = cmon_ast_struct_init_field_name_tok(_fr_ast(_fr), idx);
        cmon_idx expr = cmon_ast_struct_init_field_expr(_fr_ast(_fr), idx);
        if (first)
        {
            first = cmon_false;
            expect_field_names = cmon_is_valid_idx(fname_tok);
        }

        if (counter >= field_count)
        {
            _fr_err(_fr,
                    cmon_ast_token(_fr_ast(_fr), expr),
                    "too many expressions in '%s' literal",
                    cmon_types_name(_fr->resolver->types, type));
            break;
        }

        if ((expect_field_names && !cmon_is_valid_idx(fname_tok)) ||
            (!expect_field_names && cmon_is_valid_idx(fname_tok)))
        {
            _fr_err(_fr,
                    cmon_ast_token(_fr_ast(_fr), expr),
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
                        "no field '%.*s' in '%s'",
                        name_str_view.end - name_str_view.begin,
                        name_str_view.begin,
                        cmon_types_name(_fr->resolver->types, type));
                continue;
            }
        }
        else
        {
            field_idx = counter;
        }

        cmon_idx field_type = cmon_types_struct_field_type(_fr->resolver->types, type, field_idx);
        assert(cmon_is_valid_idx(field_type));
        cmon_idx expr_type = _resolve_expr(_fr, _scope, expr, field_type);
        _validate_conversion(_fr, cmon_ast_token(_fr_ast(_fr), expr), expr_type, field_type);

        cmon_idx_buf_set(_fr->idx_buf_mng, field_initialized_buf, field_idx, expr_type);
        ++counter;
    }

    for (i = 0; i < cmon_idx_buf_count(_fr->idx_buf_mng, field_initialized_buf); ++i)
    {
        if (!cmon_is_valid_idx(cmon_idx_buf_at(_fr->idx_buf_mng, field_initialized_buf, i)))
        {
            if (!cmon_is_valid_idx(cmon_types_struct_field_def_expr(_fr->resolver->types, type, i)))
            {
                _fr_err(_fr,
                        cmon_ast_token(_fr_ast(_fr), _ast_idx),
                        "field '%s' is not initialized",
                        cmon_types_struct_field_name(_fr->resolver->types, type, i));
            }
        }
    }

    cmon_idx_buf_mng_return(_fr->idx_buf_mng, field_initialized_buf);

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

    cmon_ast_iter param_it = cmon_ast_fn_params_iter(_fr_ast(_fr), _ast_idx);
    cmon_idx idx;
    cmon_bool param_err = cmon_false;
    while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &param_it)))
    {
        cmon_astk kind = cmon_ast_kind(_fr_ast(_fr), idx);
        if (kind == cmon_astk_var_decl)
        {
            cmon_idx pt = _resolve_parsed_type(_fr, _scope, idx);
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
                                 cmon_idx_buf_count(_fr->idx_buf_mng, idx_buf));
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
                "redeclaration of parameter '%.*s'",
                str_view.end - str_view.begin,
                str_view.begin);
        return cmon_true;
    }
    else
    {
        //@NOTE: The type of the parameter must have been resolved in _resolve_fn_sig
        assert(cmon_is_valid_idx(_fr->resolved_types[_ast_idx]));
        CMON_UNUSED(cmon_symbols_scope_add_var(_fr->resolver->symbols,
                                               _scope,
                                               str_view,
                                               _fr->resolved_types[_ast_idx],
                                               cmon_false,
                                               _is_mut,
                                               _fr->src_file_idx,
                                               _ast_idx));
    }
    return cmon_false;
}

static inline void _resolve_fn_body(_file_resolver * _fr, cmon_idx _scope, cmon_idx _ast_idx)
{
    //@NOTE: Functions only see file scope variables
    cmon_idx scope = cmon_symbols_scope_begin(_fr->resolver->symbols, _fr->file_scope);
    cmon_ast_iter param_it = cmon_ast_fn_params_iter(_fr_ast(_fr), _ast_idx);
    cmon_idx idx;
    while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &param_it)))
    {
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
        ret = cmon_types_builtin_u8_view(_fr->resolver->types);
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
        // assert(0);
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

    // if its not a global being resolved, create the symbol.
    if (!is_global)
    {
        CMON_UNUSED(cmon_symbols_scope_add_var(
            _fr->resolver->symbols,
            _scope,
            cmon_tokens_str_view(_fr_tokens(_fr),
                                 cmon_ast_var_decl_name_tok(_fr_ast(_fr), _ast_idx)),
            cmon_is_valid_idx(parsed_type_idx) ? ptype_idx : expr_type_idx,
            cmon_false,
            cmon_ast_var_decl_is_mut(_fr_ast(_fr), _ast_idx),
            _fr->src_file_idx,
            _ast_idx));
    }
    // only set the globals symbol type if it was not set above yet
    else if (!cmon_is_valid_idx(parsed_type_idx))
    {
        cmon_idx sym = cmon_ast_var_decl_sym(_fr_ast(_fr), _ast_idx);
        assert(cmon_is_valid_idx(sym));
        cmon_symbols_var_set_type(_fr->resolver->symbols, sym, expr_type_idx);
    }

    if (cmon_is_valid_idx(parsed_type_idx))
    {
        _validate_conversion(_fr, expr_idx, expr_type_idx, ptype_idx);
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
        cmon_idx scope = cmon_symbols_scope_begin(_fr->resolver->symbols, _scope);
        cmon_ast_iter it = cmon_ast_block_iter(_fr_ast(_fr), _ast_idx);
        cmon_idx idx;
        while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_fr_ast(_fr), &it)))
        {
            _resolve_stmt(_fr, scope, idx);
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
    ret->max_errors = _max_errors;
    ret->global_type_pass = cmon_false;
    cmon_dyn_arr_init(&ret->file_resolvers, _alloc, 8);
    cmon_dyn_arr_init(&ret->dep_buffer, _alloc, 32);
    cmon_dyn_arr_init(&ret->errs, _alloc, 16);
    cmon_dyn_arr_init(&ret->globals_pass_stack, _alloc, 8);
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
        cmon_dyn_arr_dealloc(&fr->global_alias_decls);
        cmon_dyn_arr_dealloc(&fr->global_var_decls);
        cmon_dyn_arr_dealloc(&fr->errs);
        cmon_dyn_arr_dealloc(&fr->type_decls);
        cmon_idx_buf_mng_destroy(fr->idx_buf_mng);
        cmon_str_builder_destroy(fr->str_builder);
    }
    cmon_dyn_arr_dealloc(&_r->globals_pass_stack);
    cmon_dyn_arr_dealloc(&_r->errs);
    cmon_dyn_arr_dealloc(&_r->dep_buffer);
    cmon_dyn_arr_dealloc(&_r->file_resolvers);
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
    _r->types = _types;
    _r->symbols = _symbols;
    _r->mods = _mods;
    _r->mod_idx = _mod_idx;
    _r->global_scope = cmon_symbols_scope_begin(_r->symbols, CMON_INVALID_IDX);
    cmon_modules_set_global_scope(_r->mods, _r->mod_idx, _r->global_scope);

    // add all builtins to the global scope
    for (i = 0; i < cmon_types_builtin_count(_r->types); ++i)
    {
        cmon_idx idx = cmon_types_builtin(_r->types, i);
        cmon_symbols_scope_add_type(_r->symbols,
                                    _r->global_scope,
                                    cmon_str_view_make(cmon_types_name(_r->types, idx)),
                                    idx,
                                    cmon_false,
                                    CMON_INVALID_IDX,
                                    CMON_INVALID_IDX);
    }

    for (i = 0; i < cmon_modules_src_file_count(_r->mods, _r->mod_idx); ++i)
    {
        _file_resolver fr;
        cmon_ast * ast;
        cmon_idx src_file_idx;

        src_file_idx = cmon_modules_src_file(_r->mods, _r->mod_idx, i);
        ast = cmon_src_ast(_r->src, src_file_idx);

        fr.str_builder = cmon_str_builder_create(_r->alloc, 1024);
        fr.idx_buf_mng = cmon_idx_buf_mng_create(_r->alloc);
        fr.src_file_idx = src_file_idx;
        fr.resolver = _r;
        fr.file_scope = cmon_symbols_scope_begin(_r->symbols, _r->global_scope);
        cmon_dyn_arr_init(&fr.type_decls, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.errs, _r->alloc, _r->max_errors);
        cmon_dyn_arr_init(&fr.global_var_decls, _r->alloc, 16);
        cmon_dyn_arr_init(&fr.global_alias_decls, _r->alloc, 16);
        fr.resolved_types =
            cmon_allocator_alloc(_r->alloc, sizeof(cmon_idx) * cmon_ast_count(ast)).ptr;
        memset(fr.resolved_types, (int)CMON_INVALID_IDX, sizeof(cmon_idx) * cmon_ast_count(ast));
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

    if (setjmp(fr->err_jmp))
    {
        goto err_end;
    }

    cmon_idx root_block = cmon_ast_root_block(ast);

    cmon_idx idx;
    cmon_ast_iter it = cmon_ast_block_iter(ast, root_block);
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

                    printf("finding module %.*s\n", path.end - path.begin, path.begin);
                    imod_idx = cmon_modules_find(fr->resolver->mods, path);
                    printf("imod_idx %lu\n", imod_idx);

                    if (!cmon_is_valid_idx(imod_idx))
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
                                               cmon_ast_import_pair_ident(ast, ipp_idx)))
                    {
                        cmon_modules_add_dep(fr->resolver->mods, fr->resolver->mod_idx, imod_idx);
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
                _add_global_var_name(_r,
                                     fr,
                                     cmon_ast_var_decl_name_tok(ast, idx),
                                     cmon_ast_var_decl_is_pub(ast, idx),
                                     cmon_ast_var_decl_is_mut(ast, idx),
                                     idx);
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
    _file_resolver * fr = &_r->file_resolvers[_file_idx];

    if (setjmp(fr->err_jmp))
    {
        goto err_end;
    }

    cmon_idx name_tok_buf = cmon_idx_buf_mng_get(fr->idx_buf_mng);
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&fr->type_decls); ++i)
    {
        assert(cmon_types_kind(_r->types, fr->type_decls[i].type_idx) == cmon_typek_struct);
        cmon_idx_buf_clear(fr->idx_buf_mng, name_tok_buf);
        cmon_ast_iter field_it =
            cmon_ast_struct_fields_iter(_fr_ast(fr), fr->type_decls[i].ast_idx);
        cmon_idx ast_idx;
        while (cmon_is_valid_idx(ast_idx = cmon_ast_iter_next(_fr_ast(fr), &field_it)))
        {
            cmon_astk kind = cmon_ast_kind(_fr_ast(fr), ast_idx);
            cmon_idx parsed_type_idx;
            cmon_idx def_expr;
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
            if (cmon_is_valid_idx(def_expr) && cmon_is_valid_idx(type))
            {
                cmon_idx expr_type = _resolve_expr(fr, fr->file_scope, def_expr, type);
                if (cmon_is_valid_idx(expr_type))
                {
                    _validate_conversion(
                        fr, cmon_ast_token(_fr_ast(fr), def_expr), expr_type, type);
                }
            }

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
    return cmon_dyn_arr_count(&fr->errs) > 0;
}

cmon_bool cmon_resolver_circ_pass(cmon_resolver * _r)
{
    size_t i, j, k;
    _file_resolver * fr;
    cmon_dep_graph_result result;

    cmon_dep_graph_clear(_r->dep_graph);
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        fr = &_r->file_resolvers[i];
        for (j = 0; j < cmon_dyn_arr_count(&fr->type_decls); ++j)
        {
            // the only user defined type can be a struct for now
            assert(cmon_types_kind(_r->types, fr->type_decls[j].type_idx) == cmon_typek_struct);
            cmon_dyn_arr_clear(&_r->dep_buffer);
            for (k = 0; k < cmon_types_struct_field_count(_r->types, fr->type_decls[j].type_idx);
                 ++k)
            {
                cmon_dyn_arr_append(
                    &_r->dep_buffer,
                    cmon_types_struct_field_type(_r->types, fr->type_decls[j].type_idx, k));
            }
            cmon_dep_graph_add(_r->dep_graph,
                               fr->type_decls[j].type_idx,
                               &_r->dep_buffer[0],
                               cmon_dyn_arr_count(&_r->dep_buffer));
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

        if (setjmp(fr->err_jmp))
        {
            goto err_end;
        }

        if (a != b)
        {
            _fr_err(fr,
                    cmon_types_name_tok(_r->types, a),
                    "circular dependency between types '%s' and '%s'",
                    cmon_types_name(_r->types, a),
                    cmon_types_name(_r->types, b));
        }
        else
        {
            _fr_err(fr,
                    cmon_types_name_tok(_r->types, a),
                    "recursive type '%s'",
                    cmon_types_name(_r->types, a));
        }
    err_end:
        return cmon_true;
    }

    return cmon_false;
}

cmon_bool cmon_resolver_globals_pass(cmon_resolver * _r)
{
    size_t i, j;
    _r->global_type_pass = cmon_true;

    // first resolve the types of all global aliases and make sure there is no recursion.
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];

        if (setjmp(fr->err_jmp))
        {
            goto err_end;
        }

        for (j = 0; j < cmon_dyn_arr_count(&fr->global_alias_decls); ++j)
        {
            cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_alias_decls[j]);
            _resolve_alias(fr, fr->file_scope, ast_idx);
        }
    }

    //@NOTE: Global variables have their own, slightly different implementation than regular
    // variables because symbol creation and the rest is split into multiple separate steps.
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr;
        cmon_ast * ast;
        fr = &_r->file_resolvers[i];
        ast = _fr_ast(fr);

        if (setjmp(fr->err_jmp))
        {
            goto err_end;
        }

        for (j = 0; j < cmon_dyn_arr_count(&fr->global_var_decls); ++j)
        {
            // cmon_idx type_idx;
            // cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_var_decls[j]);
            // cmon_idx parsed_type_idx = cmon_ast_var_decl_type(ast, ast_idx);
            // if (cmon_is_valid_idx(parsed_type_idx))
            // {
            //     type_idx = _resolve_parsed_type(fr, fr->file_scope, parsed_type_idx);
            // }
            // else
            // {
            //     cmon_idx expr_idx = cmon_ast_var_decl_expr(ast, ast_idx);
            //     assert(cmon_is_valid_idx(expr_idx));
            //     if (cmon_ast_kind(ast, expr_idx) != cmon_astk_fn_decl)
            //     {
            //         // cmon_dyn_arr_append(&fr->globals_pass_stack, fr->global_var_decls[j]);
            //         type_idx = _resolve_expr(fr, fr->file_scope, expr_idx, CMON_INVALID_IDX);
            //         // cmon_dyn_arr_pop(&fr->globals_pass_stack);
            //     }
            //     else
            //     {
            //         // for functions we only want to determine the signature for now
            //         type_idx = _resolve_fn_sig(fr, fr->file_scope, expr_idx);
            //     }
            // }

            // cmon_symbols_var_set_type(_r->symbols, fr->global_var_decls[j], type_idx);
            cmon_idx ast_idx = cmon_symbols_ast(_r->symbols, fr->global_var_decls[j]);
            _resolve_var_decl(fr, fr->file_scope, ast_idx);
        }
    }

err_end:
    _r->global_type_pass = cmon_false;
    return cmon_resolver_has_errors(_r);
}

cmon_bool cmon_resolver_main_pass(cmon_resolver * _r, cmon_idx _file_idx)
{
    _file_resolver * fr = &_r->file_resolvers[_file_idx];

    if (setjmp(fr->err_jmp))
    {
        goto err_end;
    }

    size_t i;
    // @NOTE: only thing left to do is to iterate over all global variables. Resolve their default
    // expressions if that has not happened during the globals pass yet. If they are functions,
    // resolve the function bodies!
    for (i = 0; i < cmon_dyn_arr_count(&fr->global_var_decls); ++i)
    {
        cmon_idx var_ast_idx = cmon_symbols_ast(_r->symbols, fr->global_var_decls[i]);
        assert(cmon_is_valid_idx(var_ast_idx));
        cmon_idx expr_idx = cmon_ast_var_decl_expr(_fr_ast(fr), var_ast_idx);
        assert(cmon_is_valid_idx(expr_idx));

        // if its not a function or the function had an explicit type, resolve the var decl
        // expression rhs and validate the conversion
        if (cmon_ast_kind(_fr_ast(fr), expr_idx) != cmon_astk_fn_decl ||
            (!cmon_is_valid_idx(fr->resolved_types[expr_idx]) ||
             cmon_ast_kind(_fr_ast(fr), expr_idx) == cmon_astk_struct_init))
        {
            // only do this for expressions that have not been resolved during globals pass
            cmon_idx type_suggestion = CMON_INVALID_IDX;
            cmon_idx ptype = cmon_ast_var_decl_type(_fr_ast(fr), var_ast_idx);
            if (cmon_is_valid_idx(ptype))
            {
                type_suggestion = fr->resolved_types[ptype];
                assert(cmon_is_valid_idx(type_suggestion));
            }
            cmon_idx type_idx = _resolve_expr(fr, fr->file_scope, expr_idx, type_suggestion);
            if (cmon_is_valid_idx(type_idx) && cmon_is_valid_idx(type_suggestion))
            {
                _validate_conversion(
                    fr, cmon_ast_token(_fr_ast(fr), expr_idx), type_idx, type_suggestion);
            }
        }
        else
        {
            // otherwise the function signature must have already been resolved during globals type
            // pass and we only need to resolve the function body
            assert(cmon_is_valid_idx(fr->resolved_types[expr_idx]));
            _resolve_fn_body(fr, fr->file_scope, expr_idx);
        }
    }

err_end:
    return cmon_dyn_arr_count(&fr->errs) > 0;
}

cmon_bool cmon_resolver_has_errors(cmon_resolver * _r)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_r->file_resolvers); ++i)
    {
        _file_resolver * fr = &_r->file_resolvers[i];
        if (cmon_dyn_arr_count(&fr->errs))
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
        for (j = 0; j < cmon_dyn_arr_count(&fr->errs); ++j)
        {
            cmon_dyn_arr_append(&_r->errs, fr->errs[j]);
        }
        cmon_dyn_arr_clear(&fr->errs);
    }

    *_out_errs = _r->errs;
    *_out_count = cmon_dyn_arr_count(&_r->errs);

    return cmon_dyn_arr_count(&_r->errs) > 0;
}

// cmon_bool cmon_resolver_resolve(cmon_resolver * _r)
// {
//     size_t i;
//     for (i = 0; i < cmon_modules_src_file_count(_r->mods, _r->mod_idx); ++i)
//     {
//         cmon_resolver_top_lvl_pass(_r, i);
//     }

//     if (cmon_resolver_has_errors(_r))
//         return cmon_true;

//     if (cmon_resolver_globals_pass(_r))
//         return cmon_true;

//     for (i = 0; i < cmon_modules_src_file_count(_r->mods, _r->mod_idx); ++i)
//     {
//         cmon_resolver_usertypes_pass(_r, i);
//     }

//     if (cmon_resolver_has_errors(_r))
//         return cmon_true;

//     if (cmon_resolver_circ_pass(_r))
//         return cmon_true;

//     for (i = 0; i < cmon_modules_src_file_count(_r->mods, _r->mod_idx); ++i)
//     {
//         cmon_resolver_main_pass(_r, i);
//     }

//     return cmon_resolver_has_errors(_r);
// }
