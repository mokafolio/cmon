#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_ir.h>
#include <cmon/cmon_str_builder.h>

typedef struct
{
    cmon_idx left;
    cmon_idx right;
    cmon_idx meta;
} _ir_left_right_meta;

typedef struct
{
    unsigned char op;
    cmon_idx left;
    cmon_idx right;
} _binop;

typedef struct
{
    unsigned char op;
    cmon_idx right;
} _prefix;

typedef struct
{
    cmon_idx left;
    cmon_idx args_begin;
    cmon_idx args_end;
} _call;

// used for every init that consists of a type and a range of expressions (i.e. struct init, array
// init etc.)
typedef struct
{
    cmon_idx type;
    cmon_idx exprs_begin;
    cmon_idx exprs_end;
} _init;

typedef struct
{
    cmon_idx left;
    cmon_idx right;
} _idx_pair;

typedef struct
{
    size_t name_off;
    cmon_bool is_pub;
    cmon_bool is_mut;
    cmon_idx type_idx;
    cmon_idx expr_idx;
} _var_decl;

typedef struct
{
    size_t name_off;
    cmon_idx return_type;
    cmon_idx params_begin;
    cmon_idx params_end;
    cmon_idx body;
} _fn_decl;

typedef struct cmon_irb
{
    cmon_allocator * alloc;
    cmon_str_builder * str_builder;
    cmon_str_buf * str_buf;
    cmon_dyn_arr(cmon_idx) types;
    cmon_dyn_arr(cmon_irk) kinds;
    cmon_dyn_arr(cmon_idx) data;
    cmon_dyn_arr(_binop) binops;
    cmon_dyn_arr(_prefix) prefix;
    cmon_dyn_arr(_call) calls;
    cmon_dyn_arr(_init) inits;
    cmon_dyn_arr(_idx_pair) idx_pairs;
    cmon_dyn_arr(_var_decl) var_decls;
    cmon_dyn_arr(_fn_decl) fns;
    cmon_dyn_arr(cmon_idx) idx_buffer;
    cmon_dyn_arr(_ir_left_right_meta) left_right_meta;
    // cmon_dyn_arr(cmon_idx) meta;
    // cmon_dyn_arr(cmon_idx) fns;
    // cmon_dyn_arr(cmon_idx) global_vars;
    // cmon_dyn_arr(cmon_idx) global_aliases;
    // cmon_ir ir; // filled in in cmon_irb_ir
} cmon_irb;

cmon_irb * cmon_irb_create(cmon_allocator * _alloc,
                           size_t _type_count,
                           size_t _fn_count,
                           size_t _global_var_count,
                           size_t _global_alias_count,
                           size_t _node_count_estimate)
{
    cmon_irb * ret = CMON_CREATE(_alloc, cmon_irb);
    ret->alloc = _alloc;
    ret->str_builder = cmon_str_builder_create(_alloc, 512);
    ret->str_buf = cmon_str_buf_create(_alloc, 1024);
    cmon_dyn_arr_init(&ret->types, _alloc, _type_count);
    cmon_dyn_arr_init(&ret->kinds, _alloc, _node_count_estimate);
    cmon_dyn_arr_init(&ret->data, _alloc, _node_count_estimate);
    cmon_dyn_arr_init(&ret->binops, _alloc, 32);
    cmon_dyn_arr_init(&ret->prefix, _alloc, 8);
    cmon_dyn_arr_init(&ret->calls, _alloc, 16);
    cmon_dyn_arr_init(&ret->inits, _alloc, 16);
    cmon_dyn_arr_init(&ret->idx_pairs, _alloc, 16);
    cmon_dyn_arr_init(&ret->var_decls, _alloc, 32);
    cmon_dyn_arr_init(&ret->fns, _alloc, _fn_count);
    cmon_dyn_arr_init(&ret->idx_buffer, _alloc, _node_count_estimate / 8);
    cmon_dyn_arr_init(&ret->left_right_meta, _alloc, _node_count_estimate);
    // cmon_dyn_arr_init(&ret->fns, _alloc, _fn_count);
    // cmon_dyn_arr_init(&ret->global_vars, _alloc, _global_var_count);
    // cmon_dyn_arr_init(&ret->global_aliases, _alloc, _global_alias_count);
    return ret;
}

void cmon_irb_destroy(cmon_irb * _b)
{
    if (!_b)
        return;

    // cmon_dyn_arr_dealloc(&_b->global_aliases);
    // cmon_dyn_arr_dealloc(&_b->global_vars);
    // cmon_dyn_arr_dealloc(&_b->fns);
    cmon_dyn_arr_dealloc(&_b->left_right_meta);
    cmon_dyn_arr_dealloc(&_b->idx_buffer);
    cmon_dyn_arr_dealloc(&_b->fns);
    cmon_dyn_arr_dealloc(&_b->var_decls);
    cmon_dyn_arr_dealloc(&_b->idx_pairs);
    cmon_dyn_arr_dealloc(&_b->inits);
    cmon_dyn_arr_dealloc(&_b->calls);
    cmon_dyn_arr_dealloc(&_b->prefix);
    cmon_dyn_arr_dealloc(&_b->binops);
    cmon_dyn_arr_dealloc(&_b->data);
    cmon_dyn_arr_dealloc(&_b->kinds);
    cmon_dyn_arr_dealloc(&_b->types);
    cmon_str_buf_destroy(_b->str_buf);
    cmon_str_builder_destroy(_b->str_builder);
    CMON_DESTROY(_b->alloc, _b);
}

void cmon_irb_add_type(cmon_irb * _b, cmon_idx _type_idx)
{
    cmon_dyn_arr_append(&_b->types, _type_idx);
}

size_t cmon_irb_type_count(cmon_irb * _b)
{
    return cmon_dyn_arr_count(&_b->types);
}

cmon_idx cmon_irb_type(cmon_irb * _b, size_t _idx)
{
    assert(_idx < cmon_irb_type_count(_b));
    return _b->types[_idx];
}

static inline cmon_idx _add_node(cmon_irb * _b, cmon_irk _kind, cmon_idx _data_idx)
{
    cmon_dyn_arr_append(&_b->kinds, _kind);
    cmon_dyn_arr_append(&_b->data, _data_idx);
    assert(cmon_dyn_arr_count(&_b->kinds) == cmon_dyn_arr_count(&_b->data));
    return (cmon_idx)(cmon_dyn_arr_count(&_b->kinds) - 1);
}

static inline cmon_idx _add_indices(cmon_irb * _b, cmon_idx * _indices, size_t _count)
{
    cmon_idx ret = cmon_dyn_arr_count(&_b->idx_buffer);
    for (size_t i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->idx_buffer, _indices[i]);
    }
    return ret;
}

cmon_idx cmon_irb_add_ident(cmon_irb * _b, const char * _name)
{
    return _add_node(_b, cmon_irk_string_lit, cmon_str_buf_append(_b->str_buf, _name));
}

cmon_idx cmon_irb_add_bool_lit(cmon_irb * _b, cmon_bool _value)
{
    return _add_node(_b, cmon_irk_bool_lit, (cmon_idx)_value);
}

cmon_idx cmon_irb_add_float_lit(cmon_irb * _b, const char * _value)
{
    return _add_node(_b, cmon_irk_float_lit, cmon_str_buf_append(_b->str_buf, _value));
}

cmon_idx cmon_irb_add_int_lit(cmon_irb * _b, const char * _value)
{
    return _add_node(_b, cmon_irk_int_lit, cmon_str_buf_append(_b->str_buf, _value));
}

cmon_idx cmon_irb_add_string_lit(cmon_irb * _b, const char * _value)
{
    return _add_node(_b, cmon_irk_string_lit, cmon_str_buf_append(_b->str_buf, _value));
}

cmon_idx cmon_irb_add_addr(cmon_irb * _b, cmon_idx _expr)
{
    return _add_node(_b, cmon_irk_addr, _expr);
}

cmon_idx cmon_irb_add_deref(cmon_irb * _b, cmon_idx _expr)
{
    return _add_node(_b, cmon_irk_deref, _expr);
}

cmon_idx cmon_irb_add_binary(cmon_irb * _b, unsigned char _op, cmon_idx _left, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->binops, ((_binop){ _op, _left, _right }));
    return _add_node(_b, cmon_irk_binary, cmon_dyn_arr_count(&_b->binops) - 1);
}

cmon_idx cmon_irb_add_prefix(cmon_irb * _b, unsigned char _op, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->prefix, ((_prefix){ _op, _right }));
    return _add_node(_b, cmon_irk_prefix, cmon_dyn_arr_count(&_b->binops) - 1);
}

cmon_idx cmon_irb_add_paran(cmon_irb * _b, cmon_idx _expr)
{
    return _add_node(_b, cmon_irk_paran_expr, _expr);
}

cmon_idx cmon_irb_add_call(cmon_irb * _b,
                           cmon_idx _expr_idx,
                           cmon_idx * _arg_indices,
                           size_t _count)
{
    cmon_idx begin = _add_indices(_b, _arg_indices, _count);
    cmon_dyn_arr_append(&_b->calls,
                        ((_call){ _expr_idx, begin, cmon_dyn_arr_count(&_b->idx_buffer) }));
    return _add_node(_b, cmon_irk_call, cmon_dyn_arr_count(&_b->calls) - 1);
}

cmon_idx cmon_irb_add_init(cmon_irb * _b,
                           cmon_idx _struct_type_idx,
                           cmon_idx * _fields,
                           size_t _count)
{
    cmon_idx begin = _add_indices(_b, _fields, _count);
    cmon_dyn_arr_append(&_b->inits,
                        ((_init){ _struct_type_idx, begin, cmon_dyn_arr_count(&_b->idx_buffer) }));
    return _add_node(_b, cmon_irk_struct_init, cmon_dyn_arr_count(&_b->inits) - 1);
}

cmon_idx cmon_irb_add_array_init(cmon_irb * _b,
                                 cmon_idx _array_type_idx,
                                 cmon_idx * _exprs,
                                 size_t _count)
{
    cmon_idx begin = _add_indices(_b, _exprs, _count);
    cmon_dyn_arr_append(&_b->inits,
                        ((_init){ _array_type_idx, begin, cmon_dyn_arr_count(&_b->idx_buffer) }));
    return _add_node(_b, cmon_irk_array_init, cmon_dyn_arr_count(&_b->inits) - 1);
}

cmon_idx cmon_irb_add_selector(cmon_irb * _b, cmon_idx _left, const char * _name)
{
    cmon_dyn_arr_append(&_b->idx_pairs,
                        ((_idx_pair){ _left, cmon_str_buf_append(_b->str_buf, _name) }));
    return _add_node(_b, cmon_irk_selector, cmon_dyn_arr_count(&_b->idx_pairs) - 1);
}

cmon_idx cmon_irb_add_index(cmon_irb * _b, cmon_idx _lhs, cmon_idx _index_expr)
{
    cmon_dyn_arr_append(&_b->idx_pairs, ((_idx_pair){ _lhs, _index_expr }));
    return _add_node(_b, cmon_irk_index, cmon_dyn_arr_count(&_b->idx_pairs) - 1);
}

cmon_idx cmon_irb_add_block(cmon_irb * _b, cmon_idx * _stmt_indices, size_t _count)
{
    cmon_idx begin = _add_indices(_b, _stmt_indices, _count);
    cmon_dyn_arr_append(&_b->idx_pairs,
                        ((_idx_pair){ begin, cmon_dyn_arr_count(&_b->idx_buffer) - 1 }));
    return _add_node(_b, cmon_irk_block, cmon_dyn_arr_count(&_b->idx_pairs) - 1);
}

cmon_idx cmon_irb_add_var_decl(
    cmon_irb * _b, const char * _name, cmon_bool _is_mut, cmon_idx _type_idx, cmon_idx _expr)
{
    cmon_dyn_arr_append(
        &_b->var_decls,
        ((_var_decl){
            cmon_str_buf_append(_b->str_buf, _name), cmon_false, _is_mut, _type_idx, _expr }));
    return _add_node(_b, cmon_irk_var_decl, cmon_dyn_arr_count(&_b->var_decls) - 1);
}

cmon_idx cmon_irb_add_alias(cmon_irb * _b, const char * _name, cmon_idx _type_idx)
{
    cmon_dyn_arr_append(&_b->idx_pairs,
                        ((_idx_pair){ cmon_str_buf_append(_b->str_buf, _name), _type_idx }));
    return _add_node(_b, cmon_irk_alias, cmon_dyn_arr_count(&_b->idx_pairs) - 1);
}

cmon_idx cmon_irb_add_fn(cmon_irb * _b,
                         const char * _name,
                         cmon_idx _return_type,
                         cmon_idx * _params,
                         size_t _count,
                         cmon_idx _body_block)
{
    cmon_idx params_begin = _add_indices(_b, _params, _count);
    cmon_dyn_arr_append(&_b->fns,
                        ((_fn_decl){ cmon_str_buf_append(_b->str_buf, _name),
                                     _return_type,
                                     params_begin,
                                     cmon_dyn_arr_count(&_b->idx_buffer),
                                     _body_block }));
    return cmon_dyn_arr_count(&_b->fns) - 1; 
}
