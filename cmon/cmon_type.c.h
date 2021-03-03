#include <cmon/cmon_type.h>

typedef struct
{
    cmon_idx name_tok;
    cmon_idx type;
    cmon_idx def_expr;
} _struct_field;

typedef struct
{
    cmon_dyn_arr(_struct_field) fields;
} _struct;

typedef struct cmon_tr
{
    cmon_dyn_arr(_struct) structs;
    cmon_dyn_arr(_type) types;
    // cmon_hashmap()
} cmon_tr;

cmon_tr * cmon_tr_create(cmon_allocator * _alloc)
{
}

void cmon_tr_destroy(cmon_tr * _tr)
{
}

cmon_idx cmon_tr_add_struct(cmon_tr * _tr, cmon_idx _name_tok)
{
}

cmon_idx cmon_tr_struct_add_field(
    cmon_tr * _tr, cmon_idx _struct, cmon_idx _name_tok, cmon_idx _type, cmon_idx _def_expr_ast)
{
}

cmon_idx cmon_tr_find_ptr(cmon_tr * _tr, cmon_idx _type, cmon_bool _is_mut)
{
}

const char * cmon_tr_unique_name(cmon_tr * _tr, cmon_idx _type_idx)
{
}

const char * cmon_tr_name(cmon_tr * _tr, cmon_idx _type_idx)
{
}

const char * cmon_tr_full_name(cmon_tr * _tr, cmon_idx _type_idx)
{
}

cmon_idx cmon_tr_name_token(cmon_tr * _tr, cmon_idx _type_idx)
{
}

cmon_type_kind cmon_tr_kind(cmon_tr * _tr, cmon_idx _type_idx)
{
}
