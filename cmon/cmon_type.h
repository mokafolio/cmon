#ifndef CMON_TYPE_H
#define CMON_TYPE_H

typedef enum
{
    cmon_type_void,
    cmon_type_s8,
    cmon_type_s16,
    cmon_type_s32,
    cmon_type_s64,
    cmon_type_u8,
    cmon_type_u16,
    cmon_type_u32,
    cmon_type_u64,
    cmon_type_f32,
    cmon_type_f64,
    cmon_type_bool,
    cmon_type_fn,
    cmon_type_struct,
    cmon_type_ptr,
    cmon_type_array,
    cmon_type_view,
    cmon_type_tuple,
    cmon_type_optional,
    cmon_type_noinit,
    cmon_type_variant,
    // cmon_type_typealias,
    // cmon_type_typedef,
    // cmon_type_range,
    cmon_type_module // type specifier for module identifiers
} cmon_type_kind;

typedef struct cmon_tr cmon_tr;

CMON_API cmon_tr * cmon_tr_create(cmon_allocator * _alloc);
CMON_API void cmon_tr_destroy(cmon_tr * _tr);
CMON_API cmon_idx cmon_tr_add_struct(cmon_tr * _tr, cmon_idx _name_tok);
CMON_API cmon_idx cmon_tr_struct_add_field(cmon_tr * _tr,
                                           cmon_idx _struct,
                                           cmon_idx _name_tok,
                                           cmon_idx _type,
                                           cmon_idx _def_expr_ast);
CMON_API cmon_idx cmon_tr_find_ptr(cmon_tr * _tr, cmon_idx _type, cmon_bool _is_mut);

CMON_API const char * cmon_tr_unique_name(cmon_tr * _tr, cmon_idx _type_idx);
CMON_API const char * cmon_tr_name(cmon_tr * _tr, cmon_idx _type_idx);
CMON_API const char * cmon_tr_full_name(cmon_tr * _tr, cmon_idx _type_idx);
CMON_API cmon_idx cmon_tr_name_token(cmon_tr * _tr, cmon_idx _type_idx);
CMON_API cmon_type_kind cmon_tr_kind(cmon_tr * _tr, cmon_idx _type_idx);

#endif // CMON_TYPE_H
