#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
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

typedef struct
{
    cmon_allocator * alloc;
    const char * input;
    const char * pos;
    const char * end;
    cmon_idx current_line;
    cmon_idx current_line_off;
    cmon_err_report err;
    cmon_str_builder * tmp_str_b;
} _tokenizer;

typedef struct cmon_tini
{
    cmon_allocator * alloc;
    cmon_dyn_arr(_token) tokens;
    cmon_dyn_arr(cmon_tinik) kinds;
    cmon_dyn_arr(cmon_idx) data;
    cmon_dyn_arr(cmon_str_view) str_values;
    cmon_dyn_arr(_key_val) key_value_pairs;
    cmon_dyn_arr(_array) arrays;
    cmon_dyn_arr(_obj) objs;
} cmon_tini;

static inline cmon_idx _add_node(cmon_tini * _t, cmon_tinik _kind, cmon_idx _data)
{
    cmon_dyn_arr_append(&_t->kinds, _kind);
    cmon_dyn_arr_append(&_t->data, _data);
    return cmon_dyn_arr_count(&_t->kinds) - 1;
}

static inline _tokk _tok_kind(cmon_tini * _t, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_t->tokens));
    return _t->tokens[_idx].kind;
}

static inline void _advance_pos(_tokenizer * _t, size_t _advance)
{
    _t->pos += _advance;
    _t->current_line_off += _advance;
}

static inline void _advance_line(_tokenizer * _t)
{
    ++_t->current_line;
    _t->current_line_off = 0;
}

static inline size_t _skip_whitespace(_tokenizer * _t)
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

static inline cmon_bool _finalize_tok(_tokenizer * _t,
                                      _tokk _kind,
                                      size_t _advance,
                                      _token * _out_tok)
{
    _advance_pos(_t, _advance);
    _out_tok->str_view.end = _t->pos;
    _out_tok->kind = _kind;
    return cmon_true;
}

static cmon_bool _next_token(_tokenizer * _t, _token * _out_tok, _tokk _prev_kind)
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
        if (_prev_kind == _tokk_assign)
        {
            // if this is an explicit string/multiline string
            if (*_t->pos == '"')
            {
                _advance_pos(_t, 1);
                while (*_t->pos != '"' && _t->pos != _t->end)
                    _advance_pos(_t, 1);

                //skip closing "
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

static inline cmon_bool _is_impl_v(cmon_tini * _t, cmon_idx _idx, va_list _args)
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

static inline cmon_bool _tokens_is_impl(cmon_tini * _t, cmon_idx _idx, ...)
{
}

static inline cmon_idx _tokens_accept_impl(cmon_tini * _t, ...)
{
}

cmon_tini * cmon_tini_parse(cmon_allocator * _alloc, const char * _txt, cmon_err_report * _out_err)
{
    cmon_tini * ret = NULL;
    size_t len = strlen(_txt);
    cmon_dyn_arr(_token) tokens;
    cmon_dyn_arr_init(&tokens, _alloc, len / 4);

    // tokenize first
    _tokenizer tn;
    tn.alloc = _alloc;
    tn.input = _txt;
    tn.pos = _txt;
    tn.end = _txt + len;
    tn.current_line = 1;
    tn.current_line_off = 1;
    tn.err = cmon_err_report_make_empty();
    tn.tmp_str_b = cmon_str_builder_create(_alloc, 512);
    _token tok;

    _tokk prev_kind = _tokk_none;
    while (_next_token(&tn, &tok, prev_kind))
    {
        cmon_dyn_arr_append(&tokens, tok);
        if(tok.kind != _tokk_comment)
        {
            prev_kind = tok.kind;
        }
    }

    printf("tok count %lu\n", cmon_dyn_arr_count(&tokens));
    // then parse

    // ret = CMON_CREATE(_alloc, cmon_tini);
    // ret->alloc = _alloc;

end:
    cmon_dyn_arr_dealloc(&tokens);
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
