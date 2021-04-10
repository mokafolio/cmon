#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_types.h>

typedef struct
{
    size_t name_str_off;
    cmon_idx type;
    cmon_idx def_expr;
} _struct_field;

typedef struct
{
    cmon_dyn_arr(_struct_field) fields;
} _struct;

typedef struct
{
    cmon_dyn_arr(cmon_idx) params;
    cmon_idx return_type;
} _fn_sig;

typedef struct
{
    cmon_bool is_mut;
    cmon_idx type;
} _ptr;

typedef struct
{
    cmon_typek kind;
    // if it's a none builtin type, src_file_idx and name_tok will be set.
    cmon_idx src_file_idx;
    cmon_idx name_tok;
    size_t name_str_off;
    size_t full_name_str_off;
    size_t unique_name_str_off;
    // data idx for additional type information that gets interpreted based on kind.
    // i.e. for structs this is the index into the structs array.
    cmon_idx data_idx;
} _type;

typedef struct cmon_types
{
    cmon_allocator * alloc;
    cmon_modules * mods;
    cmon_dyn_arr(_struct) structs;
    cmon_dyn_arr(_fn_sig) fns;
    cmon_dyn_arr(_ptr) ptrs;
    cmon_dyn_arr(_type) types;
    cmon_hashmap(const char *, cmon_idx) name_map;
    cmon_str_builder * str_builder;
    cmon_str_buf * str_buf;

    // builtin type indices
    cmon_idx builtin_s8;
    cmon_idx builtin_s16;
    cmon_idx builtin_s32;
    cmon_idx builtin_s64;
    cmon_idx builtin_u8;
    cmon_idx builtin_u16;
    cmon_idx builtin_u32;
    cmon_idx builtin_u64;
    cmon_idx builtin_f32;
    cmon_idx builtin_f64;
    cmon_idx builtin_void;
    cmon_idx builtin_bool;
    cmon_idx builtin_u8_view;
} cmon_types;

#define _tmp_str(_t, _fmt, ...)                                                                    \
    (cmon_str_builder_clear(_t->str_builder),                                                      \
     cmon_str_builder_append_fmt(_t->str_builder, _fmt, ##__VA_ARGS__),                            \
     cmon_str_builder_c_str(_t->str_builder))

#define _intern_str(_t, _fmt, ...)                                                                 \
    cmon_str_buf_append(_t->str_buf, _tmp_str(_t, _fmt, ##__VA_ARGS__))

#define _get_type(_t, _idx) (assert(_idx < cmon_dyn_arr_count(&_t->types)), _t->types[_idx])

#define _return_if_found(_t, _name)                                                                \
    do                                                                                             \
    {                                                                                              \
        cmon_idx _idx;                                                                             \
        if (cmon_is_valid_idx(_idx = cmon_types_find(_t, _name)))                                  \
        {                                                                                          \
            return _idx;                                                                           \
        }                                                                                          \
    } while (0)

static inline cmon_idx _add_type(cmon_types * _t,
                                 cmon_typek _kind,
                                 size_t _name_off,
                                 size_t _unique_off,
                                 size_t _full_off,
                                 cmon_idx _src_file_idx,
                                 cmon_idx _name_tok,
                                 cmon_idx _extra_data)
{
    _type t;
    t.kind = _kind;
    t.src_file_idx = _src_file_idx;
    t.name_tok = _name_tok;
    t.name_str_off = _name_off;
    t.unique_name_str_off = _unique_off;
    t.full_name_str_off = _full_off;
    t.data_idx = _extra_data;
    cmon_dyn_arr_append(&_t->types, t);
    cmon_hashmap_set(
        &_t->name_map, cmon_str_buf_get(_t->str_buf, _unique_off), cmon_dyn_arr_count(&_t->types));
    return cmon_dyn_arr_count(&_t->types) - 1;
}

static inline cmon_idx _add_builtin(cmon_types * _t, cmon_typek _kind, const char * _name)
{
    size_t name_off = _intern_str(_t, _name);
    return _add_type(_t,
                     _kind,
                     name_off,
                     name_off,
                     name_off,
                     CMON_INVALID_IDX,
                     CMON_INVALID_IDX,
                     CMON_INVALID_IDX);
}

cmon_types * cmon_types_create(cmon_allocator * _alloc, cmon_modules * _mods)
{
    cmon_types * ret = CMON_CREATE(_alloc, cmon_types);
    ret->alloc = _alloc;
    ret->mods = _mods;
    cmon_dyn_arr_init(&ret->structs, _alloc, 32);
    cmon_dyn_arr_init(&ret->fns, _alloc, 16);
    cmon_dyn_arr_init(&ret->ptrs, _alloc, 16);
    cmon_dyn_arr_init(&ret->types, _alloc, 64);
    cmon_hashmap_str_key_init(&ret->name_map, _alloc);
    ret->str_builder = cmon_str_builder_create(_alloc, 256);
    ret->str_buf = cmon_str_buf_create(_alloc, 256);

    ret->builtin_s8 = _add_builtin(ret, cmon_typek_s8, "s8");
    ret->builtin_s16 = _add_builtin(ret, cmon_typek_s16, "s16");
    ret->builtin_s32 = _add_builtin(ret, cmon_typek_s32, "s32");
    ret->builtin_s64 = _add_builtin(ret, cmon_typek_s64, "s64");
    ret->builtin_u8 = _add_builtin(ret, cmon_typek_u8, "u8");
    ret->builtin_u16 = _add_builtin(ret, cmon_typek_u16, "u16");
    ret->builtin_u32 = _add_builtin(ret, cmon_typek_u32, "u32");
    ret->builtin_u64 = _add_builtin(ret, cmon_typek_u64, "u64");
    ret->builtin_f32 = _add_builtin(ret, cmon_typek_f32, "f32");
    ret->builtin_f64 = _add_builtin(ret, cmon_typek_f64, "f64");
    ret->builtin_void = _add_builtin(ret, cmon_typek_void, "void");
    ret->builtin_bool = _add_builtin(ret, cmon_typek_bool, "bool");
    // ret->builtin_u8_view;

    return ret;
}

void cmon_types_destroy(cmon_types * _t)
{
    size_t i;
    cmon_str_buf_destroy(_t->str_buf);
    cmon_str_builder_destroy(_t->str_builder);
    cmon_hashmap_dealloc(&_t->name_map);
    cmon_dyn_arr_dealloc(&_t->types);
    cmon_dyn_arr_dealloc(&_t->ptrs);
    for (i = 0; i < cmon_dyn_arr_count(&_t->fns); ++i)
    {
        cmon_dyn_arr_dealloc(&_t->fns[i].params);
    }
    cmon_dyn_arr_dealloc(&_t->fns);
    for (i = 0; i < cmon_dyn_arr_count(&_t->structs); ++i)
    {
        cmon_dyn_arr_dealloc(&_t->structs[i].fields);
    }
    cmon_dyn_arr_dealloc(&_t->structs);
}

cmon_idx cmon_types_add_struct(
    cmon_types * _t, cmon_idx _mod, cmon_str_view _name, cmon_idx _src_file_idx, cmon_idx _name_tok)
{
    _struct strct;
    cmon_dyn_arr_init(&strct.fields, _t->alloc, 8);
    cmon_dyn_arr_append(&_t->structs, strct);
    return _add_type(
        _t,
        cmon_typek_struct,
        _intern_str(_t, "%.*s", _name.begin, _name.end - _name.begin),
        _intern_str(_t,
                    "%s_%.*s",
                    cmon_modules_prefix(_t->mods, _mod),
                    _name.begin,
                    _name.end - _name.begin),
        _intern_str(
            _t, "%s.%.*s", cmon_modules_name(_t->mods, _mod), _name.begin, _name.end - _name.begin),
        _src_file_idx,
        _name_tok,
        cmon_dyn_arr_count(&_t->structs) - 1);
}

cmon_idx cmon_types_struct_add_field(
    cmon_types * _t, cmon_idx _struct, cmon_str_view _name, cmon_idx _type, cmon_idx _def_expr_ast)
{
    assert(_struct < cmon_dyn_arr_count(&_t->structs));
    cmon_dyn_arr_append(
        &_t->structs[_struct].fields,
        ((_struct_field){
            _intern_str(_t, "%.*s", _name.begin, _name.end - _name.begin), _type, _def_expr_ast }));
    return cmon_dyn_arr_count(&_t->structs[_struct].fields) - 1;
}

cmon_idx cmon_types_find_ptr(cmon_types * _t, cmon_idx _type, cmon_bool _is_mut)
{
    _ptr ptr;
    const char * unique_name;
    unique_name = _tmp_str(_t, "Ptr%s_%s", _is_mut ? "Mut" : "", cmon_types_unique_name(_t, _type));
    _return_if_found(_t, unique_name);

    ptr.is_mut = _is_mut;
    ptr.type = _type;
    cmon_dyn_arr_append(&_t->ptrs, ptr);

    return _add_type(
        _t,
        cmon_typek_ptr,
        _intern_str(_t, "*%s %s", _is_mut ? "mut" : "", cmon_types_name(_t, _type)),
        cmon_str_buf_append(_t->str_buf, unique_name),
        _intern_str(_t, "*%s %s", _is_mut ? "mut" : "", cmon_types_full_name(_t, _type)),
        CMON_INVALID_IDX,
        CMON_INVALID_IDX,
        cmon_dyn_arr_count(&_t->ptrs) - 1);
}

static inline const char * _fn_name(cmon_types * _t,
                                    cmon_idx _ret_type,
                                    cmon_idx * _params,
                                    size_t _param_count,
                                    const char * (*_name_fn)(cmon_types *, cmon_idx))
{
    size_t i;
    cmon_str_builder_clear(_t->str_builder);
    cmon_str_builder_append(_t->str_builder, "fn(");
    for (i = 0; i < _param_count; ++i)
    {
        cmon_str_builder_append(_t->str_builder, _name_fn(_t, _params[i]));
        if (i < _param_count - 1)
            cmon_str_builder_append(_t->str_builder, ", ");
    }
    cmon_str_builder_append_fmt(_t->str_builder, ")->%s", _name_fn(_t, _ret_type));
    return cmon_str_builder_c_str(_t->str_builder);
}

cmon_idx cmon_types_find_fn(cmon_types * _t,
                            cmon_idx _ret_type,
                            cmon_idx * _params,
                            size_t _param_count)
{
    _fn_sig sig;
    const char * unique_name;

    unique_name = _fn_name(_t, _ret_type, _params, _param_count, cmon_types_unique_name);
    _return_if_found(_t, unique_name);

    cmon_dyn_arr_init(&sig.params, _t->alloc, 8);
    cmon_dyn_arr_append(&_t->fns, sig);

    return _add_type(
        _t,
        cmon_typek_fn,
        cmon_str_buf_append(_t->str_buf,
                            _fn_name(_t, _ret_type, _params, _param_count, cmon_types_name)),
        cmon_str_buf_append(_t->str_buf, unique_name),
        cmon_str_buf_append(_t->str_buf,
                            _fn_name(_t, _ret_type, _params, _param_count, cmon_types_full_name)),
        CMON_INVALID_IDX,
        CMON_INVALID_IDX,
        cmon_dyn_arr_count(&_t->fns) - 1);
}

cmon_idx cmon_types_find(cmon_types * _t, const char * _unique_name)
{
    cmon_idx * idx_ptr;
    if ((idx_ptr = cmon_hashmap_get(&_t->name_map, _unique_name)))
        return *idx_ptr;
    return CMON_INVALID_IDX;
}

inline const char * _get_str(cmon_types * _t, cmon_idx _str_idx)
{
    return cmon_str_buf_get(_t->str_buf, _str_idx);
}

const char * cmon_types_unique_name(cmon_types * _t, cmon_idx _type_idx)
{
    return cmon_str_buf_get(_t->str_buf, _get_type(_t, _type_idx).unique_name_str_off);
}

const char * cmon_types_name(cmon_types * _t, cmon_idx _type_idx)
{
    return cmon_str_buf_get(_t->str_buf, _get_type(_t, _type_idx).name_str_off);
}

const char * cmon_types_full_name(cmon_types * _t, cmon_idx _type_idx)
{
    return cmon_str_buf_get(_t->str_buf, _get_type(_t, _type_idx).full_name_str_off);
}

cmon_typek cmon_types_kind(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).kind;
}

cmon_idx cmon_types_src_file(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).src_file_idx;
}

cmon_idx cmon_types_name_tok(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).name_tok;
}

static inline _struct_field * _get_struct_field(cmon_types * _t,
                                                cmon_idx _struct_idx,
                                                cmon_idx _field_idx)
{
    assert(_get_type(_t, _struct_idx).kind == cmon_typek_struct);
    assert(_t->types[_struct_idx].data_idx < cmon_dyn_arr_count(&_t->structs));
    assert(_field_idx < cmon_dyn_arr_count(&_t->structs[_t->types[_struct_idx].data_idx].fields));
    return &_t->structs[_t->types[_struct_idx].data_idx].fields[_field_idx];
}

cmon_idx cmon_types_struct_field_count(cmon_types * _t, cmon_idx _struct_idx)
{
    assert(_get_type(_t, _struct_idx).kind == cmon_typek_struct);
    assert(_t->types[_struct_idx].data_idx < cmon_dyn_arr_count(&_t->structs));
    return cmon_dyn_arr_count(&_t->structs[_t->types[_struct_idx].data_idx].fields);
}

const char * cmon_types_struct_field_name(cmon_types * _t,
                                          cmon_idx _struct_idx,
                                          cmon_idx _field_idx)
{
    return cmon_str_buf_get(_t->str_buf,
                            _get_struct_field(_t, _struct_idx, _field_idx)->name_str_off);
}

cmon_idx cmon_types_struct_field_type(cmon_types * _t, cmon_idx _struct_idx, cmon_idx _field_idx)
{
    return _get_struct_field(_t, _struct_idx, _field_idx)->type;
}

cmon_idx cmon_types_struct_field_def_expr(cmon_types * _t,
                                          cmon_idx _struct_idx,
                                          cmon_idx _field_idx)
{
    return _get_struct_field(_t, _struct_idx, _field_idx)->def_expr;
}

cmon_bool cmon_types_ptr_is_mut(cmon_types * _t, cmon_idx _ptr_idx)
{
    assert(_get_type(_t, _ptr_idx).kind == cmon_typek_ptr);
    return _t->ptrs[_t->types[_ptr_idx].data_idx].is_mut;
}

cmon_idx cmon_types_ptr_type(cmon_types * _t, cmon_idx _ptr_idx)
{
    assert(_get_type(_t, _ptr_idx).kind == cmon_typek_ptr);
    return _t->ptrs[_t->types[_ptr_idx].data_idx].type; 
}

cmon_idx cmon_types_fn_param_count(cmon_types * _t, cmon_idx _fn_idx)
{
    assert(_get_type(_t, _fn_idx).kind == cmon_typek_fn);
    assert(_t->types[_fn_idx].data_idx < cmon_dyn_arr_count(&_t->fns));
    return cmon_dyn_arr_count(&_t->fns[_t->types[_fn_idx].data_idx].params);
}

cmon_idx cmon_types_fn_param(cmon_types * _t, cmon_idx _fn_idx, cmon_idx _param_idx)
{
    assert(_get_type(_t, _fn_idx).kind == cmon_typek_fn);
    assert(_t->types[_fn_idx].data_idx < cmon_dyn_arr_count(&_t->fns));
    assert(_param_idx < cmon_dyn_arr_count(&_t->fns[_t->types[_fn_idx].data_idx].params));
    return _t->fns[_t->types[_fn_idx].data_idx].params[_param_idx];
}

cmon_idx cmon_types_builtin_s8(cmon_types * _t)
{
    return _t->builtin_s8;
}

cmon_idx cmon_types_builtin_s16(cmon_types * _t)
{
    return _t->builtin_s16;
}

cmon_idx cmon_types_builtin_s32(cmon_types * _t)
{
    return _t->builtin_s32;
}

cmon_idx cmon_types_builtin_s64(cmon_types * _t)
{
    return _t->builtin_s64;
}

cmon_idx cmon_types_builtin_u8(cmon_types * _t)
{
    return _t->builtin_u8;
}

cmon_idx cmon_types_builtin_u16(cmon_types * _t)
{
    return _t->builtin_u16;
}

cmon_idx cmon_types_builtin_u32(cmon_types * _t)
{
    return _t->builtin_u32;
}

cmon_idx cmon_types_builtin_u64(cmon_types * _t)
{
    return _t->builtin_u64;
}

cmon_idx cmon_types_builtin_f32(cmon_types * _t)
{
    return _t->builtin_f32;
}

cmon_idx cmon_types_builtin_f64(cmon_types * _t)
{
    return _t->builtin_f64;
}

cmon_idx cmon_types_builtin_void(cmon_types * _t)
{
    return _t->builtin_void;
}

cmon_idx cmon_types_builtin_bool(cmon_types * _t)
{
    return _t->builtin_bool;
}

cmon_idx cmon_types_builtin_u8_view(cmon_types * _t)
{
    return _t->builtin_u8_view;
}

cmon_bool cmon_types_is_unsigned_int(cmon_types * _t, cmon_idx _idx)
{
    cmon_typek kind = cmon_types_kind(_t, _idx);
    return kind == cmon_typek_u8 || kind == cmon_typek_u16 || kind == cmon_typek_u32 ||
           kind == cmon_typek_u64;
}

cmon_bool cmon_types_is_signed_int(cmon_types * _t, cmon_idx _idx)
{
    cmon_typek kind = cmon_types_kind(_t, _idx);
    return kind == cmon_typek_s8 || kind == cmon_typek_s16 || kind == cmon_typek_s32 ||
           kind == cmon_typek_s64;
}

cmon_bool cmon_types_is_int(cmon_types * _t, cmon_idx _idx)
{
    return cmon_types_is_unsigned_int(_t, _idx) || cmon_types_is_signed_int(_t, _idx);
}
