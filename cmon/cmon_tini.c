#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
#include <cmon/cmon_idx_buf_mng.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tini.h>
#include <cmon/cmon_util.h>
#include <cmon/cmon_fs.h>
#include <setjmp.h>
#include <stdarg.h>

typedef enum
{
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
} _array_or_obj;

typedef struct
{
    cmon_allocator * alloc;
    const char * input;
    cmon_bool owns_input;
    cmon_dyn_arr(cmon_tinik) kinds;
    cmon_dyn_arr(cmon_idx) data;
    cmon_dyn_arr(cmon_idx) idx_buffer;
    cmon_dyn_arr(cmon_str_view) str_values;
    cmon_dyn_arr(_key_val) key_value_pairs;
    cmon_dyn_arr(_array_or_obj) arr_objs;
} _tini_shared;

// we use one helper object for tokenizing and parsing to keep it simple
typedef struct
{
    char name[CMON_FILENAME_MAX];
    // tokenizing related
    const char * pos;
    const char * end;
    cmon_idx current_line;
    cmon_idx current_line_off;
    // parsing related
    cmon_idx_buf_mng * idx_buf_mng;
    cmon_dyn_arr(_token) tokens;
    cmon_idx tok_idx;
    _tini_shared ts;
    // error
    cmon_tini_err err;
    jmp_buf err_jmp;
    cmon_str_builder * tmp_str_b;
    cmon_str_builder * tk_str_builder;
} _tokparse;

typedef struct cmon_tini
{
    cmon_allocator * alloc;
    // cmon_dyn_arr(cmon_tinik) kinds;
    // cmon_dyn_arr(cmon_idx) data;
    // cmon_dyn_arr(cmon_idx) idx_buffer;
    // cmon_dyn_arr(cmon_str_view) str_values;
    // cmon_dyn_arr(_key_val) key_value_pairs;
    // cmon_dyn_arr(_array_or_obj) arr_objs;
    _tini_shared ts;
    cmon_idx root_obj;
} cmon_tini;

static inline cmon_tini_err _err_make(const char * _file,
                                      size_t _line,
                                      size_t _line_off,
                                      const char * _msg)
{
    cmon_tini_err ret;
    assert(strlen(_file) < CMON_FILENAME_MAX - 1);
    assert(strlen(_msg) < CMON_ERR_MSG_MAX - 1);
    ret.line = _line;
    ret.line_offset = _line_off;
    strcpy(ret.filename, _file);
    strcpy(ret.msg, _msg);
    return ret;
}

static inline cmon_tini_err _err_make_empty()
{
    cmon_tini_err ret;
    memset(&ret, 0, sizeof(ret));
    return ret;
}

static inline cmon_bool _err_is_empty(cmon_tini_err * _err)
{
    return _err->line == 0;
}

#define _err(_tp, _fmt, ...)                                                                       \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(_tp->tmp_str_b);                                                    \
        cmon_str_builder_append_fmt(_tp->tmp_str_b, _fmt, ##__VA_ARGS__);                          \
        _tp->err = _err_make(_tp->name,                                                            \
                             _tp->current_line,                                                    \
                             _tp->current_line_off,                                                \
                             cmon_str_builder_c_str(_tp->tmp_str_b));                              \
        longjmp(_tp->err_jmp, 1);                                                                  \
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
        // if (_prev_kind == _tokk_assign || _prev_kind == _tokk_comma ||
        //     _prev_kind == _tokk_square_open)
        // {
        // if this is an explicit string/multiline string
        if (*_t->pos == '"')
        {
            _advance_pos(_t, 1);
            _out_tok->str_view.begin = _t->pos;
            while (*_t->pos != '"' && _t->pos != _t->end)
            {
                if (*_t->pos == '\n')
                {
                    _advance_line(_t);
                }
                _advance_pos(_t, 1);
            }

            // skip closing "
            _out_tok->str_view.end = _t->pos;
            _advance_pos(_t, 1);
            _out_tok->kind = _tokk_string;
            return cmon_true;
        }
        else
        {
            while (*_t->pos != ',' && *_t->pos != ']' && *_t->pos != '}' && *_t->pos != '=' &&
                   !isspace(*_t->pos) && _t->pos != _t->end)
                _advance_pos(_t, 1);
        }

        _out_tok->kind = _tokk_string;
        // }
        // else
        // {
        //     if (isalpha(*_t->pos) || *_t->pos == '_')
        //     {
        //         while ((isalnum(*_t->pos) || *_t->pos == '_') && _t->pos != _t->end)
        //             _advance_pos(_t, 1);
        //     }
        //     else
        //     {
        //         //@TODO: Error, bad name
        //         assert(0);
        //     }
        //     _out_tok->kind = _tokk_name;
        // }
        _out_tok->str_view.end = _t->pos;
        return cmon_true;
    }
    return cmon_false;
}

// parsing related functions
static inline cmon_idx _inline_next(_tokparse * _t, cmon_bool _skip_comments)
{
    size_t ret = _t->tok_idx;
    while (ret < cmon_dyn_arr_count(&_t->tokens))
    {
        ++ret;
        if (!_skip_comments || _t->tokens[ret].kind != _tokk_comment)
            return ret;
    }

    return CMON_INVALID_IDX;
}

static inline cmon_idx _token_advance(_tokparse * _t, cmon_bool _skip_comments)
{
    cmon_idx ret, idx;
    ret = _t->tok_idx;
    idx = _inline_next(_t, _skip_comments);
    _t->tok_idx = idx != CMON_INVALID_IDX ? idx : _t->tok_idx;
    return ret;
}

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

static inline cmon_bool _is_valid_identifier(_tokparse * _t, cmon_idx _tok_idx)
{
    cmon_str_view sv = _tok_str_view(_t, _tok_idx);
    char first = *sv.begin;
    if (!isalpha(first) && first != '_')
        return cmon_false;

    const char * pos = sv.begin;
    while (pos != sv.end)
    {
        if (!isalnum(*pos) && *pos != '_')
            return cmon_false;

        ++pos;
    }

    return cmon_true;
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
    va_start(args, _idx);
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
    default:
        return "none";
    }
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
        if (_tok_kind(_t, cur) != _tokk_eof)
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
    _tokens_is_impl((_t), _idx, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define _tok_is_current(_t, ...)                                                                   \
    _tokens_is_impl((_t), (_t)->tok_idx, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define _tok_accept(_t, ...) _tokens_accept_impl((_t), _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define _tok_accept_ot(_t, _out_tok, ...)                                                          \
    (cmon_is_valid_idx(*(_out_tok) = _tokens_accept_impl((_t), __VA_ARGS__)))
#define _tok_check(_t, ...) _tok_check_impl((_t), _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))

static inline cmon_idx _add_node(_tokparse * _t, cmon_tinik _kind, cmon_idx _data)
{
    cmon_dyn_arr_append(&_t->ts.kinds, _kind);
    cmon_dyn_arr_append(&_t->ts.data, _data);
    return cmon_dyn_arr_count(&_t->ts.kinds) - 1;
}

static inline cmon_idx _add_indices(_tokparse * _t, cmon_idx * _indices, size_t _count)
{
    cmon_idx ret = cmon_dyn_arr_count(&_t->ts.idx_buffer);
    for (size_t i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_t->ts.idx_buffer, _indices[i]);
    }
    return ret;
}

static inline cmon_idx _parse_value(_tokparse * _t);

static inline cmon_idx _parse_array(_tokparse * _t)
{
    _tok_check(_t, _tokk_square_open);

    cmon_idx ib = cmon_idx_buf_mng_get(_t->idx_buf_mng);
    while (!_tok_is_current(_t, _tokk_square_close, _tokk_eof))
    {
        cmon_idx_buf_append(_t->idx_buf_mng, ib, _parse_value(_t));
        _tok_accept(_t, _tokk_comma);
    }
    _tok_check(_t, _tokk_square_close);

    cmon_idx begin = _add_indices(
        _t, cmon_idx_buf_ptr(_t->idx_buf_mng, ib), cmon_idx_buf_count(_t->idx_buf_mng, ib));
    cmon_dyn_arr_append(&_t->ts.arr_objs,
                        ((_array_or_obj){ begin, cmon_dyn_arr_count(&_t->ts.idx_buffer) }));
    cmon_idx_buf_mng_return(_t->idx_buf_mng, ib);
    return _add_node(_t, cmon_tinik_array, cmon_dyn_arr_count(&_t->ts.arr_objs) - 1);
}

static inline cmon_idx _parse_assign(_tokparse * _t)
{
    cmon_idx name_tok = _tok_check(_t, _tokk_string);
    // if (!_is_valid_identifier(_t, name_tok))
    // {
    //     cmon_str_view sv = _tok_str_view(_t, name_tok);
    //     _err(_t, "invalid identifier '%.*s'", sv.end - sv.begin, sv.begin);
    // }
    _tok_check(_t, _tokk_assign);
    cmon_idx rhs = _parse_value(_t);
    cmon_dyn_arr_append(&_t->ts.key_value_pairs, ((_key_val){ _tok_str_view(_t, name_tok), rhs }));
    return _add_node(_t, cmon_tinik_pair, cmon_dyn_arr_count(&_t->ts.key_value_pairs) - 1);
}

static inline cmon_idx _parse_obj(_tokparse * _t, cmon_bool _is_root_obj)
{
    if (!_is_root_obj)
    {
        _tok_check(_t, _tokk_curl_open);
    }

    cmon_idx ib = cmon_idx_buf_mng_get(_t->idx_buf_mng);
    while (!_tok_is_current(_t, _tokk_curl_close, _tokk_eof))
    {
        cmon_idx_buf_append(_t->idx_buf_mng, ib, _parse_assign(_t));
        _tok_accept(_t, _tokk_comma);
    }
    if (!_is_root_obj)
    {
        _tok_check(_t, _tokk_curl_close);
    }

    cmon_idx begin = _add_indices(
        _t, cmon_idx_buf_ptr(_t->idx_buf_mng, ib), cmon_idx_buf_count(_t->idx_buf_mng, ib));
    cmon_dyn_arr_append(&_t->ts.arr_objs,
                        ((_array_or_obj){ begin, cmon_dyn_arr_count(&_t->ts.idx_buffer) }));
    cmon_idx_buf_mng_return(_t->idx_buf_mng, ib);
    return _add_node(_t, cmon_tinik_obj, cmon_dyn_arr_count(&_t->ts.arr_objs) - 1);
}

static inline cmon_idx _parse_value(_tokparse * _t)
{
    cmon_idx tok;
    if (_tok_accept_ot(_t, &tok, _tokk_string))
    {
        cmon_dyn_arr_append(&_t->ts.str_values, _tok_str_view(_t, tok));
        return _add_node(_t, cmon_tinik_string, cmon_dyn_arr_count(&_t->ts.str_values) - 1);
    }
    else if (_tok_is_current(_t, _tokk_square_open))
    {
        return _parse_array(_t);
    }
    else if (_tok_is_current(_t, _tokk_curl_open))
    {
        return _parse_obj(_t, cmon_false);
    }

    _err(_t,
         "unexpected token '%s', string, object or array expected.",
         _tokk_to_str(_tok_kind(_t, _t->tok_idx)));
    return CMON_INVALID_IDX;
}

static inline void _destroy_shared_data(_tini_shared * _ts)
{
    if(_ts->owns_input)
    {
        cmon_c_str_free(_ts->alloc, _ts->input);
    }
    cmon_dyn_arr_dealloc(&_ts->arr_objs);
    cmon_dyn_arr_dealloc(&_ts->key_value_pairs);
    cmon_dyn_arr_dealloc(&_ts->str_values);
    cmon_dyn_arr_dealloc(&_ts->idx_buffer);
    cmon_dyn_arr_dealloc(&_ts->data);
    cmon_dyn_arr_dealloc(&_ts->kinds);
}

cmon_tini * cmon_tini_parse(cmon_allocator * _alloc,
                            const char * _name,
                            const char * _input,
                            cmon_bool _owns_input,
                            cmon_tini_err * _out_err)
{
    cmon_tini * ret = NULL;
    size_t len = strlen(_input);

    // tokenize first
    _tokparse tn;
    strcpy(tn.name, _name);
    tn.ts.alloc = _alloc;
    tn.ts.input = _input;
    tn.ts.owns_input = _owns_input;
    tn.pos = _input;
    tn.end = _input + len;
    tn.current_line = 1;
    tn.current_line_off = 1;
    tn.idx_buf_mng = cmon_idx_buf_mng_create(_alloc);
    cmon_dyn_arr_init(&tn.tokens, _alloc, len / 4);
    tn.tok_idx = 0;
    cmon_dyn_arr_init(&tn.ts.kinds, _alloc, len / 4);
    cmon_dyn_arr_init(&tn.ts.data, _alloc, len / 4);
    cmon_dyn_arr_init(&tn.ts.idx_buffer, _alloc, len / 4);
    cmon_dyn_arr_init(&tn.ts.str_values, _alloc, len / 8);
    cmon_dyn_arr_init(&tn.ts.key_value_pairs, _alloc, len / 8);
    cmon_dyn_arr_init(&tn.ts.arr_objs, _alloc, len / 4);
    tn.err = _err_make_empty();
    tn.tmp_str_b = cmon_str_builder_create(_alloc, 512);
    tn.tk_str_builder = cmon_str_builder_create(_alloc, 256);

    if (setjmp(tn.err_jmp))
    {
        goto end;
    }

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

    cmon_idx root_obj = _parse_obj(&tn, cmon_true);

    ret = CMON_CREATE(_alloc, cmon_tini);
    ret->alloc = _alloc;
    ret->ts = tn.ts;
    ret->root_obj = root_obj;

end:
    *_out_err = tn.err;
    if (!_err_is_empty(&tn.err))
    {
        _destroy_shared_data(&tn.ts);
    }
    cmon_str_builder_destroy(tn.tk_str_builder);
    cmon_str_builder_destroy(tn.tmp_str_b);
    cmon_dyn_arr_dealloc(&tn.tokens);
    cmon_idx_buf_mng_destroy(tn.idx_buf_mng);
    return ret;
}

cmon_tini * cmon_tini_parse_file(cmon_allocator * _alloc,
                                 const char * _path,
                                 cmon_tini_err * _out_err)
{
    char fname[CMON_FILENAME_MAX];
    cmon_filename(_path, fname, sizeof(fname));
    char * txt = cmon_fs_load_txt_file(_alloc, _path);
    if(!txt)
    {
        cmon_tini_err err;
        strcpy(err.filename, fname);
        strcpy(err.msg, "could not load tini file");
        *_out_err = err;
        return NULL;
    }
    return cmon_tini_parse(_alloc, fname, txt, cmon_true, _out_err);
}

void cmon_tini_destroy(cmon_tini * _t)
{
    if (!_t)
        return;

    _destroy_shared_data(&_t->ts);
    CMON_DESTROY(_t->alloc, _t);
}

cmon_tinik cmon_tini_kind(cmon_tini * _t, cmon_idx _idx)
{
    return _t->ts.kinds[_idx];
}

cmon_idx cmon_tini_root_obj(cmon_tini * _t)
{
    return _t->root_obj;
}

static inline cmon_tinik _get_tini_kind(cmon_tini * _t, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_t->ts.kinds));
    return _t->ts.kinds[_idx];
}

static inline cmon_idx _get_data(cmon_tini * _t, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_t->ts.data));
    return _t->ts.data[_idx];
}

static inline _array_or_obj * _get_array_obj(cmon_tini * _t, cmon_idx _idx)
{
    assert(_get_tini_kind(_t, _idx) == cmon_tinik_array ||
           _get_tini_kind(_t, _idx) == cmon_tinik_obj);
    cmon_idx data_idx = _get_data(_t, _idx);
    assert(data_idx < cmon_dyn_arr_count(&_t->ts.arr_objs));
    return &_t->ts.arr_objs[data_idx];
}

static inline _key_val * _get_pair(cmon_tini * _t, cmon_idx _idx)
{
    assert(_get_tini_kind(_t, _idx) == cmon_tinik_pair);
    cmon_idx data_idx = _get_data(_t, _idx);
    assert(data_idx < cmon_dyn_arr_count(&_t->ts.key_value_pairs));
    return &_t->ts.key_value_pairs[data_idx];
}

static inline cmon_idx _get_idx(cmon_tini * _t, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_t->ts.idx_buffer));
    return _t->ts.idx_buffer[_idx];
}

static inline cmon_str_view _get_str_view(cmon_tini * _t, cmon_idx _idx)
{
    assert(_get_tini_kind(_t, _idx) == cmon_tinik_string);
    cmon_idx data_idx = _get_data(_t, _idx);
    assert(data_idx < cmon_dyn_arr_count(&_t->ts.str_values));
    return _t->ts.str_values[data_idx];
}

cmon_idx cmon_tini_obj_find(cmon_tini * _t, cmon_idx _idx, const char * _key)
{
    assert(_get_tini_kind(_t, _idx) == cmon_tinik_obj);
    for (size_t i = 0; i < cmon_tini_child_count(_t, _idx); ++i)
    {
        cmon_idx child = cmon_tini_child(_t, _idx, i);
        if (cmon_str_view_c_str_cmp(cmon_tini_pair_key(_t, child), _key) == 0)
        {
            return cmon_tini_pair_value(_t, child);
        }
    }
    return CMON_INVALID_IDX;
}

size_t cmon_tini_child_count(cmon_tini * _t, cmon_idx _idx)
{
    _array_or_obj * o = _get_array_obj(_t, _idx);
    return o->children_end - o->children_begin;
}

cmon_idx cmon_tini_child(cmon_tini * _t, cmon_idx _idx, size_t _child_idx)
{
    _array_or_obj * o = _get_array_obj(_t, _idx);
    return _get_idx(_t, o->children_begin + _child_idx);
}

cmon_str_view cmon_tini_pair_key(cmon_tini * _t, cmon_idx _idx)
{
    return _get_pair(_t, _idx)->key;
}

cmon_idx cmon_tini_pair_value(cmon_tini * _t, cmon_idx _idx)
{
    return _get_pair(_t, _idx)->val_idx;
}

cmon_str_view cmon_tini_string(cmon_tini * _t, cmon_idx _idx)
{
    return _get_str_view(_t, _idx);
}
