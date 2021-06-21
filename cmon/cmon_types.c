#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_types.h>
#include <cmon/cmon_util.h>

typedef struct
{
    const char * name_str;
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
    cmon_bool is_mut;
    cmon_idx type;
} _view;

typedef struct
{
    cmon_idx type;
    size_t count;
} _array;

typedef struct
{
    cmon_typek kind;
    // if it's a none builtin type, the following will be set.
    cmon_idx mod_idx;
    cmon_idx src_file_idx;
    cmon_idx name_tok;
    const char * name_str;
    const char * full_name_str;
    const char * unique_name_str;
    // data idx for additional type information that gets interpreted based on kind.
    // i.e. for structs this is the index into the structs array.
    cmon_idx data_idx;
    // array the size of all modules being compiled to indicate if the type is used by the module
    cmon_dyn_arr(cmon_bool) mod_map;
} _type;

typedef struct cmon_types
{
    cmon_allocator * alloc;
    cmon_modules * mods;
    cmon_dyn_arr(_struct) structs;
    cmon_dyn_arr(_fn_sig) fns;
    cmon_dyn_arr(_ptr) ptrs;
    cmon_dyn_arr(_view) views;
    cmon_dyn_arr(_array) arrays;
    cmon_dyn_arr(_type) types;
    cmon_hashmap(const char *, cmon_idx) name_map;
    cmon_str_builder * str_builder;
    cmon_dyn_arr(cmon_short_str) name_buf;

    // builtin type indices
    cmon_dyn_arr(cmon_idx) builtins;
    size_t builtins_end;
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
    cmon_idx builtin_modident;
    cmon_idx builtin_typeident;
} cmon_types;

// #define _tmp_str(_t, _fmt, ...)                                                                    \
//     (cmon_str_builder_clear(_t->str_builder),                                                      \
//      cmon_str_builder_append_fmt(_t->str_builder, _fmt, ##__VA_ARGS__),                            \
//      cmon_str_builder_c_str(_t->str_builder))

// #define _intern_str(_t, _fmt, ...)                                                                 \
//     cmon_str_buf_append(_t->str_buf, _tmp_str(_t, _fmt, ##__VA_ARGS__))

// #define _intern_str(_t, _fmt, ...)                                                                 \
//     (cmon_dyn_arr_append(&(_t)->name_buf,                                                          \
//                          (cmon_short_str_make(_t->alloc, _tmp_str(_t, _fmt, ##__VA_ARGS__)))),     \
//      cmon_short_str_c_str(&cmon_dyn_arr_last(&_t->name_buf)))

#define _get_type(_t, _idx) (assert(_idx < cmon_dyn_arr_count(&_t->types)), _t->types[_idx])

#define _return_if_found(_t, _name, _mod_idx)                                                      \
    do                                                                                             \
    {                                                                                              \
        cmon_idx _idx;                                                                             \
        if (cmon_is_valid_idx(_idx = cmon_types_find(_t, _name)))                                  \
        {                                                                                          \
            _t->types[_idx].mod_map[_mod_idx] = cmon_true;                                         \
            return _idx;                                                                           \
        }                                                                                          \
    } while (0)

static inline const char * _intern_c_str(cmon_types * _t, const char * _c_str)
{
    cmon_dyn_arr_append(&_t->name_buf, cmon_short_str_make(_t->alloc, _c_str));
    cmon_short_str str = cmon_dyn_arr_last(&_t->name_buf);
    return cmon_short_str_c_str(&cmon_dyn_arr_last(&_t->name_buf));
}

static inline const char * _intern_str(cmon_types * _t, const char * _fmt, ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_dyn_arr_append(
        &_t->name_buf,
        cmon_short_str_make(_t->alloc, cmon_str_builder_tmp_str_v(_t->str_builder, _fmt, args)));
    va_end(args);
    return cmon_short_str_c_str(&cmon_dyn_arr_last(&_t->name_buf));
}

static inline cmon_idx _add_type(cmon_types * _t,
                                 cmon_typek _kind,
                                 const char * _name,
                                 const char * _unique,
                                 const char * _full,
                                 cmon_idx _mod_idx,
                                 cmon_idx _src_file_idx,
                                 cmon_idx _name_tok,
                                 cmon_idx _extra_data)
{
    _type t;
    t.kind = _kind;
    t.src_file_idx = _src_file_idx;
    t.name_tok = _name_tok;
    t.name_str = _name;
    t.unique_name_str = _unique;
    t.full_name_str = _full;
    t.data_idx = _extra_data;
    cmon_dyn_arr_init(&t.mod_map, _t->alloc, cmon_modules_count(_t->mods));
    cmon_dyn_arr_resize(&t.mod_map, cmon_modules_count(_t->mods));
    memset(&t.mod_map[0], cmon_false, cmon_dyn_arr_count(&t.mod_map) * sizeof(cmon_bool));
    if(cmon_is_valid_idx(_mod_idx))
    {
        t.mod_map[_mod_idx] = cmon_true;
    }

    //only user defined types can be defined in a module. struct is the only user type for now.
    if(_kind == cmon_typek_struct)
    {
        t.mod_idx = _mod_idx;
    }
    else
    {
        t.mod_idx = CMON_INVALID_IDX;
    }

    cmon_dyn_arr_append(&_t->types, t);
    cmon_hashmap_set(&_t->name_map, _unique, cmon_dyn_arr_count(&_t->types) - 1);

    printf("adding type %s\n", t.unique_name_str);
    return cmon_dyn_arr_count(&_t->types) - 1;
}

static inline cmon_idx _add_builtin(cmon_types * _t,
                                    cmon_typek _kind,
                                    const char * _name,
                                    cmon_bool _hidden)
{
    const char * name = _intern_c_str(_t, _name);
    cmon_idx ret = _add_type(
        _t, _kind, name, name, name, CMON_INVALID_IDX, CMON_INVALID_IDX, CMON_INVALID_IDX, CMON_INVALID_IDX);
    if (!_hidden)
    {
        cmon_dyn_arr_append(&_t->builtins, ret);
    }
    return ret;
}

cmon_types * cmon_types_create(cmon_allocator * _alloc, cmon_modules * _mods)
{
    cmon_types * ret = CMON_CREATE(_alloc, cmon_types);
    ret->alloc = _alloc;
    ret->mods = _mods;
    cmon_dyn_arr_init(&ret->structs, _alloc, 32);
    cmon_dyn_arr_init(&ret->fns, _alloc, 16);
    cmon_dyn_arr_init(&ret->ptrs, _alloc, 16);
    cmon_dyn_arr_init(&ret->views, _alloc, 16);
    cmon_dyn_arr_init(&ret->arrays, _alloc, 16);
    cmon_dyn_arr_init(&ret->types, _alloc, 64);
    cmon_hashmap_str_key_init(&ret->name_map, _alloc);
    ret->str_builder = cmon_str_builder_create(_alloc, 256);
    cmon_dyn_arr_init(&ret->name_buf, _alloc, 64);
    cmon_dyn_arr_init(&ret->builtins, _alloc, 16);

    ret->builtin_s8 = _add_builtin(ret, cmon_typek_s8, "s8", cmon_false);
    ret->builtin_s16 = _add_builtin(ret, cmon_typek_s16, "s16", cmon_false);
    ret->builtin_s32 = _add_builtin(ret, cmon_typek_s32, "s32", cmon_false);
    ret->builtin_s64 = _add_builtin(ret, cmon_typek_s64, "s64", cmon_false);
    ret->builtin_u8 = _add_builtin(ret, cmon_typek_u8, "u8", cmon_false);
    ret->builtin_u16 = _add_builtin(ret, cmon_typek_u16, "u16", cmon_false);
    ret->builtin_u32 = _add_builtin(ret, cmon_typek_u32, "u32", cmon_false);
    ret->builtin_u64 = _add_builtin(ret, cmon_typek_u64, "u64", cmon_false);
    ret->builtin_f32 = _add_builtin(ret, cmon_typek_f32, "f32", cmon_false);
    ret->builtin_f64 = _add_builtin(ret, cmon_typek_f64, "f64", cmon_false);
    ret->builtin_void = _add_builtin(ret, cmon_typek_void, "void", cmon_false);
    ret->builtin_bool = _add_builtin(ret, cmon_typek_bool, "bool", cmon_false);
    ret->builtin_modident = _add_builtin(ret, cmon_typek_modident, "__modident", cmon_true);
    ret->builtin_typeident = _add_builtin(ret, cmon_typek_typeident, "__typeident", cmon_true);

    ret->builtins_end = cmon_dyn_arr_count(&ret->types);

    return ret;
}

void cmon_types_destroy(cmon_types * _t)
{
    if (!_t)
        return;

    size_t i;
    cmon_str_builder_destroy(_t->str_builder);
    for (i = 0; i < cmon_dyn_arr_count(&_t->name_buf); ++i)
    {
        cmon_short_str_dealloc(&_t->name_buf[i]);
    }
    cmon_dyn_arr_dealloc(&_t->name_buf);
    cmon_dyn_arr_dealloc(&_t->builtins);
    cmon_hashmap_dealloc(&_t->name_map);

    for (i = 0; i < cmon_dyn_arr_count(&_t->types); ++i)
    {
        cmon_dyn_arr_dealloc(&_t->types[i].mod_map);
    }
    cmon_dyn_arr_dealloc(&_t->types);
    cmon_dyn_arr_dealloc(&_t->arrays);
    cmon_dyn_arr_dealloc(&_t->views);
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
    CMON_DESTROY(_t->alloc, _t);
}

size_t cmon_types_count(cmon_types * _t)
{
    return cmon_dyn_arr_count(&_t->types);
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
        _intern_str(_t, "%.*s", _name.end - _name.begin, _name.begin),
        _intern_str(_t,
                    "%s_%.*s",
                    cmon_modules_prefix(_t->mods, _mod),
                    _name.end - _name.begin,
                    _name.begin),
        _intern_str(
            _t, "%s.%.*s", cmon_modules_name(_t->mods, _mod), _name.end - _name.begin, _name.begin),
        _mod,
        _src_file_idx,
        _name_tok,
        cmon_dyn_arr_count(&_t->structs) - 1);
}

cmon_idx cmon_types_struct_add_field(cmon_types * _t,
                                     cmon_idx _struct,
                                     cmon_str_view _name,
                                     cmon_idx _type,
                                     cmon_idx _def_expr_ast)
{
    cmon_dyn_arr_append(
        &_t->structs[_get_type(_t, _struct).data_idx].fields,
        ((_struct_field){
            _intern_str(_t, "%.*s", _name.end - _name.begin, _name.begin), _type, _def_expr_ast }));
    return cmon_dyn_arr_count(&_t->structs[_get_type(_t, _struct).data_idx].fields) - 1;
}

cmon_idx cmon_types_find_ptr(cmon_types * _t, cmon_idx _type, cmon_bool _is_mut, cmon_idx _mod_idx)
{
    _ptr ptr;
    const char * unique_name;
    unique_name = cmon_str_builder_tmp_str(
        _t->str_builder, "Ptr%s_%s", _is_mut ? "Mut" : "", cmon_types_unique_name(_t, _type));
    _return_if_found(_t, unique_name, _mod_idx);

    ptr.is_mut = _is_mut;
    ptr.type = _type;
    cmon_dyn_arr_append(&_t->ptrs, ptr);

    return _add_type(
        _t,
        cmon_typek_ptr,
        _intern_str(_t, "*%s %s", _is_mut ? "mut" : "", cmon_types_name(_t, _type)),
        _intern_c_str(_t, unique_name),
        _intern_str(_t, "*%s %s", _is_mut ? "mut" : "", cmon_types_full_name(_t, _type)),
        _mod_idx,
        CMON_INVALID_IDX,
        CMON_INVALID_IDX,
        cmon_dyn_arr_count(&_t->ptrs) - 1);
}

cmon_idx cmon_types_find_view(cmon_types * _t, cmon_idx _type, cmon_bool _is_mut, cmon_idx _mod_idx)
{
    _view view;
    const char * unique_name;
    unique_name = cmon_str_builder_tmp_str(
        _t->str_builder, "View%s_%s", _is_mut ? "Mut" : "", cmon_types_unique_name(_t, _type));
    _return_if_found(_t, unique_name, _mod_idx);

    view.is_mut = _is_mut;
    view.type = _type;
    cmon_dyn_arr_append(&_t->views, view);

    unique_name = _intern_c_str(_t, unique_name);
    return _add_type(
        _t,
        cmon_typek_view,
        _intern_str(_t, "[]%s %s", _is_mut ? "mut" : "", cmon_types_name(_t, _type)),
        unique_name,
        _intern_str(_t, "[]%s %s", _is_mut ? "mut" : "", cmon_types_full_name(_t, _type)),
        _mod_idx,
        CMON_INVALID_IDX,
        CMON_INVALID_IDX,
        cmon_dyn_arr_count(&_t->views) - 1);
}

cmon_idx cmon_types_find_array(cmon_types * _t, cmon_idx _type, size_t _size, cmon_idx _mod_idx)
{
    _array arr;
    const char * unique_name;
    unique_name = cmon_str_builder_tmp_str(
        _t->str_builder, "Array%lu_%s", _size, cmon_types_unique_name(_t, _type));
    _return_if_found(_t, unique_name, _mod_idx);
    arr.count = _size;
    arr.type = _type;
    cmon_dyn_arr_append(&_t->arrays, arr);
    unique_name = _intern_c_str(_t, unique_name);
    return _add_type(_t,
                     cmon_typek_array,
                     _intern_str(_t, "[%lu]%s", _size, cmon_types_name(_t, _type)),
                     unique_name,
                     _intern_str(_t, "[%lu]%s", _size, cmon_types_full_name(_t, _type)),
                     _mod_idx,
                     CMON_INVALID_IDX,
                     CMON_INVALID_IDX,
                     cmon_dyn_arr_count(&_t->arrays) - 1);
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

cmon_idx cmon_types_find_fn(
    cmon_types * _t, cmon_idx _ret_type, cmon_idx * _params, size_t _param_count, cmon_idx _mod_idx)
{
    _fn_sig sig;
    const char * unique_name;

    unique_name = _fn_name(_t, _ret_type, _params, _param_count, cmon_types_unique_name);

    _return_if_found(_t, unique_name, _mod_idx);

    cmon_dyn_arr_init(&sig.params, _t->alloc, _param_count);

    size_t i;
    for (i = 0; i < _param_count; ++i)
    {
        cmon_dyn_arr_append(&sig.params, _params[i]);
    }
    sig.return_type = _ret_type;

    cmon_dyn_arr_append(&_t->fns, sig);

    unique_name = _intern_c_str(_t, unique_name);
    printf("addinf fn sig %s\n", unique_name);
    return _add_type(
        _t,
        cmon_typek_fn,
        _intern_str(_t, _fn_name(_t, _ret_type, _params, _param_count, cmon_types_name)),
        unique_name,
        _intern_str(_t, _fn_name(_t, _ret_type, _params, _param_count, cmon_types_full_name)),
        _mod_idx,
        CMON_INVALID_IDX,
        CMON_INVALID_IDX,
        cmon_dyn_arr_count(&_t->fns) - 1);
}

cmon_idx cmon_types_find(cmon_types * _t, const char * _unique_name)
{
    cmon_idx * idx_ptr;
    if ((idx_ptr = cmon_hashmap_get(&_t->name_map, _unique_name)))
    {
        return *idx_ptr;
    }
    return CMON_INVALID_IDX;
}

const char * cmon_types_unique_name(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).unique_name_str;
}

const char * cmon_types_name(cmon_types * _t, cmon_idx _type_idx)
{
    printf("getting type name %lu %s\n", _type_idx, _get_type(_t, _type_idx).name_str);
    return _get_type(_t, _type_idx).name_str;
}

const char * cmon_types_full_name(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).full_name_str;
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

cmon_idx cmon_types_module(cmon_types * _t, cmon_idx _type_idx)
{
    return _get_type(_t, _type_idx).mod_idx;
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

size_t cmon_types_struct_field_count(cmon_types * _t, cmon_idx _struct_idx)
{
    assert(_get_type(_t, _struct_idx).kind == cmon_typek_struct);
    assert(_t->types[_struct_idx].data_idx < cmon_dyn_arr_count(&_t->structs));
    return cmon_dyn_arr_count(&_t->structs[_t->types[_struct_idx].data_idx].fields);
}

const char * cmon_types_struct_field_name(cmon_types * _t,
                                          cmon_idx _struct_idx,
                                          cmon_idx _field_idx)
{
    return _get_struct_field(_t, _struct_idx, _field_idx)->name_str;
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

cmon_idx cmon_types_struct_find_field(cmon_types * _t, cmon_idx _struct_idx, const char * _name)
{
    return cmon_types_struct_findv_field(_t, _struct_idx, cmon_str_view_make(_name));
}

cmon_idx cmon_types_struct_findv_field(cmon_types * _t, cmon_idx _struct_idx, cmon_str_view _name)
{
    //@NOTE: for now just linear search. maybe use hashmap in the future
    size_t i;
    for (i = 0; i < cmon_types_struct_field_count(_t, _struct_idx); ++i)
    {
        if (cmon_str_view_c_str_cmp(_name, cmon_types_struct_field_name(_t, _struct_idx, i)) == 0)
        {
            return i;
        }
    }
    return CMON_INVALID_IDX;
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

cmon_bool cmon_types_view_is_mut(cmon_types * _t, cmon_idx _v_idx)
{
    assert(_get_type(_t, _v_idx).kind == cmon_typek_view);
    return _t->views[_t->types[_v_idx].data_idx].is_mut;
}

cmon_idx cmon_types_view_type(cmon_types * _t, cmon_idx _v_idx)
{
    assert(_get_type(_t, _v_idx).kind == cmon_typek_view);
    return _t->views[_t->types[_v_idx].data_idx].type;
}

size_t cmon_types_array_count(cmon_types * _t, cmon_idx _arr_idx)
{
    assert(_get_type(_t, _arr_idx).kind == cmon_typek_array);
    return _t->arrays[_t->types[_arr_idx].data_idx].count;
}

cmon_idx cmon_types_array_type(cmon_types * _t, cmon_idx _arr_idx)
{
    assert(_get_type(_t, _arr_idx).kind == cmon_typek_array);
    return _t->arrays[_t->types[_arr_idx].data_idx].type;
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

cmon_idx cmon_types_fn_return_type(cmon_types * _t, cmon_idx _fn_idx)
{
    assert(_get_type(_t, _fn_idx).kind == cmon_typek_fn);
    assert(_t->types[_fn_idx].data_idx < cmon_dyn_arr_count(&_t->fns));
    return _t->fns[_t->types[_fn_idx].data_idx].return_type;
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

cmon_idx cmon_types_builtin_modident(cmon_types * _t)
{
    return _t->builtin_modident;
}

cmon_idx cmon_types_builtin_typeident(cmon_types * _t)
{
    return _t->builtin_typeident;
}

size_t cmon_types_builtin_count(cmon_types * _tr)
{
    return cmon_dyn_arr_count(&_tr->builtins);
}

cmon_idx cmon_types_builtin(cmon_types * _tr, cmon_idx _idx)
{
    return _tr->builtins[_idx];
}

cmon_bool cmon_types_is_builtin(cmon_types * _tr, cmon_idx _idx)
{
    return _idx < _tr->builtins_end;
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

cmon_bool cmon_types_is_float(cmon_types * _t, cmon_idx _idx)
{
    cmon_typek kind = cmon_types_kind(_t, _idx);
    return kind == cmon_typek_f32 || kind == cmon_typek_f64;
}

cmon_bool cmon_types_is_numeric(cmon_types * _t, cmon_idx _idx)
{
    return cmon_types_is_int(_t, _idx) || cmon_types_is_float(_t, _idx);
}

cmon_idx cmon_types_remove_ptr(cmon_types * _t, cmon_idx _idx)
{
    while (cmon_types_kind(_t, _idx) == cmon_typek_ptr)
        _idx = cmon_types_ptr_type(_t, _idx);
    return _idx;
}

cmon_bool cmon_types_is_implicit(cmon_types * _t, cmon_idx _idx)
{
    cmon_typek kind = cmon_types_kind(_t, _idx);
    return kind == cmon_typek_array || kind == cmon_typek_view || kind == cmon_typek_ptr ||
           kind == cmon_typek_fn;
}

void cmon_types_set_used_in_module(cmon_types * _tr, cmon_idx _idx, cmon_idx _mod_idx)
{
    _get_type(_tr, _idx).mod_map[_mod_idx] = cmon_true;
}

cmon_bool cmon_types_is_used_in_module(cmon_types * _tr, cmon_idx _idx, cmon_idx _mod_idx)
{
    return _get_type(_tr, _idx).mod_map[_mod_idx];
}

const char * cmon_typek_to_str(cmon_typek _kind)
{

}
