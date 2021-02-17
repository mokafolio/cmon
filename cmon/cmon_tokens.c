#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tokens.h>
#include <stdarg.h>

// for now the lexer stops on the first error. we simpy save the error.
#define _err(_l, _msg, ...)                                                                        \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(_l->tmp_str_b);                                                     \
        cmon_str_builder_append_fmt(_l->tmp_str_b, _msg, ##__VA_ARGS__);                           \
        _l->err = cmon_err_report_make(cmon_src_filename(_l->src, _l->src_file_idx),               \
                                       _l->current_line,                                           \
                                       _l->current_line_off,                                       \
                                       cmon_str_builder_c_str(_l->tmp_str_b));                     \
    } while (0)

typedef struct
{
    cmon_idx line;
    cmon_idx line_end;
    cmon_idx line_off;
    cmon_str_view str_view;
    // true if the token is preceeded by a new line
    cmon_bool follows_nl;
} _token;

typedef struct cmon_tokens
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_token_kind) kinds;
    cmon_dyn_arr(_token) tokens;
    cmon_idx tok_idx;
} cmon_tokens;

typedef struct
{
    cmon_allocator * alloc;
    cmon_src * src;
    cmon_idx src_file_idx;
    const char * input;
    const char * pos;
    const char * end;
    cmon_idx current_line;
    cmon_idx current_line_off;
    cmon_err_report err;
    cmon_str_builder * tmp_str_b;
} _tokenize_session;

static inline void _advance_pos(_tokenize_session * _l, size_t _advance)
{
    _l->pos += _advance;
    _l->current_line_off += _advance;
}

static inline void _advance_line(_tokenize_session * _l)
{
    ++_l->current_line;
    _l->current_line_off = 0;
}

static inline size_t _skip_whitespace(_tokenize_session * _l)
{
    size_t count = 0;
    while (isspace(*_l->pos))
    {
        if (*_l->pos == '\n')
        {
            _advance_line(_l);
            ++count;
        }
        _advance_pos(_l, 1);
    }
    return count;
}

static inline cmon_bool _name_equals(const char * _begin, int _len, const char * _keyword)
{
    return strncmp(_begin, _keyword, CMON_MAX(_len, strlen(_keyword))) == 0;
}

static cmon_token_kind _tok_kind_for_name(const char * _begin, const char * _end)
{
    int len = _end - _begin;

    if (_name_equals(_begin, len, "if"))
        return cmon_tk_if;
    else if (_name_equals(_begin, len, "else"))
        return cmon_tk_else;
    else if (_name_equals(_begin, len, "fn"))
        return cmon_tk_fn;
    else if (_name_equals(_begin, len, "return"))
        return cmon_tk_return;
    else if (_name_equals(_begin, len, "break"))
        return cmon_tk_break;
    else if (_name_equals(_begin, len, "and"))
        return cmon_tk_and;
    else if (_name_equals(_begin, len, "or"))
        return cmon_tk_or;
    else if (_name_equals(_begin, len, "struct"))
        return cmon_tk_struct;
    else if (_name_equals(_begin, len, "true"))
        return cmon_tk_true;
    else if (_name_equals(_begin, len, "false"))
        return cmon_tk_false;
    else if (_name_equals(_begin, len, "mut"))
        return cmon_tk_mut;
    else if (_name_equals(_begin, len, "pub"))
        return cmon_tk_pub;
    else if (_name_equals(_begin, len, "as"))
        return cmon_tk_as;
    else if (_name_equals(_begin, len, "in"))
        return cmon_tk_in;
    else if (_name_equals(_begin, len, "for"))
        return cmon_tk_for;
    else if (_name_equals(_begin, len, "module"))
        return cmon_tk_module;
    else if (_name_equals(_begin, len, "import"))
        return cmon_tk_import;
    else if (_name_equals(_begin, len, "none"))
        return cmon_tk_none;
    else if (_name_equals(_begin, len, "embed"))
        return cmon_tk_embed;
    else if (_name_equals(_begin, len, "interface"))
        return cmon_tk_interface;
    else if (_name_equals(_begin, len, "self"))
        return cmon_tk_self;
    else if (_name_equals(_begin, len, "type"))
        return cmon_tk_type;
    else if (_name_equals(_begin, len, "alias"))
        return cmon_tk_alias;
    else if (_name_equals(_begin, len, "defer"))
        return cmon_tk_defer;
    else if (_name_equals(_begin, len, "try"))
        return cmon_tk_try;
    else if (_name_equals(_begin, len, "continue"))
        return cmon_tk_continue;
    else if (_name_equals(_begin, len, "scoped"))
        return cmon_tk_scoped;
    else if (_name_equals(_begin, len, "scope_exit"))
        return cmon_tk_scope_exit;

    return cmon_tk_ident;
}

static cmon_bool _peek_float_lit(_tokenize_session * _l)
{
    const char * pos = _l->pos + 1;

    if (isspace(*pos))
        return cmon_false;

    while (pos != _l->end && !isspace(*pos))
    {
        if (!isdigit(*pos) && *pos != '-' && *pos != '+' && *pos != 'e')
            return cmon_false;
        ++pos;
    }
    return cmon_true;
}

static inline cmon_bool _next_char_is(_tokenize_session * _l, char _c)
{
    return _l->pos < _l->end && *(_l->pos + 1) == _c;
}

static inline void _consume_digits(_tokenize_session * _l)
{
    while (isdigit(*_l->pos))
        _advance_pos(_l, 1);
}

static inline void _parse_float_or_int_literal(_tokenize_session * _l, cmon_bool * _is_float)
{
    cmon_bool is_float = cmon_false;
    cmon_bool is_hex = cmon_false;
    printf("start %s!\n", _l->pos);
    if (isdigit(*_l->pos))
    {
        printf("consume\n");
        // skip hex portion
        if (*_l->pos == '0' && (_next_char_is(_l, 'x') || _next_char_is(_l, 'X')))
        {
            is_hex = cmon_true;
            _advance_pos(_l, 2);
            while (isxdigit(*_l->pos))
                _advance_pos(_l, 1);
        }
        else
        {
            // parse remaining digits
            _consume_digits(_l);
        }
    }

    // floating point hex literals are not supported for now
    // @NOTE: the second check bails early if there is a double dot (range expression operator)
    if (!is_hex && !(*_l->pos == '.' && _next_char_is(_l, '.')))
    {
        cmon_bool has_dec_sep = cmon_false;
        // parse decimal part
        if (*_l->pos == '.')
        {
            has_dec_sep = cmon_true;
            is_float = cmon_true;
            _advance_pos(_l, 1);

            _consume_digits(_l);
        }

        // potentially parse exponent
        if (*_l->pos == 'e' || *_l->pos == 'E')
        {
            is_float = cmon_true;
            if (_next_char_is(_l, '-') || _next_char_is(_l, '+'))
            {
                _advance_pos(_l, 2);
            }
            else
                _advance_pos(_l, 1);

            if (isdigit(*_l->pos))
                _consume_digits(_l);
            else if (has_dec_sep)
            {
                _err(_l, "exponent has no digits");
            }
        }
        // assert(cmon_false);
    }

    // @TODO: if the literal does not end on a whitespace or semicolon, something obiously went
    // wrong. error right then and there?

    *_is_float = is_float;
}

static inline cmon_bool _finalize_tok(_tokenize_session * _l,
                                      cmon_token_kind _kind,
                                      size_t _advance,
                                      cmon_token_kind * _out_kind,
                                      _token * _out_tok)
{
    _advance_pos(_l, _advance);
    _out_tok->str_view.end = _l->pos;
    *_out_kind = _kind;
    return cmon_true;
}

static cmon_bool _next_token(_tokenize_session * _l, cmon_token_kind * _out_kind, _token * _out_tok)
{
    if (_l->pos >= _l->end)
        return cmon_false;

    _out_tok->follows_nl = _skip_whitespace(_l) > 0;

    if (_l->pos >= _l->end)
        return cmon_false;

    _out_tok->str_view.begin = _l->pos;
    _out_tok->line = _l->current_line;
    _out_tok->line_off = _l->current_line_off;

    if (*_l->pos == '{')
        return _finalize_tok(_l, cmon_tk_curl_open, 1, _out_kind, _out_tok);
    else if (*_l->pos == '}')
        return _finalize_tok(_l, cmon_tk_curl_close, 1, _out_kind, _out_tok);
    else if (*_l->pos == '(')
        return _finalize_tok(_l, cmon_tk_paran_open, 1, _out_kind, _out_tok);
    else if (*_l->pos == ')')
        return _finalize_tok(_l, cmon_tk_paran_close, 1, _out_kind, _out_tok);
    else if (*_l->pos == '&')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_bw_and_assign, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tk_bw_and, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '|')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_bw_or_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tk_bw_or, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '^')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_bw_xor_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tk_bw_xor, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == ';')
    {
        return _finalize_tok(_l, cmon_tk_semicolon, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '~')
        return _finalize_tok(_l, cmon_tk_bw_not, 1, _out_kind, _out_tok);
    else if (*_l->pos == '?')
        return _finalize_tok(_l, cmon_tk_question, 1, _out_kind, _out_tok);
    else if (*_l->pos == '$')
        return _finalize_tok(_l, cmon_tk_dollar, 1, _out_kind, _out_tok);
    else if (*_l->pos == ':')
    {
        return _finalize_tok(_l, cmon_tk_colon, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '=')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_equals, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tk_assign, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '<')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_less_equal, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '<'))
            return _finalize_tok(_l, cmon_tk_bw_left, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tk_less, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '>')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_greater_equal, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '>'))
            return _finalize_tok(_l, cmon_tk_bw_right, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tk_greater, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '+')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_plus_assign, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '+'))
            return _finalize_tok(_l, cmon_tk_inc, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tk_plus, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '-')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_minus_assign, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '-'))
        {
            _advance_pos(_l, 1);
            if (_next_char_is(_l, '-'))
            {
                // noinit
                _finalize_tok(_l, cmon_tk_noinit, 2, _out_kind, _out_tok);
            }
            return _finalize_tok(_l, cmon_tk_dec, 1, _out_kind, _out_tok);
        }
        return _finalize_tok(_l, cmon_tk_minus, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '*')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_mult_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tk_mult, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '%')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tk_mod_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tk_mod, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == ',')
        return _finalize_tok(_l, cmon_tk_comma, 1, _out_kind, _out_tok);
    else if (*_l->pos == '!')
    {
        if (_next_char_is(_l, '='))
        {
            return _finalize_tok(_l, cmon_tk_not_equals, 2, _out_kind, _out_tok);
        }
        return _finalize_tok(_l, cmon_tk_exclam, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '[')
        return _finalize_tok(_l, cmon_tk_square_open, 1, _out_kind, _out_tok);
    else if (*_l->pos == ']')
        return _finalize_tok(_l, cmon_tk_square_close, 1, _out_kind, _out_tok);
    else if (*_l->pos == '@')
        return _finalize_tok(_l, cmon_tk_at, 1, _out_kind, _out_tok);
    else if (*_l->pos == '.' && !_peek_float_lit(_l))
    {
        if (_next_char_is(_l, '.'))
        {
            return _finalize_tok(_l, cmon_tk_double_dot, 2, _out_kind, _out_tok);
        }
        return _finalize_tok(_l, cmon_tk_dot, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '"' || *_l->pos == '\'')
    {
        char marks = *_l->pos;
        _advance_pos(_l, 1);
        while (*_l->pos != marks && _l->pos != _l->end)
        {
            // if (*_l->pos == '\n')
            //     _advance_line(_l);
            _advance_pos(_l, 1);
        }
        *_out_kind == cmon_tk_string;
        _out_tok->str_view.end = _l->pos;
        _advance_pos(_l, 1); // skip closing "
        return cmon_true;
    }
    else if (*_l->pos == '/')
    {
        // single line comment
        if (_next_char_is(_l, '/'))
        {
            while (*_l->pos != '\n' && _l->pos != _l->end)
            {
                _advance_pos(_l, 1);
            }
            *_out_kind = cmon_tk_comment;
            _out_tok->str_view.end = _l->pos;
            return cmon_true;
        }
        // multiline comment
        else if (_next_char_is(_l, '*'))
        {
            size_t openings_to_match = 1;
            _advance_pos(_l, 2);
            while (openings_to_match > 0 && _l->pos != _l->end)
            {
                if (*_l->pos == '*' && _next_char_is(_l, '/'))
                {
                    --openings_to_match;
                    _advance_pos(_l, 2);
                }
                else if (*_l->pos == '/' && _next_char_is(_l, '*'))
                {
                    ++openings_to_match;
                    _advance_pos(_l, 2);
                }
                else
                {
                    if (*_l->pos == '\n')
                        _advance_line(_l);
                    _advance_pos(_l, 1);
                }
            }

            if (openings_to_match)
                _err(_l, "unclosed multiline comment");

            *_out_kind = cmon_tk_comment;
            _out_tok->str_view.end = _l->pos;
            _out_tok->line_end = _l->current_line;

            return cmon_true;
        }
        else if (_next_char_is(_l, '='))
        {
            return _finalize_tok(_l, cmon_tk_div_assign, 2, _out_kind, _out_tok);
        }
        else
        {
            return _finalize_tok(_l, cmon_tk_div, 1, _out_kind, _out_tok);
        }
    }
    else
    {
        // this is not any of the explicitly defined Tokens.
        // it has to be a identifier, number or keyword
        const char * startp = _l->pos;
        if (isalpha(*_l->pos) || *_l->pos == '_')
        {
            printf("ident\n");
            while (isalnum(*_l->pos) || *_l->pos == '_')
                _advance_pos(_l, 1);

            // this will set it to either cmon_tok_type_ident or the correct keyword token type
            *_out_kind = _tok_kind_for_name(startp, _l->pos);
        }
        // integer or float
        //@TODO: Make this work for hex, octal, scientific float notation etc.
        else if (isdigit(*_l->pos) || *_l->pos == '.')
        {
            cmon_bool is_float;
            _parse_float_or_int_literal(_l, &is_float);
            if (is_float)
                *_out_kind = cmon_tk_float;
            else
                *_out_kind = cmon_tk_int;

            printf("DA LIT %.*s\n\n", _l->pos - startp, startp);
        }
        else
        {
            //@TODO: better error
            _err(_l, "invalid character '%c' in source code", *_l->pos);
        }

        _out_tok->str_view.end = _l->pos;
        return cmon_true;
    }
    return cmon_false;
}

static inline cmon_tokens * _tokens_create(cmon_allocator * _alloc)
{
    cmon_tokens * ret = CMON_CREATE(_alloc, cmon_tokens);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->kinds, _alloc, 128);
    cmon_dyn_arr_init(&ret->tokens, _alloc, 128);
    ret->tok_idx = CMON_INVALID_IDX;
    return ret;
}

static inline void _tokenize_session_init(_tokenize_session * _s,
                                          cmon_allocator * _alloc,
                                          cmon_src * _src,
                                          cmon_idx _src_file_idx)
{
    const char * code;
    code = cmon_src_code(_src, _src_file_idx);

    printf("session init %s\n", cmon_src_filename(_src, _src_file_idx));
    _s->alloc = _alloc;
    _s->src = _src;
    _s->src_file_idx = _src_file_idx;
    _s->input = code;
    _s->pos = code;
    _s->end = code + strlen(code);
    _s->current_line = 1;
    _s->current_line_off = 1;
    _s->err = cmon_err_report_make_empty();
    //@TODO: lazy init the string builder only on error
    _s->tmp_str_b = cmon_str_builder_create(_alloc, 512);
}

static inline void _tokenize_session_dealloc(_tokenize_session * _s)
{
    cmon_str_builder_destroy(_s->tmp_str_b);
}

cmon_tokens * cmon_tokenize(cmon_allocator * _alloc,
                            cmon_src * _src,
                            cmon_idx _src_file_idx,
                            cmon_err_report * _out_err)
{
    _token tok;
    cmon_token_kind kind;
    _tokenize_session s;

    _tokenize_session_init(&s, _alloc, _src, _src_file_idx);
    cmon_tokens * ret = _tokens_create(_alloc);
    while (_next_token(&s, &kind, &tok) && cmon_err_report_is_empty(&s.err))
    {
        cmon_dyn_arr_append(&ret->kinds, kind);
        cmon_dyn_arr_append(&ret->tokens, tok);
    }

    if (!cmon_err_report_is_empty(&s.err))
    {
        if (_out_err)
        {
            *_out_err = cmon_err_report_copy(&s.err);
        }
        cmon_tokens_destroy(ret);
        ret = NULL;
    }
    else
    {
        _token tok;
        tok.str_view.begin = s.end;
        tok.str_view.end = s.end;
        cmon_dyn_arr_append(&ret->kinds, cmon_tk_eof);
        cmon_dyn_arr_append(&ret->tokens, tok);
        ret->tok_idx = 0;
    }

    _tokenize_session_dealloc(&s);
    return ret;
}

void cmon_tokens_destroy(cmon_tokens * _t)
{
    if (!_t)
        return;
    cmon_dyn_arr_dealloc(&_t->tokens);
    cmon_dyn_arr_dealloc(&_t->kinds);
    CMON_DESTROY(_t->alloc, _t);
}

size_t cmon_tokens_count(cmon_tokens * _t)
{
    return cmon_dyn_arr_count(&_t->kinds);
}

cmon_idx cmon_tokens_prev(cmon_tokens * _t, cmon_bool _skip_comments)
{
    size_t ret = _t->tok_idx;
    while (ret > 0)
    {
        --ret;
        if (!_skip_comments || _t->kinds[ret] != cmon_tk_comment)
            return ret;
    }

    return CMON_INVALID_IDX;
}

cmon_idx cmon_tokens_current(cmon_tokens * _t)
{
    return _t->tok_idx;
}

static inline cmon_idx _inline_next(cmon_tokens * _t, cmon_bool _skip_comments)
{
    size_t ret = _t->tok_idx;
    while (ret < cmon_dyn_arr_count(&_t->tokens))
    {
        ++ret;
        if (!_skip_comments || _t->kinds[ret] != cmon_tk_comment)
            return ret;
    }

    return CMON_INVALID_IDX;
}

cmon_idx cmon_tokens_next(cmon_tokens * _t, cmon_bool _skip_comments)
{
    return _inline_next(_t, _skip_comments);
}

cmon_idx cmon_tokens_advance(cmon_tokens * _t, cmon_bool _skip_comments)
{
    cmon_idx ret, idx;
    ret = _t->tok_idx;
    idx = _inline_next(_t, _skip_comments);
    _t->tok_idx = idx != CMON_INVALID_IDX ? idx : _t->tok_idx;
    return ret;
}

#define _get_token(_t, _idx) (assert(_idx < cmon_dyn_arr_count(&_t->tokens)), _t->tokens[_idx])
#define _get_kind(_t, _idx) (assert(_idx < cmon_dyn_arr_count(&_t->kinds)), _t->kinds[_idx])

cmon_token_kind cmon_tokens_kind(cmon_tokens * _t, cmon_idx _idx)
{
    return _get_kind(_t, _idx);
}

cmon_str_view cmon_tokens_str_view(cmon_tokens * _t, cmon_idx _idx)
{
    return _get_token(_t, _idx).str_view;
}

cmon_idx cmon_tokens_line(cmon_tokens * _t, cmon_idx _idx)
{
    return _get_token(_t, _idx).line;
}

cmon_idx cmon_tokens_line_offset(cmon_tokens * _t, cmon_idx _idx)
{
    return _get_token(_t, _idx).line_off;
}

cmon_bool cmon_tokens_follows_nl(cmon_tokens * _t, cmon_idx _idx)
{
    return _get_token(_t, _idx).follows_nl;
}

static inline cmon_bool _is_impl_v(cmon_tokens * _t, cmon_idx _idx, va_list _args)
{
    cmon_token_kind kind;
    do
    {
        kind = va_arg(_args, cmon_token_kind);
        if (_get_kind(_t, _idx) == kind)
            return cmon_true;
    } while (kind != -1);

    return cmon_false;
}

cmon_bool cmon_tokens_is_impl(cmon_tokens * _t, cmon_idx _idx, ...)
{
    va_list args;
    cmon_bool ret;
    va_start(args, _idx);
    ret = _is_impl_v(_t, _idx, args);
    va_end(args);
    return ret;
}

cmon_bool cmon_tokens_is_current_impl(cmon_tokens * _t, ...)
{
    va_list args;
    cmon_bool ret;
    va_start(args, _t);
    ret = _is_impl_v(_t, _t->tok_idx, args);
    va_end(args);
    return ret;
}

cmon_bool cmon_tokens_is_next_impl(cmon_tokens * _t, ...)
{
    va_list args;
    cmon_bool ret;
    va_start(args, _t);
    ret = _is_impl_v(_t, _t->tok_idx + 1, args);
    va_end(args);
    return ret;
}

cmon_bool cmon_tokens_accept_impl(cmon_tokens * _t, ...)
{
    va_list args;
    cmon_bool ret;
    va_start(args, _t);
    ret = _is_impl_v(_t, _t->tok_idx, args);
    va_end(args);

    if(ret)
        return cmon_tokens_advance(_t, cmon_true);
    return CMON_INVALID_IDX;
}

// cmon_bool cmon_tokens_is(cmon_tokens * _t, cmon_idx _idx, cmon_token_kind _kind)
// {
//     return _get_kind(_t, _idx) == _kind;
// }

// cmon_bool cmon_tokens_is_next(cmon_tokens * _t, cmon_token_kind _kind)
// {
//     return cmon_tokens_is(_t, _t->tok_idx + 1, _kind);
// }

// cmon_bool cmon_tokens_is_current(cmon_tokens * _t, cmon_token_kind _kind)
// {
//     return cmon_tokens_is(_t, _t->tok_idx, _kind);
// }

// cmon_idx cmon_tokens_accept(cmon_tokens * _t, cmon_token_kind _kind)
// {
//     if (cmon_tokens_is_current(_t, _kind))
//         return cmon_tokens_advance(_t, cmon_true);
// }

const char * cmon_token_kind_to_str(cmon_token_kind _kind)
{
    switch (_kind)
    {
    case cmon_tk_ident:
        return "identifier";
    case cmon_tk_int:
        return "int literal";
    case cmon_tk_float:
        return "float literal";
    case cmon_tk_string:
        return "string literal";
    case cmon_tk_curl_open:
        return "{";
    case cmon_tk_curl_close:
        return "}";
    case cmon_tk_paran_open:
        return "(";
    case cmon_tk_paran_close:
        return ")";
    case cmon_tk_equals:
        return "==";
    case cmon_tk_not_equals:
        return "!=";
    case cmon_tk_assign:
        return "=";
    case cmon_tk_plus_assign:
        return "+=";
    case cmon_tk_minus_assign:
        return "-=";
    case cmon_tk_mult_assign:
        return "*=";
    case cmon_tk_div_assign:
        return "/=";
    case cmon_tk_mod_assign:
        return "%=";
    case cmon_tk_bw_left_assign:
        return "<<=";
    case cmon_tk_bw_right_assign:
        return ">>=";
    case cmon_tk_bw_and_assign:
        return "&=";
    case cmon_tk_bw_xor_assign:
        return "^=";
    case cmon_tk_bw_or_assign:
        return "|=";
    case cmon_tk_plus:
        return "+";
    case cmon_tk_minus:
        return "-";
    case cmon_tk_inc:
        return "++";
    case cmon_tk_dec:
        return "--";
    case cmon_tk_mult:
        return "*";
    case cmon_tk_div:
        return "/";
    case cmon_tk_mod:
        return "%";
    case cmon_tk_bw_left:
        return "<<";
    case cmon_tk_bw_right:
        return ">>";
    case cmon_tk_bw_and:
        return "&";
    case cmon_tk_bw_xor:
        return "^";
    case cmon_tk_bw_or:
        return "|";
    case cmon_tk_bw_not:
        return "~";
    case cmon_tk_fn:
        return "fn";
    case cmon_tk_dot:
        return ".";
    case cmon_tk_comma:
        return ",";
    case cmon_tk_colon:
        return ":";
    case cmon_tk_exclam:
        return "exclam";
    case cmon_tk_if:
        return "if";
    case cmon_tk_else:
        return "else";
    case cmon_tk_return:
        return "return";
    case cmon_tk_true:
        return "true";
    case cmon_tk_false:
        return "false";
    case cmon_tk_square_open:
        return "[";
    case cmon_tk_square_close:
        return "]";
    case cmon_tk_less:
        return "<";
    case cmon_tk_greater:
        return ">";
    case cmon_tk_less_equal:
        return "<=";
    case cmon_tk_greater_equal:
        return ">=";
    case cmon_tk_and:
        return "and";
    case cmon_tk_or:
        return "or";
    case cmon_tk_for:
        return "for";
    case cmon_tk_in:
        return "in";
    case cmon_tk_break:
        return "break";
    case cmon_tk_none:
        return "none";
    case cmon_tk_struct:
        return "struct";
    case cmon_tk_pub:
        return "pub";
    case cmon_tk_mut:
        return "mut";
    //@TODO: amp redundant with bw_and, remove this if amps only use case is bitwise and
    case cmon_tk_amp:
        return "&";
    case cmon_tk_as:
        return "as";
    case cmon_tk_double_dot:
        return "..";
    case cmon_tk_at:
        return "@";
    case cmon_tk_noinit:
        return "---";
    case cmon_tk_module:
        return "module";
    case cmon_tk_import:
        return "import";
    case cmon_tk_question:
        return "?";
    case cmon_tk_dollar:
        return "$";
    case cmon_tk_embed:
        return "embed";
    case cmon_tk_enum:
        return "enum";
    case cmon_tk_interface:
        return "interface";
    case cmon_tk_semicolon:
        return ";";
    case cmon_tk_continue:
        return "continue";
    case cmon_tk_self:
        return "self";
    case cmon_tk_type:
        return "type";
    case cmon_tk_alias:
        return "alias";
    case cmon_tk_defer:
        return "defer";
    case cmon_tk_try:
        return "try";
    case cmon_tk_comment:
        return "comment";
    case cmon_tk_scoped:
        return "scoped";
    case cmon_tk_scope_exit:
        return "scope_exit";
    case cmon_tk_eof:
        return "EOF";
    }
    return "unknown";
}
