#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_handler.h>
#include <cmon/cmon_idx_buf_mng.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_str_builder.h>
#include <setjmp.h>
#include <stdarg.h>

// for now the parser stops on the first error. we simpy save the error and jump out of parsing.
// #define _err(_p, _tok, _msg, ...) \
//     do \
//     { \
//         cmon_str_builder_clear(_p->err_str_builder); \
//         cmon_str_builder_append_fmt(_p->err_str_builder, _msg, ##__VA_ARGS__); \
//         _p->err = cmon_err_report_make(cmon_src_filename(_p->src, _p->src_file_idx), \
//                                        cmon_tokens_line(_p->tokens, _tok), \
//                                        cmon_tokens_line_offset(_p->tokens, _tok), \
//                                        cmon_str_builder_c_str(_p->err_str_builder)); \
//         longjmp(_p->err_jmp, 1); \
//     } while (0)

#define _err(_p, _tok, _fmt, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        cmon_err_handler_err(                                                                      \
            _p->err_handler, cmon_true, _p->src_file_idx, _tok, _fmt, ##__VA_ARGS__);              \
    } while (0)

typedef enum
{
    _precedence_nil,
    _precedence_assign,
    _precedence_range,
    _precedence_or,
    _precedence_and,
    _precedence_comp,
    _precedence_sum,
    _precedence_product,
    _precedence_prefix,
    _precedence_cast
} _precedence;

static _precedence _tok_prec(cmon_tokk _kind)
{
    switch (_kind)
    {
    case cmon_tokk_or:
        return _precedence_or;
    case cmon_tokk_and:
        return _precedence_and;
    case cmon_tokk_less:
    case cmon_tokk_less_equal:
    case cmon_tokk_greater:
    case cmon_tokk_greater_equal:
    case cmon_tokk_equals:
    case cmon_tokk_not_equals:
        return _precedence_comp;
    case cmon_tokk_assign:
    case cmon_tokk_plus_assign:
    case cmon_tokk_minus_assign:
    case cmon_tokk_mult_assign:
    case cmon_tokk_div_assign:
    case cmon_tokk_mod_assign:
    case cmon_tokk_bw_and_assign:
    case cmon_tokk_bw_or_assign:
    case cmon_tokk_bw_xor_assign:
    case cmon_tokk_bw_left_assign:
    case cmon_tokk_bw_right_assign:
        return _precedence_assign;
    case cmon_tokk_plus:
    case cmon_tokk_minus:
        return _precedence_sum;
    case cmon_tokk_mult:
    case cmon_tokk_div:
    case cmon_tokk_mod:
        return _precedence_product;
    case cmon_tokk_as:
        return _precedence_cast;
    default:
        return _precedence_nil;
    }
}

typedef struct cmon_parser
{
    cmon_allocator * alloc;
    cmon_src * src;
    cmon_idx src_file_idx;
    cmon_astb * ast_builder;
    cmon_str_builder * tk_str_builder;
    cmon_idx_buf_mng * idx_buf_mng;
    cmon_tokens * tokens;
    cmon_err_handler * err_handler;
    jmp_buf err_jmp;
} cmon_parser;

const char * _token_kinds_to_str(cmon_str_builder * _b, va_list _args)
{
    cmon_tokk kind;

    cmon_str_builder_clear(_b);
    do
    {
        kind = va_arg(_args, cmon_tokk);
        if (kind != -1)
        {
            if (cmon_str_builder_count(_b))
                cmon_str_builder_append(_b, " or ");
            cmon_str_builder_append_fmt(_b, "'%s'", cmon_tokk_to_str(kind));
        }
    } while (kind != -1);
    return cmon_str_builder_c_str(_b);
}

static inline cmon_idx _token_check_impl(cmon_parser * _p, cmon_bool _allow_line_break, ...)
{
    va_list args, cpy;
    va_start(args, _allow_line_break);
    va_copy(cpy, args);
    cmon_idx tok = cmon_tokens_accept_impl_v(_p->tokens, args);
    if (!cmon_is_valid_idx(tok))
    {
        const char * tk_kinds_str = _token_kinds_to_str(_p->tk_str_builder, cpy);
        va_end(cpy);
        va_end(args);

        cmon_idx cur = cmon_tokens_current(_p->tokens);
        if (cmon_tokens_kind(_p->tokens, cur) != cmon_tokk_eof)
        {
            cmon_str_view name = cmon_tokens_str_view(_p->tokens, cur);
            _err(_p,
                 cur,
                 "%s expected, got '%.*s'",
                 tk_kinds_str,
                 name.end - name.begin,
                 name.begin);
        }
        else
        {
            _err(_p, cur, "%s expected, got 'EOF'", tk_kinds_str);
        }
    }

    //@TODO: error if it does not _allow_line_break

    va_end(args);
    return tok;
}

#define _tok_check(_p, _allow_line_break, ...)                                                     \
    _token_check_impl(                                                                             \
        _p, _allow_line_break, _CMON_VARARG_APPEND_LAST(CMON_INVALID_IDX, __VA_ARGS__))

#define _accept(_p, _out_tok, ...)                                                                 \
    (cmon_is_valid_idx(*(_out_tok) = cmon_tokens_accept(_p->tokens, __VA_ARGS__)))

static cmon_idx _parse_expr(cmon_parser * _p, _precedence _prec);
static cmon_idx _parse_block(cmon_parser * _p, cmon_idx _open_tok);
static cmon_idx _parse_stmt(cmon_parser * _p);

static cmon_idx _parse_type(cmon_parser * _p)
{
    cmon_idx tok, tmp, mod, ret_type;
    cmon_bool is_mut;
    cmon_idx param_buf;

    if (_accept(_p, &tok, cmon_tokk_mult))
    {
        is_mut = _accept(_p, &tmp, cmon_tokk_mut);
        return cmon_astb_add_type_ptr(_p->ast_builder, tok, is_mut, _parse_type(_p));
    }
    else if (_accept(_p, &tok, cmon_tokk_square_open))
    {
        //@TODO: Once there is some more elaborate compile time expressions, we need to take that
        //into account here
        cmon_idx count_tok;
        cmon_bool is_array = _accept(_p, &count_tok, cmon_tokk_int);
        _tok_check(_p, cmon_true, cmon_tokk_square_close);

        if(is_array)
        {
            return cmon_astb_add_type_array(_p->ast_builder, tok, atol(cmon_tokens_str_view(_p->tokens, count_tok).begin), _parse_type(_p));
        }
        else
        {
            cmon_bool is_mut = _accept(_p, &count_tok, cmon_tokk_mut);
            return cmon_astb_add_type_view(_p->ast_builder, tok, is_mut, _parse_type(_p));
        }
    }
    else if (_accept(_p, &tok, cmon_tokk_fn))
    {
        _tok_check(_p, cmon_true, cmon_tokk_paran_open);
        param_buf = cmon_idx_buf_mng_get(_p->idx_buf_mng);
        while (!cmon_tokens_is_current(_p->tokens, cmon_tokk_paran_close, cmon_tokk_eof))
        {
            cmon_idx_buf_append(_p->idx_buf_mng, param_buf, _parse_type(_p));
            if (_accept(_p, &tmp, cmon_tokk_comma))
            {
                if (cmon_tokens_is_current(_p->tokens, cmon_tokk_paran_close))
                {
                    _err(_p, tmp, "unexpected comma");
                    return CMON_INVALID_IDX;
                }
            }
            else
                break;
        }

        _tok_check(_p, cmon_true, cmon_tokk_paran_close);

        if (_accept(_p, &tmp, cmon_tokk_arrow))
        {
            ret_type = _parse_type(_p);
        }
        else
        {
            ret_type = CMON_INVALID_IDX;
        }

        cmon_idx ret = cmon_astb_add_type_fn(_p->ast_builder,
                                             tok,
                                             ret_type,
                                             cmon_idx_buf_ptr(_p->idx_buf_mng, param_buf),
                                             cmon_idx_buf_count(_p->idx_buf_mng, param_buf));
        cmon_idx_buf_mng_return(_p->idx_buf_mng, param_buf);
        return ret;
    }
    else if (_accept(_p, &tok, cmon_tokk_ident))
    {
        mod = CMON_INVALID_IDX;
        if (_accept(_p, &tmp, cmon_tokk_dot))
        {
            mod = tok;
            tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
        }
        return cmon_astb_add_type_named(_p->ast_builder, mod, tok);
    }

    //@TODO: error, unexpected token

    return CMON_INVALID_IDX;
}

static inline void _parse_comma_separated_exprs(cmon_parser * _p,
                                                cmon_idx _idx_buf,
                                                cmon_tokk _end_tkind)
{
    cmon_idx tmp;
    while (!cmon_tokens_is_current(_p->tokens, _end_tkind, cmon_tokk_eof))
    {
        cmon_idx_buf_append(_p->idx_buf_mng, _idx_buf, _parse_expr(_p, _precedence_nil));
        if (_accept(_p, &tmp, cmon_tokk_comma))
        {
            // printf("current %s\n",
            //        cmon_tokk_to_str(cmon_tokens_kind(_p->tokens,
            //        cmon_tokens_current(_p->tokens))));
            if (cmon_tokens_is_current(_p->tokens, _end_tkind))
            {
                _err(_p, tmp, "unexpected comma");
                return;
            }
        }
        else
            break;
    }
    _tok_check(_p, cmon_true, _end_tkind);
}

static cmon_idx _parse_call_expr(cmon_parser * _p, cmon_idx _tok, cmon_idx _lhs)
{
    cmon_idx ret;
    cmon_idx b;

    b = cmon_idx_buf_mng_get(_p->idx_buf_mng);

    _parse_comma_separated_exprs(_p, b, cmon_tokk_paran_close);

    ret = cmon_astb_add_call(_p->ast_builder,
                             _tok,
                             _lhs,
                             cmon_idx_buf_ptr(_p->idx_buf_mng, b),
                             cmon_idx_buf_count(_p->idx_buf_mng, b));
    cmon_idx_buf_mng_return(_p->idx_buf_mng, b);
    return ret;
}

static cmon_idx _parse_selector_expr(cmon_parser * _p, cmon_idx _tok, cmon_idx _lhs)
{
    cmon_idx name_tok;
    name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
    return cmon_astb_add_selector(_p->ast_builder, _tok, _lhs, name_tok);
}

static cmon_idx _parse_index(cmon_parser * _p, cmon_idx _tok, cmon_idx _lhs)
{
    cmon_idx expr;
    expr = _parse_expr(_p, _precedence_nil);
    _tok_check(_p, cmon_true, cmon_tokk_square_close);
    return cmon_astb_add_selector(_p->ast_builder, _tok, _lhs, expr);
}

static cmon_idx _parse_fn(cmon_parser * _p, cmon_idx _fn_tok_idx)
{
    cmon_idx tmp;
    cmon_idx param_buf = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    // name_tok_buf = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    _tok_check(_p, cmon_true, cmon_tokk_paran_open);

    while (!cmon_tokens_is_current(_p->tokens, cmon_tokk_paran_close, cmon_tokk_eof))
    {
        cmon_bool is_mut = _accept(_p, &tmp, cmon_tokk_mut);

        cmon_idx name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
        _tok_check(_p, cmon_true, cmon_tokk_colon);

        cmon_idx type = _parse_type(_p);

        cmon_idx_buf_append(
            _p->idx_buf_mng,
            param_buf,
            cmon_astb_add_var_decl(
                _p->ast_builder, name_tok, cmon_false, is_mut, type, CMON_INVALID_IDX));

        if (_accept(_p, &tmp, cmon_tokk_comma))
        {
            if (cmon_tokens_is_current(_p->tokens, cmon_tokk_paran_close))
            {
                _err(_p, tmp, "unexpected comma");
                cmon_idx_buf_mng_return(_p->idx_buf_mng, param_buf);
                return CMON_INVALID_IDX;
            }
        }
        else
            break;
    }

    _tok_check(_p, cmon_true, cmon_tokk_paran_close);

    cmon_idx ret_type;
    if (_accept(_p, &tmp, cmon_tokk_arrow))
        ret_type = _parse_type(_p);
    else
        ret_type = CMON_INVALID_IDX;

    cmon_idx body = _parse_block(_p, _tok_check(_p, cmon_true, cmon_tokk_curl_open));
    cmon_idx ret = cmon_astb_add_fn_decl(_p->ast_builder,
                                         _fn_tok_idx,
                                         ret_type,
                                         cmon_idx_buf_ptr(_p->idx_buf_mng, param_buf),
                                         cmon_idx_buf_count(_p->idx_buf_mng, param_buf),
                                         body);

    cmon_idx_buf_mng_return(_p->idx_buf_mng, param_buf);

    return ret;
}

// parse a var decl in pretty function form (i.e. fn foo() {} instead of foo := fn(){})
static cmon_idx _parse_pretty_fn(cmon_parser * _p)
{
    cmon_idx tmp;
    cmon_bool is_pub = _accept(_p, &tmp, cmon_tokk_pub);
    cmon_idx fn_tok = _tok_check(_p, cmon_true, cmon_tokk_fn);
    cmon_idx name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);

    return cmon_astb_add_var_decl(
        _p->ast_builder, name_tok, is_pub, cmon_false, CMON_INVALID_IDX, _parse_fn(_p, fn_tok));
}

static cmon_idx _parse_struct_init(cmon_parser * _p)
{
    cmon_idx tmp;

    cmon_idx b = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    // cmon_idx struct_name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
    // if(_accept(_p, &tmp, cmon_tokk_dot))
    // {
    //     mod_name_tok = struct_name_tok;
    //     struct_name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
    // }

    cmon_idx ptype = _parse_type(_p);

    _tok_check(_p, cmon_true, cmon_tokk_curl_open);
    while (!cmon_tokens_is_current(_p->tokens, cmon_tokk_curl_close, cmon_tokk_eof))
    {
        cmon_idx name_tok, field_idx;
        if (cmon_tokens_is_current(_p->tokens, cmon_tokk_ident) &&
            cmon_tokens_is_next(_p->tokens, cmon_tokk_colon))
        {
            name_tok = cmon_tokens_advance(_p->tokens, cmon_true);
            cmon_tokens_advance(_p->tokens, cmon_true); // skip colon
            field_idx = cmon_astb_add_struct_init_field(
                _p->ast_builder, name_tok, name_tok, _parse_expr(_p, _precedence_nil));
        }
        else
        {
            field_idx = cmon_astb_add_struct_init_field(_p->ast_builder,
                                                        cmon_tokens_current(_p->tokens),
                                                        CMON_INVALID_IDX,
                                                        _parse_expr(_p, _precedence_nil));
        }
        cmon_idx_buf_append(_p->idx_buf_mng, b, field_idx);

        if (_accept(_p, &tmp, cmon_tokk_comma))
        {
            if (cmon_tokens_is_current(_p->tokens, cmon_tokk_curl_close))
            {
                _err(_p, tmp, "unexpected comma");
                cmon_idx_buf_mng_return(_p->idx_buf_mng, b);
                return CMON_INVALID_IDX;
            }
        }
        else
            break;
    }
    _tok_check(_p, cmon_true, cmon_tokk_curl_close);
    cmon_idx ret = cmon_astb_add_struct_init(_p->ast_builder,
                                             ptype,
                                             cmon_idx_buf_ptr(_p->idx_buf_mng, b),
                                             cmon_idx_buf_count(_p->idx_buf_mng, b));
    cmon_idx_buf_mng_return(_p->idx_buf_mng, b);
    return ret;
}

static cmon_idx _parse_array_init(cmon_parser * _p, cmon_idx _tok)
{
    cmon_idx idx_buf = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    _parse_comma_separated_exprs(_p, idx_buf, cmon_tokk_square_close);
    cmon_idx ret = cmon_astb_add_array_init(_p->ast_builder,
                                            _tok,
                                            cmon_idx_buf_ptr(_p->idx_buf_mng, idx_buf),
                                            cmon_idx_buf_count(_p->idx_buf_mng, idx_buf));
    cmon_idx_buf_mng_return(_p->idx_buf_mng, idx_buf);
    return ret;
}

static cmon_idx _parse_expr(cmon_parser * _p, _precedence _prec)
{
    cmon_idx tok, ret;

    printf("_parse_expr\n\n");
    if (cmon_tokens_is_current(_p->tokens, cmon_tokk_ident))
    {
        cmon_idx ctok = cmon_tokens_current(_p->tokens);
        if (cmon_tokens_is_next(_p->tokens, cmon_tokk_curl_open) ||
            (cmon_tokens_is_next(_p->tokens, cmon_tokk_dot) &&
             cmon_tokens_is(_p->tokens, ctok + 2, cmon_tokk_ident) &&
             cmon_tokens_is(_p->tokens, ctok + 3, cmon_tokk_curl_open)))
        {
            ret = _parse_struct_init(_p);
        }
        else
        {
            ret = cmon_astb_add_ident(_p->ast_builder, cmon_tokens_advance(_p->tokens, cmon_true));
        }
    }
    else if (_accept(_p, &tok, cmon_tokk_true, cmon_tokk_false))
    {
        ret = cmon_astb_add_bool_lit(_p->ast_builder, tok);
    }
    else if (_accept(_p, &tok, cmon_tokk_float))
    {
        ret = cmon_astb_add_float_lit(_p->ast_builder, tok);
    }
    else if (_accept(_p, &tok, cmon_tokk_int))
    {
        printf("int liiiit\n");
        ret = cmon_astb_add_int_lit(_p->ast_builder, tok);
    }
    else if (_accept(_p, &tok, cmon_tokk_string))
    {
        ret = cmon_astb_add_string_lit(_p->ast_builder, tok);
    }
    else if (_accept(_p, &tok, cmon_tokk_paran_open))
    {
        ret = cmon_astb_add_paran(_p->ast_builder, tok, _parse_expr(_p, _precedence_nil));
        _tok_check(_p, cmon_true, cmon_tokk_paran_close);
    }
    else if (_accept(_p, &tok, cmon_tokk_fn))
    {
        ret = _parse_fn(_p, tok);
    }
    else if (_accept(_p, &tok, cmon_tokk_minus, cmon_tokk_exclam))
    {
        ret = cmon_astb_add_prefix(_p->ast_builder, tok, _parse_expr(_p, _precedence_prefix));
    }
    else if (_accept(_p, &tok, cmon_tokk_square_open))
    {
        ret = _parse_array_init(_p, tok);
    }
    else if (_accept(_p, &tok, cmon_tokk_bw_and))
    {
        ret = cmon_astb_add_addr(_p->ast_builder, tok, _parse_expr(_p, _precedence_prefix));
    }
    else if (_accept(_p, &tok, cmon_tokk_mult))
    {
        ret = cmon_astb_add_deref(_p->ast_builder, tok, _parse_expr(_p, _precedence_prefix));
    }
    else
    {
        //@TODO: Better error
        _err(_p, cmon_tokens_current(_p->tokens), "unexpected token");
        return CMON_INVALID_IDX;
    }

    if (!cmon_is_valid_idx(ret))
    {
        return CMON_INVALID_IDX;
    }

    while (cmon_tokens_is_current(
        _p->tokens, cmon_tokk_paran_open, cmon_tokk_dot, cmon_tokk_square_open))
    {
        if (_accept(_p, &tok, cmon_tokk_paran_open))
        {
            ret = _parse_call_expr(_p, tok, ret);
        }
        else if (_accept(_p, &tok, cmon_tokk_dot))
        {
            ret = _parse_selector_expr(_p, tok, ret);
        }
        else if (_accept(_p, &tok, cmon_tokk_square_open))
        {
            ret = _parse_index(_p, tok, ret);
        }

        if (!cmon_is_valid_idx(ret))
        {
            return CMON_INVALID_IDX;
        }
    }

    while (cmon_tokens_is_current(_p->tokens, CMON_BIN_TOKS, cmon_tokk_as))
    {
        tok = cmon_tokens_current(_p->tokens);
        if (_tok_prec(cmon_tokens_kind(_p->tokens, tok)) < _prec)
            break;
        cmon_tokens_advance(_p->tokens, cmon_true);

        if (cmon_tokens_is(_p->tokens, tok, CMON_BIN_TOKS))
        {
            ret =
                cmon_astb_add_binary(_p->ast_builder,
                                     tok,
                                     ret,
                                     _parse_expr(_p, _tok_prec(cmon_tokens_kind(_p->tokens, tok))));
        }

        if (!cmon_is_valid_idx(ret))
        {
            return CMON_INVALID_IDX;
        }
    }

    return ret;
}

static cmon_idx _parse_block(cmon_parser * _p, cmon_idx _open_tok)
{
    cmon_idx ret, stmt;
    cmon_idx b;

    b = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    while (!cmon_tokens_is_current(_p->tokens, cmon_tokk_curl_close, cmon_tokk_eof))
    {
        printf("ADDING STMT a\n\n");
        if (cmon_is_valid_idx(stmt = _parse_stmt(_p)))
        {
            printf("ADDING STMT b\n\n");
            cmon_idx_buf_append(_p->idx_buf_mng, b, stmt);
        }
        else
        {
            return CMON_INVALID_IDX;
        }
    }
    printf("ADDING STMT c\n\n");
    _tok_check(_p, cmon_true, cmon_tokk_curl_close);

    ret = cmon_astb_add_block(_p->ast_builder,
                              _open_tok,
                              cmon_idx_buf_ptr(_p->idx_buf_mng, b),
                              cmon_idx_buf_count(_p->idx_buf_mng, b));
    cmon_idx_buf_mng_return(_p->idx_buf_mng, b);
    return ret;
}

static inline cmon_bool _peek_var_decl(cmon_parser * _p)
{
    return cmon_tokens_is_current(_p->tokens, cmon_tokk_mut) ||
           ((cmon_tokens_is_current(_p->tokens, cmon_tokk_ident) &&
             cmon_tokens_is_next(_p->tokens, cmon_tokk_colon, cmon_tokk_comma)) ||
            (cmon_tokens_is_current(_p->tokens, cmon_tokk_pub) &&
             cmon_tokens_is_next(_p->tokens, cmon_tokk_ident, cmon_tokk_mut)));
}

static inline cmon_bool _peek_fn_decl(cmon_parser * _p, cmon_bool _is_top_lvl)
{
    return cmon_tokens_is_current(_p->tokens, cmon_tokk_fn) ||
           (_is_top_lvl && cmon_tokens_is_current(_p->tokens, cmon_tokk_pub) &&
            cmon_tokens_is_next(_p->tokens, cmon_tokk_fn));
}

static cmon_idx _parse_var_decl(cmon_parser * _p, cmon_bool _top_lvl)
{
    cmon_idx tmp;

    cmon_bool is_pub = _top_lvl && _accept(_p, &tmp, cmon_tokk_pub);
    cmon_bool is_mut = _accept(_p, &tmp, cmon_tokk_mut);

    cmon_idx name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);

    cmon_idx type;
    _tok_check(_p, cmon_true, cmon_tokk_colon);
    if (_accept(_p, &tmp, cmon_tokk_assign))
    {
        type = CMON_INVALID_IDX;
    }
    else
    {
        type = _parse_type(_p);
        _tok_check(_p, cmon_true, cmon_tokk_assign);
    }

    return cmon_astb_add_var_decl(
        _p->ast_builder, name_tok, is_pub, is_mut, type, _parse_expr(_p, _precedence_nil));
}

static void _check_stmt_end(cmon_parser * _p)
{
    if (!cmon_is_valid_idx(cmon_tokens_accept(_p->tokens, cmon_tokk_semicolon)))
    {
        cmon_idx cur = cmon_tokens_current(_p->tokens);
        if (cmon_is_valid_idx(cur) && !cmon_tokens_follows_nl(_p->tokens, cur) &&
            !cmon_tokens_is(_p->tokens,
                            cur,
                            cmon_tokk_square_close,
                            cmon_tokk_paran_close,
                            cmon_tokk_curl_close,
                            cmon_tokk_eof,
                            cmon_tokk_else))
        {
            _err(_p,
                 cmon_tokens_current(_p->tokens),
                 "consecutive statements on a line must be separated by ';'");
        }
    }
}

static cmon_idx _parse_struct_decl(cmon_parser * _p)
{
    cmon_idx tmp;
    cmon_bool is_pub = _accept(_p, &tmp, cmon_tokk_pub);
    _tok_check(_p, cmon_true, cmon_tokk_struct);
    cmon_idx name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
    _tok_check(_p, cmon_true, cmon_tokk_curl_open);

    cmon_idx b = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    while (!cmon_tokens_is_current(_p->tokens, cmon_tokk_curl_close, cmon_tokk_eof))
    {
        cmon_idx field_name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
        _tok_check(_p, cmon_true, cmon_tokk_colon);
        cmon_idx type = _parse_type(_p);

        cmon_idx expr = _accept(_p, &tmp, cmon_tokk_assign) ? _parse_expr(_p, _precedence_nil)
                                                            : CMON_INVALID_IDX;

        cmon_idx_buf_append(
            _p->idx_buf_mng,
            b,
            cmon_astb_add_struct_field(_p->ast_builder, field_name_tok, type, expr));

        _check_stmt_end(_p);
    }

    _tok_check(_p, cmon_true, cmon_tokk_curl_close);

    cmon_idx ret = cmon_astb_add_struct_decl(_p->ast_builder,
                                             name_tok,
                                             is_pub,
                                             cmon_idx_buf_ptr(_p->idx_buf_mng, b),
                                             cmon_idx_buf_count(_p->idx_buf_mng, b));

    cmon_idx_buf_mng_return(_p->idx_buf_mng, b);

    return ret;
}

static cmon_idx _parse_alias(cmon_parser * _p)
{
    cmon_idx tmp;
    cmon_bool is_pub = _accept(_p, &tmp, cmon_tokk_pub);
    cmon_idx alias_tok = _tok_check(_p, cmon_true, cmon_tokk_alias);
    cmon_idx name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
    _tok_check(_p, cmon_true, cmon_tokk_assign);
    return cmon_astb_add_alias(_p->ast_builder, alias_tok, name_tok, is_pub, _parse_type(_p));
}

static cmon_idx _parse_typedef(cmon_parser * _p)
{
    cmon_idx tmp;
    cmon_bool is_pub = _accept(_p, &tmp, cmon_tokk_pub);
    cmon_idx type_tok = _tok_check(_p, cmon_true, cmon_tokk_type);
    cmon_idx name_tok = _tok_check(_p, cmon_true, cmon_tokk_ident);
    _tok_check(_p, cmon_true, cmon_tokk_assign);
    return cmon_astb_add_typedef(_p->ast_builder, type_tok, name_tok, is_pub, _parse_type(_p));
}

static cmon_idx _parse_stmt(cmon_parser * _p)
{
    cmon_idx tok, ret;
    if (_accept(_p, &tok, cmon_tokk_curl_open))
    {
        ret = _parse_block(_p, tok);
    }
    else if (_peek_fn_decl(_p, cmon_true))
    {
        ret = _parse_pretty_fn(_p);
    }
    else if (cmon_tokens_is_current(_p->tokens, cmon_tokk_alias))
    {
        ret = _parse_alias(_p);
    }
    else if (_peek_var_decl(_p))
    {
        printf("var decl\n");
        ret = _parse_var_decl(_p, cmon_false);
    }
    else
    {
        // is this an expression statement?
        ret = _parse_expr(_p, _precedence_nil);
    }
    if (cmon_is_valid_idx(ret))
    {
        _check_stmt_end(_p);
    }

    return ret;
}

static cmon_idx _parse_import(cmon_parser * _p, cmon_idx _tok)
{
    cmon_idx tok, ret, alias;
    cmon_idx b, path_tok_buf;

    b = cmon_idx_buf_mng_get(_p->idx_buf_mng);
    path_tok_buf = cmon_idx_buf_mng_get(_p->idx_buf_mng);

    do
    {
        cmon_idx_buf_clear(_p->idx_buf_mng, path_tok_buf);
        alias = CMON_INVALID_IDX;

        while (_accept(_p, &tok, cmon_tokk_ident))
        {
            cmon_str_view sv = cmon_tokens_str_view(_p->tokens, tok);
            printf("APPENDING DA TOK %lu: %*.s\n", tok, sv.end - sv.begin, sv.begin);
            assert(cmon_tokens_kind(_p->tokens, tok) == cmon_tokk_ident);

            cmon_idx_buf_append(_p->idx_buf_mng, path_tok_buf, tok);

            if (!_accept(_p, &tok, cmon_tokk_dot))
                break;
        }

        if (_accept(_p, &tok, cmon_tokk_as))
        {
            alias = _tok_check(_p, cmon_true, cmon_tokk_ident);
        }
        assert(cmon_idx_buf_count(_p->idx_buf_mng, path_tok_buf));

        cmon_idx_buf_append(
            _p->idx_buf_mng,
            b,
            cmon_astb_add_import_pair(_p->ast_builder,
                                      cmon_idx_buf_ptr(_p->idx_buf_mng, path_tok_buf),
                                      cmon_idx_buf_count(_p->idx_buf_mng, path_tok_buf),
                                      alias));
    } while (_accept(_p, &tok, cmon_tokk_comma));

    ret = cmon_astb_add_import(_p->ast_builder,
                               _tok,
                               cmon_idx_buf_ptr(_p->idx_buf_mng, b),
                               cmon_idx_buf_count(_p->idx_buf_mng, b));
    cmon_idx_buf_mng_return(_p->idx_buf_mng, path_tok_buf);
    cmon_idx_buf_mng_return(_p->idx_buf_mng, b);

    return ret;
}

static cmon_idx _parse_top_lvl_stmt(cmon_parser * _p)
{
    cmon_idx tok, ret;
    if (_accept(_p, &tok, cmon_tokk_module))
    {
        ret =
            cmon_astb_add_module(_p->ast_builder, tok, _tok_check(_p, cmon_true, cmon_tokk_ident));
    }
    else if (_accept(_p, &tok, cmon_tokk_import))
    {
        ret = _parse_import(_p, tok);
    }
    else if (cmon_tokens_is_current(_p->tokens, cmon_tokk_alias) ||
             (cmon_tokens_is_current(_p->tokens, cmon_tokk_pub) &&
              cmon_tokens_is_next(_p->tokens, cmon_tokk_alias)))
    {
        ret = _parse_alias(_p);
    }
    else if (cmon_tokens_is_current(_p->tokens, cmon_tokk_struct) ||
             (cmon_tokens_is_current(_p->tokens, cmon_tokk_pub) &&
              cmon_tokens_is_next(_p->tokens, cmon_tokk_struct)))
    {
        ret = _parse_struct_decl(_p);
    }
    else if (cmon_tokens_is_current(_p->tokens, cmon_tokk_type) ||
             (cmon_tokens_is_current(_p->tokens, cmon_tokk_pub) &&
              cmon_tokens_is_next(_p->tokens, cmon_tokk_type)))
    {
        ret = _parse_typedef(_p);
    }
    else if (_peek_var_decl(_p))
    {
        ret = _parse_var_decl(_p, cmon_true);
    }
    else if (_peek_fn_decl(_p, cmon_true))
    {
        ret = _parse_pretty_fn(_p);
    }
    else
    {
        _err(_p, cmon_tokens_current(_p->tokens), "top level statement expected");
        return CMON_INVALID_IDX;
    }

    _check_stmt_end(_p);
    return ret;
}

cmon_parser * cmon_parser_create(cmon_allocator * _alloc)
{
    cmon_parser * ret = CMON_CREATE(_alloc, cmon_parser);
    ret->alloc = _alloc;
    ret->ast_builder = NULL;
    ret->tk_str_builder = cmon_str_builder_create(_alloc, 64);
    ret->idx_buf_mng = cmon_idx_buf_mng_create(_alloc);
    ret->err_handler = cmon_err_handler_create(_alloc, NULL, 1);
    return ret;
}

void cmon_parser_destroy(cmon_parser * _p)
{
    if (!_p)
        return;

    cmon_err_handler_destroy(_p->err_handler);
    cmon_idx_buf_mng_destroy(_p->idx_buf_mng);
    cmon_str_builder_destroy(_p->tk_str_builder);
    cmon_astb_destroy(_p->ast_builder);
    CMON_DESTROY(_p->alloc, _p);
}

cmon_err_report cmon_parser_err(cmon_parser * _p)
{
    return cmon_err_handler_count(_p->err_handler)
               ? *cmon_err_handler_err_report(_p->err_handler, 0)
               : cmon_err_report_make_empty();
}

cmon_ast * cmon_parser_parse(cmon_parser * _p,
                             cmon_src * _src,
                             cmon_idx _src_file_idx,
                             cmon_tokens * _tokens)
{
    cmon_idx stmt_idx, root_block_idx, first_tok;
    cmon_ast * ast;
    cmon_idx b;

    ast = NULL;

    // for now a parser can't be reset, this is just a sanity check to make sure cmon_parser_parse
    // is only called once for every instance
    assert(_p->ast_builder == NULL);

    _p->src = _src;
    _p->src_file_idx = _src_file_idx;
    _p->tokens = _tokens;
    _p->ast_builder = cmon_astb_create(_p->alloc, _tokens);

    first_tok = cmon_tokens_count(_p->tokens) ? 0 : CMON_INVALID_IDX;

    cmon_src_set_tokens(_src, _src_file_idx, _tokens);
    cmon_err_handler_set_src(_p->err_handler, _src);

    if (setjmp(_p->err_jmp))
    {
        goto end;
    }
    cmon_err_handler_set_jump(_p->err_handler, &_p->err_jmp);

    b = cmon_idx_buf_mng_get(_p->idx_buf_mng);

    while (!cmon_tokens_is_current(_p->tokens, cmon_tokk_eof))
    {
        stmt_idx = _parse_top_lvl_stmt(_p);
        if (cmon_is_valid_idx(stmt_idx))
        {
            cmon_idx_buf_append(_p->idx_buf_mng, b, stmt_idx);
        }
    }

    root_block_idx = cmon_astb_add_block(_p->ast_builder,
                                         first_tok,
                                         cmon_idx_buf_ptr(_p->idx_buf_mng, b),
                                         cmon_idx_buf_count(_p->idx_buf_mng, b));
    cmon_astb_set_root_block(_p->ast_builder, root_block_idx);

    ast = cmon_astb_ast(_p->ast_builder);
    cmon_src_set_ast(_src, _src_file_idx, ast);

end:
    cmon_idx_buf_mng_return(_p->idx_buf_mng, b);
    return ast;
}
