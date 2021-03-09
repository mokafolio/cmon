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
    cmon_type_kind kind;
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
    cmon_dyn_arr(_type) types;
    cmon_hashmap(const char *, cmon_idx) name_map;
    cmon_str_builder * str_builder;
    cmon_str_buf * str_buf;
} cmon_types;

cmon_types * cmon_types_create(cmon_allocator * _alloc, cmon_modules * _mods)
{
    cmon_types * ret = CMON_CREATE(_alloc, cmon_types);
    ret->alloc = _alloc;
    ret->mods = _mods;
    cmon_dyn_arr_init(&ret->structs, _alloc, 32);
    cmon_dyn_arr_init(&ret->types, _alloc, 64);
    cmon_hashmap_str_key_init(&ret->name_map, _alloc);
    ret->str_builder = cmon_str_builder_create(_alloc, 256);
    ret->str_buf = cmon_str_buf_create(_alloc, 256);
    return ret;
}

void cmon_types_destroy(cmon_types * _t)
{
    size_t i;
    cmon_str_buf_destroy(_t->str_buf);
    cmon_str_builder_destroy(_t->str_builder);
    cmon_hashmap_dealloc(&_t->name_map);
    cmon_dyn_arr_dealloc(&_t->types);
    for (i = 0; i < cmon_dyn_arr_count(&_t->structs); ++i)
    {
        cmon_dyn_arr_dealloc(&_t->structs[i].fields);
    }
    cmon_dyn_arr_dealloc(&_t->structs);
}

#define _tmp_str(_t, _fmt, ...)                                                                    \
    (cmon_str_builder_clear(_t->str_builder),                                                      \
     cmon_str_builder_append_fmt(_t->str_builder, _fmt, __VA_ARGS__),                              \
     cmon_str_builder_c_str(_t->str_builder))

#define _intern_str(_t, _fmt, ...) cmon_str_buf_append(_t->str_buf, _tmp_str(_t, _fmt, __VA_ARGS__))

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
                                 cmon_type_kind _kind,
                                 size_t _name_off,
                                 size_t _unique_off,
                                 size_t _full_off,
                                 cmon_idx _extra_data)
{
    _type t;
    t.kind = _kind;
    t.name_str_off = _name_off;
    t.unique_name_str_off = _unique_off;
    t.full_name_str_off = _full_off;
    t.data_idx = _extra_data;
    cmon_dyn_arr_append(&_t->types, t);
    cmon_hashmap_set(
        &_t->name_map, cmon_str_buf_get(_t->str_buf, _unique_off), cmon_dyn_arr_count(&_t->types));
    return cmon_dyn_arr_count(&_t->types) - 1;
}

cmon_idx cmon_types_add_struct(cmon_types * _t, cmon_idx _mod, cmon_str_view _name)
{
    _struct strct;
    cmon_dyn_arr_init(&strct.fields, _t->alloc, 8);
    cmon_dyn_arr_append(&_t->structs, strct);
    return _add_type(
        _t,
        cmon_tk_struct,
        _intern_str(_t, "%.*s", _name.begin, _name.end - _name.begin),
        _intern_str(_t,
                    "%s_%.*s",
                    cmon_modules_prefix(_t->mods, _mod),
                    _name.begin,
                    _name.end - _name.begin),
        _intern_str(
            _t, "%s.%.*s", cmon_modules_name(_t->mods, _mod), _name.begin, _name.end - _name.begin),
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
    const char * unique_name;
    unique_name = _tmp_str(_t, "Ptr%s_%s", _is_mut ? "Mut" : "", cmon_types_unique_name(_t, _type));
    _return_if_found(_t, unique_name);
    return _add_type(
        _t,
        cmon_tk_ptr,
        _intern_str(_t, "*%s %s", _is_mut ? "mut" : "", cmon_types_name(_t, _type)),
        cmon_str_buf_append(_t->str_buf, unique_name),
        _intern_str(_t, "*%s %s", _is_mut ? "mut" : "", cmon_types_full_name(_t, _type)),
        (cmon_idx)_is_mut);
}

cmon_idx cmon_types_find(cmon_types * _t, const char * _unique_name)
{
    cmon_idx * idx_ptr;
    if (idx_ptr = cmon_hashmap_get(&_t->name_map, _unique_name))
        return *idx_ptr;
    return CMON_INVALID_IDX;
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

cmon_type_kind cmon_types_kind(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).kind;
}
