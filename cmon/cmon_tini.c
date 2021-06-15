#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
#include <cmon/cmon_idx_buf_mng.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tini.h>
#include <stdarg.h>

typedef enum
{
    _tokk_name,
    _tokk_string,
    _tokk_comma,
    _tokk_assign,
    _tokk_square_open,
    _tokk_square_close,
    _tokk_curl_open,
    _tokk_curl_close,
    _tokk_comment,
    _tokk_eof,
    _tokk_none // used to indicate no previous token in next_token function
} _tokk;

typedef struct
{
    _tokk kind;
    cmon_idx line;
    cmon_idx line_end;
    cmon_idx line_off;
    cmon_str_view str_view;
} _token;

typedef struct
{
    cmon_str_view key;
    cmon_idx val_idx;
} _key_val;

typedef struct
{
    cmon_idx children_begin;
    cmon_idx children_end;
} _array;

typedef struct
{
    cmon_idx children_begin;
    cmon_idx children_end;
    cmon_hashmap(cmon_str_view, cmon_idx) name_map;
} _obj;

// we use one helper object for tokenizing and parsing to keep it simple
typedef struct
{
    cmon_allocator * alloc;
    const char * name;
    // tokenizing related
    const char * input;
    const char * pos;
    const char * end;
    cmon_idx current_line;
    cmon_idx current_line_off;
    // parsing related
    cmon_idx_buf_mng * idx_buf_mng;
    cmon_dyn_arr(_token) tokens;
    cmon_idx tok_idx;
    cmon_dyn_arr(cmon_tinik) kinds;
    cmon_dyn_arr(cmon_idx) data;
    cmon_dyn_arr(cmon_idx) idx_buffer;
    cmon_dyn_arr(cmon_str_view) str_values;
    cmon_dyn_arr(_key_val) key_value_pairs;
    cmon_dyn_arr(_array) arrays;
    cmon_dyn_arr(_obj) objs;
    // error
    cmon_err_report err;
    cmon_str_builder * tmp_str_b;
    cmon_str_builder * tk_str_builder;
} _tokparse;

typedef struct cmon_tini
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_tinik) kinds;
    cmon_dyn_arr(cmon_idx) data;
    cmon_dyn_arr(cmon_str_view) str_values;
    cmon_dyn_arr(_key_val) key_value_pairs;
    cmon_dyn_arr(_array) arrays;
    cmon_dyn_arr(_obj) objs;
} cmon_tini;

#define _err(_tp, _fmt, ...)                                                                       \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(_tp->tmp_str_b);                                                    \
        cmon_str_builder_append_fmt(_tp->tmp_str_b, _fmt, ##__VA_ARGS__);                          \
        _tp->err = cmon_err_report_make(_tp->name,                                                 \
                                        _tp->current_line,                                         \
                                        _tp->current_line_off,                                     \
                                        cmon_str_builder_c_str(_tp->tmp_str_b));                   \
    } while (0)

// tokenize related functions
static inline void _advance_pos(_tokparse * _t, size_t _advance)
{
    _t->pos += _advance;
    _t->current_line_off += _advance;
}

static inline void _advance_line(_tokparse * _t)
{
    ++_t->current_line;
    _t->current_line_off = 0;
}

static inline size_t _skip_whitespace(_tokparse * _t)
{
    size_t count = 0;
    while (isspace(*_t->pos))
    {
        if (*_t->pos == '\n')
        {
            _advance_line(_t);
            ++count;
        }
        _advance_pos(_t, 1);
    }
    return count;
}

static inline cmon_bool _finalize_tok(_tokparse * _t,
                                      _tokk _kind,
                                      size_t _advance,
                                      _token * _out_tok)
{
    _advance_pos(_t, _advance);
    _out_tok->str_view.end = _t->pos;
    _out_tok->kind = _kind;
    return cmon_true;
}

static cmon_bool _next_token(_tokparse * _t, _token * _out_tok, _tokk _prev_kind)
{
    if (_t->pos >= _t->end)
        return cmon_false;

    _skip_whitespace(_t);

    if (_t->pos >= _t->end)
        return cmon_false;

    _out_tok->str_view.begin = _t->pos;
    _out_tok->line = _t->current_line;
    _out_tok->line_off = _t->current_line_off;

    if (*_t->pos == '{')
        return _finalize_tok(_t, _tokk_curl_open, 1, _out_tok);
    else if (*_t->pos == '}')
        return _finalize_tok(_t, _tokk_curl_close, 1, _out_tok);
    else if (*_t->pos == '[')
        return _finalize_tok(_t, _tokk_square_open, 1, _out_tok);
    else if (*_t->pos == ']')
        return _finalize_tok(_t, _tokk_square_close, 1, _out_tok);
    else if (*_t->pos == '=')
        return _finalize_tok(_t, _tokk_assign, 1, _out_tok);
    else if (*_t->pos == ',')
        return _finalize_tok(_t, _tokk_comma, 1, _out_tok);
    else if (*_t->pos == '#')
    {
        while (*_t->pos != '\n' && _t->pos != _t->end)
        {
            _advance_pos(_t, 1);
        }
        _out_tok->kind = _tokk_comment;
        _out_tok->str_view.end = _t->pos;
        return cmon_true;
    }
    else
    {
        //@TODO: This whole else block could be compacted a lot more!
        if (_prev_kind == _tokk_assign || _prev_kind == _tokk_comma)
        {
            // if this is an explicit string/multiline string
            if (*_t->pos == '"')
            {
                _advance_pos(_t, 1);
                while (*_t->pos != '"' && _t->pos != _t->end)
                {
                    if (*_t->pos == '\n')
                    {
                        _advance_line(_t);
                    }
                    _advance_pos(_t, 1);
                }

                // skip closing "
                _advance_pos(_t, 1);
            }
            else
            {
                while (*_t->pos != ',' && *_t->pos != ']' && *_t->pos != '}' && *_t->pos != '=' &&
                       _t->pos != _t->end)
                    _advance_pos(_t, 1);
            }

            _out_tok->kind = _tokk_string;
        }
        else
        {
            if (isalpha(*_t->pos) || *_t->pos == '_')
            {
                while ((isalnum(*_t->pos) || *_t->pos == '_') && _t->pos != _t->end)
                    _advance_pos(_t, 1);
            }
            else
            {
                //@TODO: Error, bad name
                assert(0);
            }
            _out_tok->kind = _tokk_name;
        }
        return cmon_true;
    }
    return cmon_false;
}

// parsing related functions
static inline _tokk _tok_kind(_tokparse * _t, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_t->tokens));
    return _t->tokens[_idx].kind;
}

static inline cmon_str_view _tok_str_view(_tokparse * _t, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_t->tokens));
    return _t->tokens[_idx].str_view;
}

static inline cmon_bool _tokens_is_impl_v(_tokparse * _t, cmon_idx _idx, va_list _args)
{
    _tokk kind;
    do
    {
        kind = va_arg(_args, _tokk);
        if (_tok_kind(_t, _idx) == kind)
            return cmon_true;
    } while (kind != -1);

    return cmon_false;
}

static inline cmon_bool _tokens_is_impl(_tokparse * _t, cmon_idx _idx, ...)
{
    va_list args;
    va_start(args, _t);
    cmon_idx ret = _tokens_is_impl_v(_t, _idx, args);
    va_end(args);
    return ret;
}

static inline cmon_idx _tokens_accept_impl_v(_tokparse * _t, va_list _args)
{
    if (_tokens_is_impl_v(_t, _t->tok_idx, _args))
        return _token_advance(_t, cmon_true);
    return CMON_INVALID_IDX;
}

static inline cmon_idx _tokens_accept_impl(_tokparse * _t, ...)
{
    va_list args;
    cmon_idx ret;
    va_start(args, _t);
    ret = _tokens_accept_impl_v(_t, args);
    va_end(args);
    return ret;
}

static inline const char * _tokk_to_str(_tokk _kind)
{
    switch (_kind)
    {
    case _tokk_name:
        return "name";
    case _tokk_string:
        return "string";
    case _tokk_comma:
        return ",";
    case _tokk_assign:
        return "=";
    case _tokk_square_open:
        return "[";
    case _tokk_square_close:
        return "]";
    case _tokk_curl_open:
        return "{";
    case _tokk_curl_close:
        return "}";
    case _tokk_comment:
        return "#comment";
    case _tokk_eof:
        return "EOF";
    }
    return "none";
}

static inline const char * _token_kinds_to_str(cmon_str_builder * _b, va_list _args)
{
    _tokk kind;
    cmon_str_builder_clear(_b);
    do
    {
        kind = va_arg(_args, _tokk);
        if (kind != -1)
        {
            if (cmon_str_builder_count(_b))
                cmon_str_builder_append(_b, " or ");
            cmon_str_builder_append_fmt(_b, "'%s'", _tokk_to_str(kind));
        }
    } while (kind != -1);
    return cmon_str_builder_c_str(_b);
}

static inline cmon_idx _tok_check_impl(_tokparse * _t, ...)
{
    va_list args, cpy;
    va_start(args, _t);
    va_copy(cpy, args);
    cmon_idx tok = _tokens_accept_impl_v(_t, args);
    if (!cmon_is_valid_idx(tok))
    {
        const char * tk_kinds_str = _token_kinds_to_str(_t->tk_str_builder, cpy);
        va_end(cpy);
        va_end(args);

        cmon_idx cur = _t->tok_idx;
        if (_tok_kind(_t->tokens, cur) != _tokk_eof)
        {
            cmon_str_view name = _tok_str_view(_t, cur);
            _err(_t, "%s expected, got '%.*s'", tk_kinds_str, name.end - name.begin, name.begin);
        }
        else
        {
            _err(_t, "%s expected, got 'EOF'", tk_kinds_str);
        }
    }

    va_end(args);
    return tok;
}

#define _tok_is(_t, _idx, ...)                                                                     \
    _tokens_is_impl_v((_t), _idx, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define _tok_is_current(_t, ...)                                                                   \
    _tokens_is_impl_v((_t), (_t)->tok_idx, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define _tok_accept(_t, ...) _tokens_accept_impl((_t), _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define _tok_check(_t, ...) _tok_check_impl((_t), _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))

static inline cmon_idx _add_node(_tokparse * _t, cmon_tinik _kind, cmon_idx _data)
{
    cmon_dyn_arr_append(&_t->kinds, _kind);
    cmon_dyn_arr_append(&_t->data, _data);
    return cmon_dyn_arr_count(&_t->kinds) - 1;
}

static inline cmon_idx _add_indices(_tokparse * _t, cmon_idx * _indices, size_t _count)
{
    cmon_idx ret = cmon_dyn_arr_count(&_t->idx_buffer);
    for (size_t i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_t->idx_buffer, _indices[i]);
    }
    return ret;
}

static inline cmon_idx _parse_value(_tokparse * _t);

static inline cmon_idx _parse_array(_tokparse * _t)
{
}

static inline cmon_idx _parse_assign(_tokparse * _t)
{
    cmon_idx name_tok = _tok_check(_t, _tokk_name);
    _tok_check(_t, _tokk_assign);
    cmon_idx rhs = _parse_value(_t);
    cmon_dyn_arr_append(&_t->key_value_pairs, ((_key_val){ _tok_str_view(_t, name_tok), rhs }));
    return _add_node(_t, cmon_tinik_assignment, cmon_dyn_arr_count(&_t->key_value_pairs) - 1);
}

static inline cmon_idx _parse_obj(_tokparse * _t)
{
    _tok_check(_t, _tokk_curl_open);

    cmon_idx ib = cmon_idx_buf_mng_get(_t->idx_buf_mng);
    while (!_tok_is_current(_t, _tokk_curl_close, _tokk_eof))
    {
        cmon_idx_buf_mng_append(_t->idx_buf_mng, ib, _parse_assign(_t));
        _tok_accept(_t, _tokk_comma);
    }
    _tok_check(_t, _tokk_curl_close);
    
    cmon_idx begin = _add_indices(_t, cmon_idx_buf_ptr(_t->idx_buf_mng, ib), cmon_idx_buf_count(_t->idx_buf_mng, ib));
    cmon_dyn_arr_append(&_t->objs, ((_obj){begin, cmon_dyn_arr_count(&_t->idx_buffer), NULL}));
    cmon_idx_buf_mng_return(_t->idx_buf_mng, ib);
    return _add_node(_t, cmon_tinik_obj, cmon_dyn_arr_count(&_t->objs) - 1);
}

static inline cmon_idx _parse_value(_tokparse * _t)
{
}

cmon_tini * cmon_tini_parse(cmon_allocator * _alloc, const char * _txt, cmon_err_report * _out_err)
{
    cmon_tini * ret = NULL;
    size_t len = strlen(_txt);

    // tokenize first
    _tokparse tn;
    tn.alloc = _alloc;
    tn.input = _txt;
    tn.pos = _txt;
    tn.end = _txt + len;
    tn.current_line = 1;
    tn.current_line_off = 1;
    cmon_dyn_arr_init(&tn.tokens, _alloc, len / 4);
    tn.tok_idx = 0;
    tn.err = cmon_err_report_make_empty();
    tn.tmp_str_b = cmon_str_builder_create(_alloc, 512);

    _token tok;
    _tokk prev_kind = _tokk_none;
    while (_next_token(&tn, &tok, prev_kind))
    {
        cmon_dyn_arr_append(&tn.tokens, tok);
        if (tok.kind != _tokk_comment)
        {
            prev_kind = tok.kind;
        }
    }
    // append eof token last
    tok.str_view.begin = tn.end;
    tok.str_view.end = tn.end;
    tok.kind = _tokk_eof;
    cmon_dyn_arr_append(&tn.tokens, tok);

    printf("tok count %lu\n", cmon_dyn_arr_count(&tn.tokens));
    // then parse

    // ret = CMON_CREATE(_alloc, cmon_tini);
    // ret->alloc = _alloc;

end:
    cmon_dyn_arr_dealloc(&tn.tokens);
    return ret;
}

cmon_tini * cmon_tini_parse_file(cmon_allocator * _alloc,
                                 const char * _path,
                                 cmon_err_report * _out_err)
{
}

void cmon_tini_destroy(cmon_tini * _t)
{
}

cmon_idx cmon_tini_find(cmon_tini * _t, const char * _key)
{
}

cmon_str_view cmon_tini_key(cmon_tini * _t, cmon_idx _idx)
{
}

cmon_idx cmon_tini_value(cmon_tini * _t, cmon_idx _idx)
{
}

cmon_tinik cmon_tini_kind(cmon_tini * _t, cmon_idx _idx)
{
}

size_t cmon_tini_child_count(cmon_tini * _t, cmon_idx _idx)
{
}

cmon_idx cmon_tini_child(cmon_tini * _t, cmon_idx _idx, cmon_idx _child_idx)
{
}
