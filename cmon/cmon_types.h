#ifndef CMON_TYPES_H
#define CMON_TYPES_H

#include <cmon/cmon_allocator.h>

typedef enum
{
    cmon_tk_void,
    cmon_tk_s8,
    cmon_tk_s16,
    cmon_tk_s32,
    cmon_tk_s64,
    cmon_tk_u8,
    cmon_tk_u16,
    cmon_tk_u32,
    cmon_tk_u64,
    cmon_tk_f32,
    cmon_tk_f64,
    cmon_tk_bool,
    cmon_tk_fn,
    cmon_tk_struct,
    cmon_tk_ptr,
    cmon_tk_array,
    cmon_tk_view,
    cmon_tk_tuple,
    cmon_tk_optional,
    cmon_tk_noinit,
    cmon_tk_variant,
    // cmon_tk_typealias,
    // cmon_tk_typedef,
    // cmon_tk_range,
    cmon_tk_module // type specifier for module identifiers
} cmon_type_kind;

typedef struct cmon_modules cmon_modules;
typedef struct cmon_types cmon_types;

CMON_API cmon_types * cmon_types_create(cmon_allocator * _alloc, cmon_modules * _mods);
CMON_API void cmon_types_destroy(cmon_types * _tr);
CMON_API cmon_idx cmon_types_add_struct(cmon_types * _tr, cmon_idx _mod, cmon_str_view _name);
CMON_API cmon_idx cmon_types_struct_add_field(cmon_types * _tr,
                                           cmon_idx _struct,
                                           cmon_str_view _name,
                                           cmon_idx _type,
                                           cmon_idx _def_expr_ast);
CMON_API cmon_idx cmon_types_find_ptr(cmon_types * _tr, cmon_idx _type, cmon_bool _is_mut);
CMON_API cmon_idx cmon_types_find(cmon_types * _t, const char * _unique_name);

CMON_API const char * cmon_types_unique_name(cmon_types * _tr, cmon_idx _type_idx);
CMON_API const char * cmon_types_name(cmon_types * _tr, cmon_idx _type_idx);
CMON_API const char * cmon_types_full_name(cmon_types * _tr, cmon_idx _type_idx);
CMON_API cmon_type_kind cmon_types_kind(cmon_types * _tr, cmon_idx _type_idx);

#endif // CMON_TYPES_H
