#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_str_builder.h>
#include <setjmp.h>
#include <stdarg.h>

// for now the parser stops on the first error. we simpy save the error and jump out of parsing.
#define _err(_p, _tok, _msg, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(&_p->err_str_builder);                                              \
        cmon_str_builder_append_fmt(&_p->err_str_builder, _msg, ##__VA_ARGS__);                    \
        _p->err = cmon_err_report_make(cmon_src_filename(_p->src, _p->src_file_idx),               \
                                       cmon_tokens_line(_p->tokens, _tok),                         \
                                       cmon_tokens_line_offset(_p->tokens, _tok),                  \
                                       cmon_str_builder_c_str(&_p->err_str_builder));              \
        longjmp(_p->err_jmp, 1);                                                                   \
    } while (0)

// typedef struct cmon_parser
// {
//     //@TODO: move everything implementation specific into a pimpl
//     cmon_allocator * alloc;
//     cmon_src_file * file;
//     cmon_tokenizer * tokenizer;
//     cmon_ast_file * ast_file;
//     // the top level statements of the parsed file
//     cmon_dyn_arr(cmon_ast_stmt *) root_stmts;
//     // set by the top module statement (if any)
//     cmon_ast_stmt * mod_stmt;
//     cmon_dyn_arr(cmon_ast_stmt *) imports;
//     // while only top level functions are supported at this point, once we want to have anonymous
//     // functions, this will come in handy. Right now we could just use a ptr to the current
//     // function.
//     cmon_dyn_arr(cmon_ast_expr_fn_decl *) fn_stack;
//     cmon_dyn_arr(cmon_ast_stmt*) for_stack;
//     // these arrays mainly exist for easy cleanup
//     cmon_dyn_arr(cmon_parsed_type *) parsed_types;
//     cmon_dyn_arr(cmon_ast_expr *) exprs;
//     cmon_dyn_arr(cmon_ast_stmt *) stmts;
//     // helper to build tmp strings
//     cmon_str_builder tmp_builder;
//     cmon_err_report err;
//     jmp_buf err_jmp;
// } cmon_parser;

typedef struct
{
    cmon_dyn_arr(cmon_idx) buf;
    size_t next;
} _idx_buf;

typedef struct
{
    cmon_dyn_arr(_idx_buf *) bufs;
    cmon_dyn_arr(_idx_buf *) free_bufs;
} _idx_buf_mng;

static inline _idx_buf * _idx_buf_create(_idx_buf_mng * _mng)
{
    _idx_buf * ret;
    cmon_allocator * a;

    a = _cmon_dyn_arr_md(&_mng->bufs)->alloc;
    ret = CMON_CREATE(a, _idx_buf);
    cmon_dyn_arr_init(&ret->buf, a, 16);
    cmon_dyn_arr_append(&_mng->bufs, ret);
    return ret;
}

static inline void _idx_buf_destroy(_idx_buf * _buf)
{
    cmon_allocator * a = _cmon_dyn_arr_md(&_buf->buf)->alloc;
    cmon_dyn_arr_dealloc(&_buf->buf);
    CMON_DESTROY(a, _buf);
}

static inline _idx_buf_mng * _idx_buf_mng_create(cmon_allocator * _alloc)
{
    _idx_buf_mng * ret = CMON_CREATE(_alloc, _idx_buf_mng);
    cmon_dyn_arr_init(&ret->bufs, _alloc, 8);
    cmon_dyn_arr_init(&ret->free_bufs, _alloc, 8);
    return ret;
}

static inline void _idx_buf_mng_destroy(_idx_buf_mng * _mng)
{
    size_t i;
    cmon_allocator * a;

    a = _cmon_dyn_arr_md(&_mng->bufs)->alloc;

    cmon_dyn_arr_dealloc(&_mng->free_bufs);
    for (i = 0; i < cmon_dyn_arr_count(&_mng->bufs); ++i)
    {
        _idx_buf_destroy(_mng->bufs[i]);
    }

    cmon_dyn_arr_dealloc(&_mng->bufs);
}

static inline _idx_buf * _idx_buf_mng_get(_idx_buf_mng * _mng)
{
    _idx_buf * ret;
    if (!cmon_dyn_arr_count(&_mng->free_bufs))
        return _idx_buf_create(_mng);
    ret = cmon_dyn_arr_pop(&_mng->free_bufs);
    cmon_dyn_arr_clear(&ret->buf);
    return ret;
}

static inline void _idx_buf_mng_return(_idx_buf_mng * _mng, _idx_buf * _buf)
{
    cmon_dyn_arr_append(&_mng->free_bufs, _buf);
}

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

static _precedence _tok_prec(cmon_token_kind _kind)
{
    switch (_kind)
    {
    case cmon_tk_or:
        return _precedence_or;
    case cmon_tk_and:
        return _precedence_and;
    case cmon_tk_less:
    case cmon_tk_less_equal:
    case cmon_tk_greater:
    case cmon_tk_greater_equal:
    case cmon_tk_equals:
    case cmon_tk_not_equals:
        return _precedence_comp;
    case cmon_tk_assign:
    case cmon_tk_plus_assign:
    case cmon_tk_minus_assign:
    case cmon_tk_mult_assign:
    case cmon_tk_div_assign:
    case cmon_tk_mod_assign:
    case cmon_tk_bw_and_assign:
    case cmon_tk_bw_or_assign:
    case cmon_tk_bw_xor_assign:
    case cmon_tk_bw_left_assign:
    case cmon_tk_bw_right_assign:
        return _precedence_assign;
    case cmon_tk_plus:
    case cmon_tk_minus:
        return _precedence_sum;
    case cmon_tk_mult:
    case cmon_tk_div:
    case cmon_tk_mod:
        return _precedence_product;
    case cmon_tk_as:
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
    cmon_str_builder * err_str_builder;
    cmon_str_builder * tk_str_builder;
    _idx_buf_mng * idx_buf_mng;
    cmon_tokens * tokens;
    cmon_err_report err;
    jmp_buf err_jmp;
} cmon_parser;

const char * _token_kinds_to_str(cmon_str_builder * _b, va_list _args)
{
    cmon_token_kind kind;

    cmon_str_builder_clear(_b);
    do
    {
        kind = va_arg(_args, cmon_token_kind);
        if (kind != -1)
        {
            if (cmon_str_builder_count(_b))
                cmon_str_builder_append(_b, ", ");
            cmon_str_builder_append(_b, cmon_token_kind_to_str(kind));
        }
    } while (kind != -1);
    return cmon_str_builder_c_str(_b);
}

static inline cmon_idx _token_check_impl(cmon_parser * _p, cmon_bool _allow_line_break, ...)
{
    va_list args;
    va_start(args, _allow_line_break);
    cmon_idx tok = cmon_tokens_accept_impl_v(_p->tokens, args);
    if (!cmon_is_valid_idx(tok))
    {
        const char * tk_kinds_str = _token_kinds_to_str(_p->tk_str_builder, args);
        va_end(args);

        cmon_idx cur = cmon_tokens_current(_p->tokens);
        if (cmon_tokens_kind(_p->tokens, cur) != cmon_tk_eof)
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
    _token_check_impl(_p, _allow_line_break, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))

#define _accept(_p, _out_tok, ...)                                                                 \
    (cmon_is_valid_idx(*(_out_tok) = cmon_tokens_accept(_p->tokens, __VA_ARGS__)))

static cmon_idx _parse_expr(cmon_parser * _p, _precedence _prec);
static cmon_idx _parse_block(cmon_parser * _p, cmon_idx _open_tok);
static cmon_idx _parse_stmt(cmon_parser * _p);

static cmon_idx _parse_type(cmon_parser * _p)
{
    cmon_idx ret, tok, tmp;
    cmon_bool is_mut;

    if (_accept(_p, &tok, cmon_tk_mult))
    {
        is_mut = _accept(_p, &tmp, cmon_tk_mut);
        return cmon_astb_add_type_ptr(_p->ast_builder, tok, is_mut, _parse_type(_p));
    }
    else if (_accept(_p, &tok, cmon_tk_ident))
    {
        return cmon_astb_add_type_named(_p->ast_builder, tok);
    }

    //@TODO: error, unexpected token

    return CMON_INVALID_IDX;
}

static cmon_idx _parse_call_expr(cmon_parser * _p, cmon_idx _tok, cmon_idx _lhs)
{
    cmon_idx ret, tmp;
    _idx_buf * b;

    b = _idx_buf_mng_get(_p->idx_buf_mng);
    while (!cmon_tokens_is_current(_p->tokens, cmon_tk_paran_close, cmon_tk_eof))
    {
        cmon_dyn_arr_append(&b->buf, _parse_expr(_p, _precedence_nil));
        if (_accept(_p, &tmp, cmon_tk_comma))
        {
            if (cmon_tokens_is_current(_p->tokens, cmon_tk_paran_close))
                _err(_p, tmp, "unexpected comma");
        }
        else
            break;
    }
    _tok_check(_p, cmon_true, cmon_tk_paran_close);
    ret = cmon_astb_add_call(_p->ast_builder, _tok, _lhs, b->buf, cmon_dyn_arr_count(&b->buf));
    _idx_buf_mng_return(_p->idx_buf_mng, b);
    return ret;
}

static cmon_idx _parse_fn(cmon_parser * _p, cmon_idx _fn_tok_idx)
{
    cmon_idx tok, type, ret, ret_type, body;
    size_t i;
    _idx_buf * param_buf;
    _idx_buf * name_tok_buf;
    cmon_bool is_mut;

    param_buf = _idx_buf_mng_get(_p->idx_buf_mng);
    name_tok_buf = _idx_buf_mng_get(_p->idx_buf_mng);
    _tok_check(_p, cmon_true, cmon_tk_paran_open);

    while (!cmon_tokens_is_current(_p->tokens, cmon_tk_paran_close, cmon_tk_eof))
    {
        is_mut = _accept(_p, &tok, cmon_tk_mut);

        cmon_dyn_arr_clear(&name_tok_buf->buf);
        do
        {
            cmon_dyn_arr_append(&name_tok_buf->buf, _tok_check(_p, cmon_true, cmon_tk_ident));
        } while (_accept(_p, &tok, cmon_tk_comma));

        _tok_check(_p, cmon_true, cmon_tk_colon);

        type = _parse_type(_p);

        for (i = 0; i < cmon_dyn_arr_count(&name_tok_buf->buf); ++i)
        {
            cmon_dyn_arr_append(
                &param_buf->buf,
                cmon_astb_add_fn_param(_p->ast_builder, name_tok_buf->buf[i], is_mut, type));
        }

        if (_accept(_p, &tok, cmon_tk_comma))
        {
            if (cmon_tokens_is_current(_p->tokens, cmon_tk_paran_close))
                _err(_p, tok, "unexpected comma");
        }
        else
            break;
    }

    _tok_check(_p, cmon_true, cmon_tk_paran_close);
    _tok_check(_p, cmon_true, cmon_tk_arrow);

    ret_type = _parse_type(_p);
    body = _parse_block(_p, _tok_check(_p, cmon_true, cmon_tk_curl_open));

    ret = cmon_astb_add_fn_decl(_p->ast_builder,
                                _fn_tok_idx,
                                ret_type,
                                param_buf->buf,
                                cmon_dyn_arr_count(&param_buf->buf),
                                body);

    _idx_buf_mng_return(_p->idx_buf_mng, name_tok_buf);
    _idx_buf_mng_return(_p->idx_buf_mng, param_buf);

    return ret;
}

static cmon_idx _parse_expr(cmon_parser * _p, _precedence _prec)
{
    cmon_idx tok, ret;

    if (_accept(_p, &tok, cmon_tk_ident))
    {
        ret = cmon_astb_add_ident(_p->ast_builder, tok);
    }
    else if(_accept(_p, &tok, cmon_tk_fn))
    {
        ret = _parse_fn(_p, cmon_tk_fn);
    }
    else if (_accept(_p, &tok, cmon_tk_minus, cmon_tk_exclam))
    {
        ret = cmon_astb_add_prefix(_p->ast_builder, tok, _parse_expr(_p, _precedence_prefix));
    }

    while (cmon_tokens_is_current(_p->tokens, cmon_tk_paran_open))
    {
        if (_accept(_p, &tok, cmon_tk_paran_open))
        {
            ret = _parse_call_expr(_p, tok, ret);
        }
    }

    while (cmon_tokens_is_current(_p->tokens, CMON_BIN_TOKS, cmon_tk_as))
    {
        tok = cmon_tokens_current(_p->tokens);
        if (_tok_prec(cmon_tokens_kind(_p->tokens, tok)) < _prec)
            break;

        if (cmon_tokens_is(_p->tokens, tok, CMON_BIN_TOKS))
        {
            ret =
                cmon_astb_add_binary(_p->ast_builder,
                                     tok,
                                     ret,
                                     _parse_expr(_p, _tok_prec(cmon_tokens_kind(_p->tokens, tok))));
        }
        else
        {
            assert(0);
        }
    }

    return ret;
}

static cmon_idx _parse_block(cmon_parser * _p, cmon_idx _open_tok)
{
    // cmon_idx tok;
    // if (cmon_is_valid_idx(tok = _tok_check(_p, cmon_true, cmon_tk_curl_open)))
    // {
    cmon_idx ret, stmt;
    _idx_buf * b;

    b = _idx_buf_mng_get(_p->idx_buf_mng);
    while (!cmon_tokens_is_current(_p->tokens, cmon_tk_curl_close, cmon_tk_eof))
    {
        if (cmon_is_valid_idx(stmt = _parse_stmt(_p)))
        {
            cmon_dyn_arr_append(&b->buf, stmt);
        }
    }
    _tok_check(_p, cmon_true, cmon_tk_curl_close);

    ret = cmon_astb_add_block(_p->ast_builder, _open_tok, b->buf, cmon_dyn_arr_count(&b->buf));
    _idx_buf_mng_return(_p->idx_buf_mng, b);
    return ret;
    // }

    // return CMON_INVALID_IDX;
}

static inline cmon_bool _peek_var_decl(cmon_parser * _p)
{
    return cmon_tokens_is_current(_p->tokens, cmon_tk_mut) ||
           ((cmon_tokens_is_current(_p->tokens, cmon_tk_ident) ||
             (cmon_tokens_is_current(_p->tokens, cmon_tk_pub) &&
              cmon_tokens_is_next(_p->tokens, cmon_tk_ident))) &&
            cmon_tokens_is_next(_p->tokens, cmon_tk_colon));
}

static cmon_idx _parse_var_decl(cmon_parser * _p, cmon_bool _top_lvl)
{
    cmon_idx tok, tmp, type;
    cmon_bool is_pub, is_mut;

    is_pub = _top_lvl && _accept(_p, &tok, cmon_tk_pub);
    is_mut = _accept(_p, &tok, cmon_tk_mut);
    tok = _tok_check(_p, cmon_true, cmon_tk_ident);
    _tok_check(_p, cmon_false, cmon_tk_colon);
    if (_accept(_p, &tmp, cmon_tk_assign))
        type = CMON_INVALID_IDX;
    else
        type = _parse_type(_p);

    return cmon_astb_add_var_decl(
        _p->ast_builder, tok, is_pub, is_mut, type, _parse_expr(_p, _precedence_nil));
}

static cmon_idx _parse_stmt(cmon_parser * _p)
{
    cmon_idx tok;
    if (_accept(_p, &tok, cmon_tk_curl_open))
    {
        return _parse_block(_p, tok);
    }
    else if (_peek_var_decl(_p))
    {
        return _parse_var_decl(_p, cmon_false);
    }
    // is this an expression statement?
    return _parse_expr(_p, _precedence_nil);
}

static cmon_idx _parse_top_lvl_stmt(cmon_parser * _p)
{
}

cmon_parser * cmon_parser_create(cmon_allocator * _alloc)
{
    cmon_parser * ret = CMON_CREATE(_alloc, cmon_parser);
    ret->alloc = _alloc;
    ret->ast_builder = cmon_astb_create(_alloc);
    ret->err_str_builder = cmon_str_builder_create(_alloc, 256);
    ret->tk_str_builder = cmon_str_builder_create(_alloc, 64);
    ret->idx_buf_mng = _idx_buf_mng_create(_alloc);
    ret->err = cmon_err_report_make_empty();
    return ret;
}

void cmon_parser_destroy(cmon_parser * _p)
{
    if (!_p)
        return;

    _idx_buf_mng_destroy(_p->idx_buf_mng);
    cmon_str_builder_destroy(_p->tk_str_builder);
    cmon_str_builder_destroy(_p->err_str_builder);
    cmon_astb_destroy(_p->ast_builder);
}

cmon_err_report cmon_parser_err(cmon_parser * _p)
{
    return _p->err;
}

cmon_ast * cmon_parser_parse(cmon_parser * _p,
                             cmon_src * _src,
                             cmon_idx _src_file_idx,
                             cmon_tokens * _tokens)
{
}
