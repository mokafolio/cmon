#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tokens.h>
#include <stdarg.h>

//@TODO: a lot of this is pretty messy, especially how kinds/token are separate right now (which is
// good, but a lot of the code can be a lot nicer). i.e. instead of having some stuff live in
// cmon_tokens and some in _tokenize_session, put everything in _tokenize_session and only create
// the cmon_tokens after successful completion.

// for now the lexer stops on the first error. we simpy save the error.
#define _err(_l, _msg, ...)                                                                        \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(_l->tmp_str_b);                                                     \
        cmon_str_builder_append_fmt(_l->tmp_str_b, _msg, ##__VA_ARGS__);                           \
        _l->err = cmon_err_report_make(_l->src_file_idx,                                           \
                                       cmon_dyn_arr_count(&_l->tokens),                            \
                                       cmon_dyn_arr_count(&_l->tokens),                            \
                                       cmon_dyn_arr_count(&_l->tokens),                            \
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

typedef struct
{
    cmon_idx tok_begin, tok_end;
    cmon_str_view str_view;
} _line;

typedef struct cmon_tokens
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_tokk) kinds;
    cmon_dyn_arr(_token) tokens;
    cmon_dyn_arr(_line) lines;
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
    const char * line_start;
    cmon_idx line_tok_start;
    cmon_dyn_arr(cmon_tokk) kinds;
    cmon_dyn_arr(_token) tokens;
    cmon_dyn_arr(_line) lines;
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

static inline void _finish_current_line(_tokenize_session * _l, cmon_bool _is_last_line, _token * _current)
{
    const char * end = _is_last_line ? _l->end : _l->pos;

    //this is a little messy to accomodate tokens that span over multiple lines.
    cmon_dyn_arr_append(&_l->lines,
                        ((_line){ _l->line_tok_start,
                                  _current ? cmon_dyn_arr_count(&_l->tokens) + 1 : cmon_dyn_arr_count(&_l->tokens),
                                  (cmon_str_view){ _l->line_start, end } }));
    _l->line_start = _l->pos + 1;
    _l->line_tok_start = cmon_dyn_arr_count(&_l->tokens);
}

static inline void _advance_line(_tokenize_session * _l, _token * _current)
{
    _finish_current_line(_l, cmon_false, _current);
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
            _advance_line(_l, NULL);
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

static cmon_tokk _tok_kind_for_name(const char * _begin, const char * _end)
{
    int len = _end - _begin;

    if (_name_equals(_begin, len, "if"))
        return cmon_tokk_if;
    else if (_name_equals(_begin, len, "else"))
        return cmon_tokk_else;
    else if (_name_equals(_begin, len, "fn"))
        return cmon_tokk_fn;
    else if (_name_equals(_begin, len, "return"))
        return cmon_tokk_return;
    else if (_name_equals(_begin, len, "break"))
        return cmon_tokk_break;
    else if (_name_equals(_begin, len, "and"))
        return cmon_tokk_and;
    else if (_name_equals(_begin, len, "or"))
        return cmon_tokk_or;
    else if (_name_equals(_begin, len, "struct"))
        return cmon_tokk_struct;
    else if (_name_equals(_begin, len, "true"))
        return cmon_tokk_true;
    else if (_name_equals(_begin, len, "false"))
        return cmon_tokk_false;
    else if (_name_equals(_begin, len, "mut"))
        return cmon_tokk_mut;
    else if (_name_equals(_begin, len, "pub"))
        return cmon_tokk_pub;
    else if (_name_equals(_begin, len, "as"))
        return cmon_tokk_as;
    else if (_name_equals(_begin, len, "in"))
        return cmon_tokk_in;
    else if (_name_equals(_begin, len, "for"))
        return cmon_tokk_for;
    else if (_name_equals(_begin, len, "module"))
        return cmon_tokk_module;
    else if (_name_equals(_begin, len, "import"))
        return cmon_tokk_import;
    else if (_name_equals(_begin, len, "none"))
        return cmon_tokk_none;
    else if (_name_equals(_begin, len, "embed"))
        return cmon_tokk_embed;
    else if (_name_equals(_begin, len, "interface"))
        return cmon_tokk_interface;
    else if (_name_equals(_begin, len, "self"))
        return cmon_tokk_self;
    else if (_name_equals(_begin, len, "type"))
        return cmon_tokk_type;
    else if (_name_equals(_begin, len, "alias"))
        return cmon_tokk_alias;
    else if (_name_equals(_begin, len, "defer"))
        return cmon_tokk_defer;
    else if (_name_equals(_begin, len, "try"))
        return cmon_tokk_try;
    else if (_name_equals(_begin, len, "continue"))
        return cmon_tokk_continue;
    else if (_name_equals(_begin, len, "scoped"))
        return cmon_tokk_scoped;
    else if (_name_equals(_begin, len, "scope_exit"))
        return cmon_tokk_scope_exit;

    return cmon_tokk_ident;
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
    if (isdigit(*_l->pos))
    {
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
                                      cmon_tokk _kind,
                                      size_t _advance,
                                      cmon_tokk * _out_kind,
                                      _token * _out_tok)
{
    _advance_pos(_l, _advance);
    _out_tok->str_view.end = _l->pos;
    *_out_kind = _kind;
    return cmon_true;
}

static cmon_bool _next_token(_tokenize_session * _l, cmon_tokk * _out_kind, _token * _out_tok)
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
        return _finalize_tok(_l, cmon_tokk_curl_open, 1, _out_kind, _out_tok);
    else if (*_l->pos == '}')
        return _finalize_tok(_l, cmon_tokk_curl_close, 1, _out_kind, _out_tok);
    else if (*_l->pos == '(')
        return _finalize_tok(_l, cmon_tokk_paran_open, 1, _out_kind, _out_tok);
    else if (*_l->pos == ')')
        return _finalize_tok(_l, cmon_tokk_paran_close, 1, _out_kind, _out_tok);
    else if (*_l->pos == '&')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_bw_and_assign, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tokk_bw_and, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '|')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_bw_or_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tokk_bw_or, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '^')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_bw_xor_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tokk_bw_xor, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == ';')
    {
        return _finalize_tok(_l, cmon_tokk_semicolon, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '~')
        return _finalize_tok(_l, cmon_tokk_bw_not, 1, _out_kind, _out_tok);
    else if (*_l->pos == '?')
        return _finalize_tok(_l, cmon_tokk_question, 1, _out_kind, _out_tok);
    else if (*_l->pos == '$')
        return _finalize_tok(_l, cmon_tokk_dollar, 1, _out_kind, _out_tok);
    else if (*_l->pos == ':')
    {
        return _finalize_tok(_l, cmon_tokk_colon, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '=')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_equals, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tokk_assign, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '<')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_less_equal, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '<'))
            return _finalize_tok(_l, cmon_tokk_bw_left, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tokk_less, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '>')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_greater_equal, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '>'))
            return _finalize_tok(_l, cmon_tokk_bw_right, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tokk_greater, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '+')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_plus_assign, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '+'))
            return _finalize_tok(_l, cmon_tokk_inc, 2, _out_kind, _out_tok);

        return _finalize_tok(_l, cmon_tokk_plus, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '-')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_minus_assign, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '>'))
            return _finalize_tok(_l, cmon_tokk_arrow, 2, _out_kind, _out_tok);
        else if (_next_char_is(_l, '-'))
        {
            _advance_pos(_l, 1);
            if (_next_char_is(_l, '-'))
            {
                // noinit
                _finalize_tok(_l, cmon_tokk_noinit, 2, _out_kind, _out_tok);
            }
            return _finalize_tok(_l, cmon_tokk_dec, 1, _out_kind, _out_tok);
        }
        return _finalize_tok(_l, cmon_tokk_minus, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '*')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_mult_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tokk_mult, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '%')
    {
        if (_next_char_is(_l, '='))
            return _finalize_tok(_l, cmon_tokk_mod_assign, 2, _out_kind, _out_tok);
        return _finalize_tok(_l, cmon_tokk_mod, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == ',')
        return _finalize_tok(_l, cmon_tokk_comma, 1, _out_kind, _out_tok);
    else if (*_l->pos == '!')
    {
        if (_next_char_is(_l, '='))
        {
            return _finalize_tok(_l, cmon_tokk_not_equals, 2, _out_kind, _out_tok);
        }
        return _finalize_tok(_l, cmon_tokk_exclam, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '[')
        return _finalize_tok(_l, cmon_tokk_square_open, 1, _out_kind, _out_tok);
    else if (*_l->pos == ']')
        return _finalize_tok(_l, cmon_tokk_square_close, 1, _out_kind, _out_tok);
    else if (*_l->pos == '@')
        return _finalize_tok(_l, cmon_tokk_at, 1, _out_kind, _out_tok);
    else if (*_l->pos == '.' && !_peek_float_lit(_l))
    {
        if (_next_char_is(_l, '.'))
        {
            return _finalize_tok(_l, cmon_tokk_double_dot, 2, _out_kind, _out_tok);
        }
        return _finalize_tok(_l, cmon_tokk_dot, 1, _out_kind, _out_tok);
    }
    else if (*_l->pos == '"' || *_l->pos == '\'')
    {
        // assert(0);
        char marks = *_l->pos;
        _advance_pos(_l, 1);
        while (*_l->pos != marks && _l->pos != _l->end)
        {
            // if (*_l->pos == '\n')
            //     _advance_line(_l);
            _advance_pos(_l, 1);
        }
        *_out_kind = cmon_tokk_string;
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
            *_out_kind = cmon_tokk_comment;
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
                        _advance_line(_l, _out_tok);
                    _advance_pos(_l, 1);
                }
            }

            if (openings_to_match)
                _err(_l, "unclosed multiline comment");

            *_out_kind = cmon_tokk_comment;
            _out_tok->str_view.end = _l->pos;
            _out_tok->line_end = _l->current_line;

            return cmon_true;
        }
        else if (_next_char_is(_l, '='))
        {
            return _finalize_tok(_l, cmon_tokk_div_assign, 2, _out_kind, _out_tok);
        }
        else
        {
            return _finalize_tok(_l, cmon_tokk_div, 1, _out_kind, _out_tok);
        }
    }
    else
    {
        // this is not any of the explicitly defined Tokens.
        // it has to be a identifier, number or keyword
        const char * startp = _l->pos;
        if (isalpha(*_l->pos) || *_l->pos == '_')
        {
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
                *_out_kind = cmon_tokk_float;
            else
                *_out_kind = cmon_tokk_int;
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
    ret->kinds = NULL;
    ret->tokens = NULL;
    ret->lines = NULL;
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

    _s->alloc = _alloc;
    _s->src = _src;
    _s->src_file_idx = _src_file_idx;
    _s->input = code;
    _s->pos = code;
    _s->end = code + strlen(code);
    _s->line_start = code;
    _s->line_tok_start = 0;
    _s->current_line = 1;
    _s->current_line_off = 1;
    cmon_dyn_arr_init(&_s->kinds, _alloc, 128);
    cmon_dyn_arr_init(&_s->tokens, _alloc, 128);
    cmon_dyn_arr_init(&_s->lines, _alloc, 32);
    _s->err = cmon_err_report_make_empty();
    //@TODO: lazy init the string builder only on error
    _s->tmp_str_b = cmon_str_builder_create(_alloc, 512);
}

static inline void _tokenize_session_dealloc(_tokenize_session * _s, cmon_bool _was_successful)
{
    // if tokenization was successful, some ownership was moved to cmon_tokens, in that case we
    // don't free it
    // if (!_was_successful)
    // {
    //     cmon_dyn_arr_dealloc(&_s->lines);
    //     cmon_dyn_arr_dealloc(&_s->tokens);
    //     cmon_dyn_arr_dealloc(&_s->kinds);
    // }
    cmon_str_builder_destroy(_s->tmp_str_b);
}

cmon_tokens * cmon_tokenize(cmon_allocator * _alloc,
                            cmon_src * _src,
                            cmon_idx _src_file_idx,
                            cmon_err_report * _out_err)
{
    _token tok;
    cmon_tokk kind;
    _tokenize_session s;

    _tokenize_session_init(&s, _alloc, _src, _src_file_idx);
    cmon_tokens * ret = _tokens_create(_alloc);
    while (_next_token(&s, &kind, &tok))
    {
        // sanity check. tokens can't consume no characters
        // assert(tok.str_view.begin < tok.str_view.end);
        cmon_dyn_arr_append(&s.kinds, kind);
        cmon_dyn_arr_append(&s.tokens, tok);

        if (!cmon_err_report_is_empty(&s.err))
            break;
    }

    _finish_current_line(&s, cmon_true, NULL);

    tok.str_view.begin = s.end;
    tok.str_view.end = s.end;
    cmon_dyn_arr_append(&s.kinds, cmon_tokk_eof);
    cmon_dyn_arr_append(&s.tokens, tok);

    ret->tok_idx = 0;
    ret->kinds = s.kinds;
    ret->tokens = s.tokens;
    ret->lines = s.lines;

    // printf("DA FOCKING LINES %lu %lu\n", cmon_dyn_arr_count(&s.lines), cmon_dyn_arr_count(&s.tokens));
    // for (size_t i = 0; i < cmon_dyn_arr_count(&s.lines); ++i)
    // {
    //     printf("%lu: %.*s (tok count: %lu %lu)\n", i + 1, s.lines[i].str_view.end - s.lines[i].str_view.begin, s.lines[i].str_view.begin, s.lines[i].tok_begin, s.lines[i].tok_end);
    // }
    // printf("\n\n");

    *_out_err = s.err;
    _tokenize_session_dealloc(&s, ret != NULL);
    return ret;
}

void cmon_tokens_destroy(cmon_tokens * _t)
{
    if (!_t)
        return;
    cmon_dyn_arr_dealloc(&_t->lines);
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
        if (!_skip_comments || _t->kinds[ret] != cmon_tokk_comment)
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
        if (!_skip_comments || _t->kinds[ret] != cmon_tokk_comment)
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

cmon_tokk cmon_tokens_kind(cmon_tokens * _t, cmon_idx _idx)
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

static inline _line * _get_line(cmon_tokens * _t, cmon_idx _l)
{
    assert(_l <= cmon_dyn_arr_count(&_t->lines));
    return &_t->lines[_l - 1];
}

size_t cmon_tokens_line_token_count(cmon_tokens * _t, cmon_idx _l)
{
    _line * l = _get_line(_t, _l);
    return l->tok_end - l->tok_begin;
}

cmon_idx cmon_tokens_line_token(cmon_tokens * _t, cmon_idx _l, size_t _toki)
{
    _line * l = _get_line(_t, _l);
    assert(_toki < cmon_tokens_line_token_count(_t, _l));
    return l->tok_begin + _toki;
}

// size_t cmon_tokens_line_count(cmon_tokens * _t, cmon_idx _idx)
// {
//     cmon_idx next_idx = _idx + 1;
//     if (next_idx < cmon_dyn_arr_count(&_t->kinds))
//     {
//         size_t a = cmon_tokens_line(_t, _idx);
//         cmon_str_view sv = cmon_tokens_str_view(_t, _idx);
//         size_t count = 0;
//         while (_t->lines[a - 1].str_view.begin <= sv.end)
//         {
//             ++count;
//             ++a;
//         }
//         return count;
//     }
//     return 1;
// }

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
    cmon_tokk kind;
    do
    {
        kind = va_arg(_args, cmon_tokk);
        if (_get_kind(_t, _idx) == kind)
            return cmon_true;
    } while (kind != -1);

    return cmon_false;
}

cmon_bool cmon_tokens_is_impl_v(cmon_tokens * _t, cmon_idx _idx, va_list _args)
{
    return _is_impl_v(_t, _idx, _args);
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

cmon_idx cmon_tokens_accept_impl_v(cmon_tokens * _t, va_list _args)
{
    if (_is_impl_v(_t, _t->tok_idx, _args))
        return cmon_tokens_advance(_t, cmon_true);
    return CMON_INVALID_IDX;
}

cmon_idx cmon_tokens_accept_impl(cmon_tokens * _t, ...)
{
    va_list args;
    cmon_idx ret;
    va_start(args, _t);
    ret = cmon_tokens_accept_impl_v(_t, args);
    va_end(args);
    return ret;
}

cmon_str_view cmon_tokens_line_str_view(cmon_tokens * _t, size_t _line)
{
    assert(_line > 0);
    assert(_line <= cmon_dyn_arr_count(&_t->lines));
    return _t->lines[_line - 1].str_view;
}

// cmon_bool cmon_tokens_is(cmon_tokens * _t, cmon_idx _idx, cmon_tokk _kind)
// {
//     return _get_kind(_t, _idx) == _kind;
// }

// cmon_bool cmon_tokens_is_next(cmon_tokens * _t, cmon_tokk _kind)
// {
//     return cmon_tokens_is(_t, _t->tok_idx + 1, _kind);
// }

// cmon_bool cmon_tokens_is_current(cmon_tokens * _t, cmon_tokk _kind)
// {
//     return cmon_tokens_is(_t, _t->tok_idx, _kind);
// }

// cmon_idx cmon_tokens_accept(cmon_tokens * _t, cmon_tokk _kind)
// {
//     if (cmon_tokens_is_current(_t, _kind))
//         return cmon_tokens_advance(_t, cmon_true);
// }

const char * cmon_tokk_to_str(cmon_tokk _kind)
{
    switch (_kind)
    {
    case cmon_tokk_ident:
        return "identifier";
    case cmon_tokk_int:
        return "int literal";
    case cmon_tokk_float:
        return "float literal";
    case cmon_tokk_string:
        return "string literal";
    case cmon_tokk_curl_open:
        return "{";
    case cmon_tokk_curl_close:
        return "}";
    case cmon_tokk_paran_open:
        return "(";
    case cmon_tokk_paran_close:
        return ")";
    case cmon_tokk_equals:
        return "==";
    case cmon_tokk_not_equals:
        return "!=";
    case cmon_tokk_assign:
        return "=";
    case cmon_tokk_plus_assign:
        return "+=";
    case cmon_tokk_minus_assign:
        return "-=";
    case cmon_tokk_mult_assign:
        return "*=";
    case cmon_tokk_div_assign:
        return "/=";
    case cmon_tokk_mod_assign:
        return "%=";
    case cmon_tokk_bw_left_assign:
        return "<<=";
    case cmon_tokk_bw_right_assign:
        return ">>=";
    case cmon_tokk_bw_and_assign:
        return "&=";
    case cmon_tokk_bw_xor_assign:
        return "^=";
    case cmon_tokk_bw_or_assign:
        return "|=";
    case cmon_tokk_plus:
        return "+";
    case cmon_tokk_minus:
        return "-";
    case cmon_tokk_inc:
        return "++";
    case cmon_tokk_dec:
        return "--";
    case cmon_tokk_mult:
        return "*";
    case cmon_tokk_div:
        return "/";
    case cmon_tokk_mod:
        return "%";
    case cmon_tokk_bw_left:
        return "<<";
    case cmon_tokk_bw_right:
        return ">>";
    case cmon_tokk_bw_and:
        return "&";
    case cmon_tokk_bw_xor:
        return "^";
    case cmon_tokk_bw_or:
        return "|";
    case cmon_tokk_bw_not:
        return "~";
    case cmon_tokk_arrow:
        return "->";
    case cmon_tokk_fn:
        return "fn";
    case cmon_tokk_dot:
        return ".";
    case cmon_tokk_comma:
        return ",";
    case cmon_tokk_colon:
        return ":";
    case cmon_tokk_exclam:
        return "exclam";
    case cmon_tokk_if:
        return "if";
    case cmon_tokk_else:
        return "else";
    case cmon_tokk_return:
        return "return";
    case cmon_tokk_true:
        return "true";
    case cmon_tokk_false:
        return "false";
    case cmon_tokk_square_open:
        return "[";
    case cmon_tokk_square_close:
        return "]";
    case cmon_tokk_less:
        return "<";
    case cmon_tokk_greater:
        return ">";
    case cmon_tokk_less_equal:
        return "<=";
    case cmon_tokk_greater_equal:
        return ">=";
    case cmon_tokk_and:
        return "and";
    case cmon_tokk_or:
        return "or";
    case cmon_tokk_for:
        return "for";
    case cmon_tokk_in:
        return "in";
    case cmon_tokk_break:
        return "break";
    case cmon_tokk_none:
        return "none";
    case cmon_tokk_struct:
        return "struct";
    case cmon_tokk_pub:
        return "pub";
    case cmon_tokk_mut:
        return "mut";
    //@TODO: amp redundant with bw_and, remove this if amps only use case is bitwise and
    // case cmon_tokk_amp:
    //     return "&";
    case cmon_tokk_as:
        return "as";
    case cmon_tokk_double_dot:
        return "..";
    case cmon_tokk_at:
        return "@";
    case cmon_tokk_noinit:
        return "---";
    case cmon_tokk_module:
        return "module";
    case cmon_tokk_import:
        return "import";
    case cmon_tokk_question:
        return "?";
    case cmon_tokk_dollar:
        return "$";
    case cmon_tokk_embed:
        return "embed";
    case cmon_tokk_enum:
        return "enum";
    case cmon_tokk_interface:
        return "interface";
    case cmon_tokk_semicolon:
        return ";";
    case cmon_tokk_continue:
        return "continue";
    case cmon_tokk_self:
        return "self";
    case cmon_tokk_type:
        return "type";
    case cmon_tokk_alias:
        return "alias";
    case cmon_tokk_defer:
        return "defer";
    case cmon_tokk_try:
        return "try";
    case cmon_tokk_comment:
        return "comment";
    case cmon_tokk_scoped:
        return "scoped";
    case cmon_tokk_scope_exit:
        return "scope_exit";
    case cmon_tokk_eof:
        return "EOF";
    }
    return "unknown";
}
