#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_lexer.h>
#include <cmon/cmon_str_builder.h>

// for now the lexer stops on the first error. we simpy save the error.
#define _err(_l, _msg, ...)                                                                        \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(_l->tmp_str_b);                                                     \
        cmon_str_builder_append_fmt(_l->tmp_str_b, _msg, ##__VA_ARGS__);                           \
        _l->err = cmon_err_report_make(_l->filename,                                               \
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

typedef struct cmon_lexer
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_token_kind) kinds;
    cmon_dyn_arr(_token) tokens;
    char path[CMON_PATH_MAX];
    char filename[CMON_FILENAME_MAX];
    cmon_src * src;
    cmon_idx src_file_idx;
    const char * input;
    const char * pos;
    const char * end;
    cmon_idx current_line;
    cmon_idx current_line_off;
    cmon_idx tok_idx;
    cmon_err_report err;
    cmon_str_builder * tmp_str_b;
} cmon_lexer;

static inline void _advance_pos(cmon_lexer * _l, size_t _advance)
{
    _l->pos += _advance;
    _l->current_line_off += _advance;
}

static inline void _advance_line(cmon_lexer * _l)
{
    ++_l->current_line;
    _l->current_line_off = 0;
}

static inline size_t _skip_whitespace(cmon_lexer * _l)
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

    return cmon_tk_ident;
}

static cmon_bool _peek_float_lit(cmon_lexer * _l)
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

static inline cmon_bool _next_char_is(cmon_lexer * _l, char _c)
{
    return _l->pos < _l->end && *(_l->pos + 1) == _c;
}

static inline void _consume_digits(cmon_lexer * _l)
{
    while (isdigit(*_l->pos))
        _advance_pos(_l, 1);
}

static inline void _parse_float_or_int_literal(cmon_lexer * _l, cmon_bool * _is_float)
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

static inline cmon_bool _append_tok(cmon_lexer * _l, _token * _tok, cmon_token_kind _kind)
{
    cmon_dyn_arr_append(&_l->kinds, _kind);
    cmon_dyn_arr_append(&_l->tokens, *_tok);
    return cmon_true;
}

static inline cmon_bool _add_and_advance_tok(cmon_lexer * _l,
                                             _token * _tok,
                                             cmon_token_kind _kind,
                                             size_t _advance)
{
    _advance_pos(_l, _advance);
    _tok->str_view.end = _l->pos;
    return _append_tok(_l, _tok, _kind);
}

static cmon_bool _next_token(cmon_lexer * _l)
{
    _token tok;

    if (_l->pos >= _l->end)
        return cmon_false;

    tok.follows_nl = _skip_whitespace(_l) > 0;

    if (_l->pos >= _l->end)
        return cmon_false;

    tok.str_view.begin = _l->pos;

    if (*_l->pos == '{')
        return _add_and_advance_tok(_l, &tok, cmon_tk_curl_open, 1);
    else if (*_l->pos == '}')
        return _add_and_advance_tok(_l, &tok, cmon_tk_curl_close, 1);
    else if (*_l->pos == '(')
        return _add_and_advance_tok(_l, &tok, cmon_tk_paran_open, 1);
    else if (*_l->pos == ')')
        return _add_and_advance_tok(_l, &tok, cmon_tk_paran_close, 1);
    else if (*_l->pos == '&')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_bw_and_assign, 2);

        return _add_and_advance_tok(_l, &tok, cmon_tk_bw_and, 1);
    }
    else if (*_l->pos == '|')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_bw_or_assign, 2);
        return _add_and_advance_tok(_l, &tok, cmon_tk_bw_or, 1);
    }
    else if (*_l->pos == '^')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_bw_xor_assign, 2);
        return _add_and_advance_tok(_l, &tok, cmon_tk_bw_xor, 1);
    }
    else if (*_l->pos == ';')
    {
        return _add_and_advance_tok(_l, &tok, cmon_tk_semicolon, 1);
    }
    else if (*_l->pos == '~')
        return _add_and_advance_tok(_l, &tok, cmon_tk_bw_not, 1);
    else if (*_l->pos == '?')
        return _add_and_advance_tok(_l, &tok, cmon_tk_question, 1);
    else if (*_l->pos == '$')
        return _add_and_advance_tok(_l, &tok, cmon_tk_dollar, 1);
    else if (*_l->pos == ':')
    {
        return _add_and_advance_tok(_l, &tok, cmon_tk_colon, 1);
    }
    else if (*_l->pos == '=')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_equals, 2);

        return _add_and_advance_tok(_l, &tok, cmon_tk_assign, 1);
    }
    else if (*_l->pos == '<')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_less_equal, 2);
        else if (_next_char_is(_l, '<'))
            return _add_and_advance_tok(_l, &tok, cmon_tk_bw_left, 2);

        return _add_and_advance_tok(_l, &tok, cmon_tk_less, 1);
    }
    else if (*_l->pos == '>')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_greater_equal, 2);
        else if (_next_char_is(_l, '>'))
            return _add_and_advance_tok(_l, &tok, cmon_tk_bw_right, 2);

        return _add_and_advance_tok(_l, &tok, cmon_tk_greater, 1);
    }
    else if (*_l->pos == '+')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_plus_assign, 2);
        else if (_next_char_is(_l, '+'))
            return _add_and_advance_tok(_l, &tok, cmon_tk_inc, 2);

        return _add_and_advance_tok(_l, &tok, cmon_tk_plus, 1);
    }
    else if (*_l->pos == '-')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_minus_assign, 2);
        else if (_next_char_is(_l, '-'))
        {
            _advance_pos(_l, 1);
            if (_next_char_is(_l, '-'))
            {
                // noinit
                _add_and_advance_tok(_l, &tok, cmon_tk_noinit, 2);
            }
            return _add_and_advance_tok(_l, &tok, cmon_tk_dec, 1);
        }
        return _add_and_advance_tok(_l, &tok, cmon_tk_minus, 1);
    }
    else if (*_l->pos == '*')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_mult_assign, 2);
        return _add_and_advance_tok(_l, &tok, cmon_tk_mult, 1);
    }
    else if (*_l->pos == '%')
    {
        if (_next_char_is(_l, '='))
            return _add_and_advance_tok(_l, &tok, cmon_tk_mod_assign, 2);
        return _add_and_advance_tok(_l, &tok, cmon_tk_mod, 1);
    }
    else if (*_l->pos == ',')
        return _add_and_advance_tok(_l, &tok, cmon_tk_comma, 1);
    else if (*_l->pos == '!')
    {
        if (_next_char_is(_l, '='))
        {
            return _add_and_advance_tok(_l, &tok, cmon_tk_not_equals, 2);
        }
        return _add_and_advance_tok(_l, &tok, cmon_tk_exclam, 1);
    }
    else if (*_l->pos == '[')
        return _add_and_advance_tok(_l, &tok, cmon_tk_square_open, 1);
    else if (*_l->pos == ']')
        return _add_and_advance_tok(_l, &tok, cmon_tk_square_close, 1);
    else if (*_l->pos == '@')
        return _add_and_advance_tok(_l, &tok, cmon_tk_at, 1);
    else if (*_l->pos == '.' && !_peek_float_lit(_l))
    {
        if (_next_char_is(_l, '.'))
        {
            return _add_and_advance_tok(_l, &tok, cmon_tk_double_dot, 2);
        }
        return _add_and_advance_tok(_l, &tok, cmon_tk_dot, 1);
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
        tok.str_view.end = _l->pos;
        _advance_pos(_l, 1); // skip closing "
        return _append_tok(_l, &tok, cmon_tk_string);
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
            tok.str_view.end = _l->pos;
            return _append_tok(_l, &tok, cmon_tk_comment);
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

            tok.str_view.end = _l->pos;
            tok.line_end = _l->current_line;

            return _append_tok(_l, &tok, cmon_tk_comment);
        }
        else if (_next_char_is(_l, '='))
        {
            return _add_and_advance_tok(_l, &tok, cmon_tk_div_assign, 2);
        }
        else
        {
            return _add_and_advance_tok(_l, &tok, cmon_tk_div, 1);
        }
    }
    else
    {
        // this is not any of the explicitly defined Tokens.
        // it has to be a identifier, number or keyword
        cmon_token_kind kind;
        const char * startp = _l->pos;
        if (isalpha(*_l->pos) || *_l->pos == '_')
        {
            printf("ident\n");
            while (isalnum(*_l->pos) || *_l->pos == '_')
                _advance_pos(_l, 1);

            // this will set it to either cmon_tok_type_ident or the correct keyword token type
            kind = _tok_kind_for_name(startp, _l->pos);
        }
        // integer or float
        //@TODO: Make this work for hex, octal, scientific float notation etc.
        else if (isdigit(*_l->pos) || *_l->pos == '.')
        {
            cmon_bool is_float;
            _parse_float_or_int_literal(_l, &is_float);
            if (is_float)
                kind = cmon_tk_float;
            else
                kind = cmon_tk_int;

            printf("DA LIT %.*s\n\n", _l->pos - startp, startp);
        }
        else
        {
            //@TODO: better error
            _err(_l, "invalid character '%c' in source code", *_l->pos);
        }

        tok.str_view.end = _l->pos;
        return _append_tok(_l, &tok, kind);
    }
    return cmon_false;
}

cmon_lexer * cmon_lexer_create(cmon_allocator * _alloc)
{
    cmon_lexer * ret = CMON_CREATE(_alloc, cmon_lexer);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->kinds, _alloc, 128);
    cmon_dyn_arr_init(&ret->tokens, _alloc, 128);
    memset(ret->path, 0, CMON_PATH_MAX);
    memset(ret->filename, 0, CMON_FILENAME_MAX);
    ret->input = NULL;
    ret->pos = NULL;
    ret->end = NULL;
    ret->current_line = -1;
    ret->current_line_off = -1;
    ret->tok_idx = -1;
    ret->err = cmon_err_report_make_empty();
    ret->tmp_str_b = cmon_str_builder_create(_alloc, 512);
    return ret;
}

void cmon_lexer_destroy(cmon_lexer * _l)
{
    if (!_l)
        return;
    cmon_str_builder_destroy(_l->tmp_str_b);
    cmon_dyn_arr_dealloc(&_l->tokens);
    cmon_dyn_arr_dealloc(&_l->kinds);
    CMON_DESTROY(_l->alloc, _l);
}

void cmon_lexer_set_input(cmon_lexer * _l, cmon_src * _src, cmon_idx _src_file_idx)
{
    const char * code;
    _l->src = _src;
    _l->src_file_idx = _src_file_idx;
    code = cmon_src_code(_src, _src_file_idx);
    _l->input = code;
    _l->pos = code;
    _l->end = code + strlen(code);
}

cmon_bool cmon_lexer_tokenize(cmon_lexer * _l)
{
}

cmon_idx cmon_lexer_prev(cmon_lexer * _l)
{
    if (_l->tok_idx < 1)
        return -1;
    return _l->tok_idx - 1;
}

cmon_idx cmon_lexer_current(cmon_lexer * _l)
{
    return _l->tok_idx;
}

cmon_idx cmon_lexer_next(cmon_lexer * _l)
{
    return _l->tok_idx < cmon_dyn_arr_count(&_l->tokens) - 1 ? _l->tok_idx + 1 : -1;
}

cmon_idx cmon_lexer_advance(cmon_lexer * _l)
{
    return _l->tok_idx < cmon_dyn_arr_count(&_l->tokens) - 1 ? _l->tok_idx++ : -1;
}

#define _get_token(_l, _idx) (assert(_idx < cmon_dyn_arr_count(&_l->tokens)), _l->tokens[_idx])
#define _get_kind(_l, _idx) (assert(_idx < cmon_dyn_arr_count(&_l->kinds)), _l->kinds[_idx])

cmon_token_kind cmon_lexer_token_kind(cmon_lexer * _l, cmon_idx _idx)
{
    return _get_kind(_l, _idx);
}

cmon_str_view Fcmon_lexer_str_view(cmon_lexer * _l, cmon_idx _idx)
{
    return _get_token(_l, _idx).str_view;
}

cmon_idx cmon_lexer_line(cmon_lexer * _l, cmon_idx _idx)
{
    return _get_token(_l, _idx).line;
}

cmon_idx cmon_lexer_line_offset(cmon_lexer * _l, cmon_idx _idx)
{
    return _get_token(_l, _idx).line_off;
}

cmon_bool cmon_lexer_follows_nl(cmon_lexer * _l, cmon_idx _idx)
{
    return _get_token(_l, _idx).follows_nl;
}

cmon_bool cmon_lexer_is_at(cmon_lexer * _l, cmon_token_kind _kind, cmon_idx _idx)
{
    return _get_kind(_l, _idx) == _kind;
}

cmon_bool cmon_lexer_is_next(cmon_lexer * _l, cmon_token_kind _kind)
{
    return cmon_lexer_is_at(_l, _kind, _l->tok_idx + 1);
}

cmon_bool cmon_lexer_is_current(cmon_lexer * _l, cmon_token_kind _kind)
{
    return cmon_lexer_is_at(_l, _kind, _l->tok_idx);
}

cmon_idx cmon_lexer_accept(cmon_lexer * _l, cmon_token_kind _kind)
{
    if (cmon_lexer_is_current(_l, _kind))
        return cmon_lexer_advance(_l);
}
