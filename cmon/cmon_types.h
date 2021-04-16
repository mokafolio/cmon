#ifndef CMON_CMON_TYPES_H
#define CMON_CMON_TYPES_H

#include <cmon/cmon_allocator.h>

typedef enum
{
    cmon_typek_void,
    cmon_typek_s8,
    cmon_typek_s16,
    cmon_typek_s32,
    cmon_typek_s64,
    cmon_typek_u8,
    cmon_typek_u16,
    cmon_typek_u32,
    cmon_typek_u64,
    cmon_typek_f32,
    cmon_typek_f64,
    cmon_typek_bool,
    cmon_typek_fn,
    cmon_typek_struct,
    cmon_typek_ptr,
    cmon_typek_array,
    cmon_typek_view,
    cmon_typek_tuple,
    cmon_typek_optional,
    cmon_typek_noinit,
    cmon_typek_variant,
    // cmon_typek_typealias,
    // cmon_typek_typedef,
    // cmon_typek_range,
    cmon_typek_modident,
    cmon_typek_typeident
} cmon_typek;

typedef struct cmon_modules cmon_modules;
typedef struct cmon_types cmon_types;

CMON_API cmon_types * cmon_types_create(cmon_allocator * _alloc, cmon_modules * _mods);
CMON_API void cmon_types_destroy(cmon_types * _tr);
CMON_API cmon_idx cmon_types_add_struct(
    cmon_types * _tr, cmon_idx _mod, cmon_str_view _name, cmon_idx _src_idx, cmon_idx _name_tok);
CMON_API cmon_idx cmon_types_struct_add_field(cmon_types * _tr,
                                              cmon_idx _struct,
                                              cmon_str_view _name,
                                              cmon_idx _type,
                                              cmon_idx _def_expr_ast);
CMON_API cmon_idx cmon_types_find_ptr(cmon_types * _tr, cmon_idx _type, cmon_bool _is_mut);
CMON_API cmon_idx cmon_types_find_view(cmon_types * _tr, cmon_idx _type, cmon_bool _is_mut);
CMON_API cmon_idx cmon_types_find_array(cmon_types * _tr, cmon_idx _type, size_t _size);
CMON_API cmon_idx cmon_types_find_fn(cmon_types * _tr,
                                     cmon_idx _ret_type,
                                     cmon_idx * _params,
                                     size_t _param_count);
CMON_API cmon_idx cmon_types_find(cmon_types * _t, const char * _unique_name);

CMON_API const char * cmon_types_unique_name(cmon_types * _tr, cmon_idx _type_idx);
CMON_API const char * cmon_types_name(cmon_types * _tr, cmon_idx _type_idx);
CMON_API const char * cmon_types_full_name(cmon_types * _tr, cmon_idx _type_idx);
CMON_API cmon_typek cmon_types_kind(cmon_types * _tr, cmon_idx _type_idx);
CMON_API cmon_idx cmon_types_src_file(cmon_types * _tr, cmon_idx _type_idx);
CMON_API cmon_idx cmon_types_name_tok(cmon_types * _tr, cmon_idx _type_idx);

// struct specific getters
CMON_API cmon_idx cmon_types_struct_field_count(cmon_types * _tr, cmon_idx _struct_idx);
CMON_API const char * cmon_types_struct_field_name(cmon_types * _tr,
                                                   cmon_idx _struct_idx,
                                                   cmon_idx _field_idx);
CMON_API cmon_idx cmon_types_struct_field_type(cmon_types * _tr,
                                               cmon_idx _struct_idx,
                                               cmon_idx _field_idx);
CMON_API cmon_idx cmon_types_struct_field_def_expr(cmon_types * _tr,
                                                   cmon_idx _struct_idx,
                                                   cmon_idx _field_idx);
CMON_API cmon_idx cmon_types_struct_find_field(cmon_types * _tr,
                                               cmon_idx _struct_idx,
                                               const char * _name);
CMON_API cmon_idx cmon_types_struct_findv_field(cmon_types * _tr,
                                               cmon_idx _struct_idx,
                                               cmon_str_view _name);

// builtin type idx getters
CMON_API cmon_idx cmon_types_builtin_s8(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_s16(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_s32(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_s64(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_u8(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_u16(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_u32(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_u64(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_f32(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_f64(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_void(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_bool(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_u8_view(cmon_types * _tr); // used for strings
// CMON_API cmon_idx cmon_types_noinit(cmon_types * _tr); //---
CMON_API cmon_idx cmon_types_builtin_modident(cmon_types * _tr);
CMON_API cmon_idx cmon_types_builtin_typeident(cmon_types * _tr);

// ptr specific getters
CMON_API cmon_bool cmon_types_ptr_is_mut(cmon_types * _tr, cmon_idx _ptr_idx);
CMON_API cmon_idx cmon_types_ptr_type(cmon_types * _tr, cmon_idx _ptr_idx);

// view specific getters
CMON_API cmon_bool cmon_types_view_is_mut(cmon_types * _tr, cmon_idx _v_idx);
CMON_API cmon_idx cmon_types_view_type(cmon_types * _tr, cmon_idx _v_idx);

// array specific getters
CMON_API size_t cmon_types_array_count(cmon_types * _tr, cmon_idx _arr_idx);
CMON_API cmon_idx cmon_types_array_type(cmon_types * _tr, cmon_idx _arr_idx);

// fn specific getters
CMON_API cmon_idx cmon_types_fn_return_type(cmon_types * _tr, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_types_fn_param_count(cmon_types * _tr, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_types_fn_param(cmon_types * _tr, cmon_idx _fn_idx, cmon_idx _param_idx);

// utilities
CMON_API cmon_bool cmon_types_is_unsigned_int(cmon_types * _tr, cmon_idx _idx);
CMON_API cmon_bool cmon_types_is_signed_int(cmon_types * _tr, cmon_idx _idx);
CMON_API cmon_bool cmon_types_is_int(cmon_types * _tr, cmon_idx _idx);

#endif // CMON_CMON_TYPES_H
