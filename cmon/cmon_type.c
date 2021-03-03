#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
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

typedef struct
{
    cmon_type_kind kind;
    const char * name;
    const char * full_name;
    const char * unique_name;
    // data idx for additional type information that gets interpreted based on kind.
    // i.e. for structs this is the index into the structs array.
    cmon_idx data_idx;
} _type;

typedef struct cmon_tr
{
    cmon_allocator * alloc;
    cmon_dyn_arr(_struct) structs;
    cmon_dyn_arr(_type) types;
    cmon_hashmap(const char *, size_t) name_map;
} cmon_tr;

cmon_tr * cmon_tr_create(cmon_allocator * _alloc)
{
    cmon_tr * ret = CMON_CREATE(_alloc, cmon_tr);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->structs, _alloc, 32);
    cmon_dyn_arr_init(&ret->types, _alloc, 64);
    cmon_hashmap_str_key_init(&ret->name_map, _alloc);
    return ret;
}

void cmon_tr_destroy(cmon_tr * _tr)
{
    size_t i;
    cmon_hashmap_dealloc(&_tr->name_map);
    cmon_dyn_arr_dealloc(&_tr->types);
    for(i=0; i<cmon_dyn_arr_count(&_tr->structs); ++i)
    {
        cmon_dyn_arr_dealloc(&_tr->structs[i].fields);
    }
    cmon_dyn_arr_dealloc(&_tr->structs);
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
