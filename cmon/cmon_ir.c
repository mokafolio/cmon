#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_ir.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_types.h>

typedef struct
{
    cmon_idx mod_idx;
    size_t name_off;
} _dependency;

typedef struct
{
    char op;
    cmon_idx left;
    cmon_idx right;
} _binop;

typedef struct
{
    char op;
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
    cmon_idx type_idx;
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
    cmon_idx body_idx;
} _fn_decl;

typedef struct cmon_ir
{
    cmon_allocator * alloc;
    const char * str_buf;
    size_t str_buf_count;
    cmon_idx main_fn_idx;
    _dependency * deps;
    size_t deps_count;
    cmon_idx * types;
    size_t types_count;
    cmon_irk * kinds;
    size_t kinds_count;
    cmon_idx * data;
    size_t data_count;
    _binop * binops;
    size_t binops_count;
    _prefix * prefixes;
    size_t prefixes_count;
    _call * calls;
    size_t calls_count;
    _init * inits;
    size_t inits_count;
    _idx_pair * idx_pairs;
    size_t idx_pairs_count;
    _var_decl * var_decls;
    size_t var_decls_count;
    _fn_decl * fn_data;
    size_t fn_data_count;
    cmon_idx * idx_buffer;
    size_t idx_buffer_count;
    cmon_idx * global_vars;
    size_t global_vars_count;
    cmon_idx * fns;
    size_t fns_count;
} cmon_ir;

typedef struct cmon_irb
{
    cmon_allocator * alloc;
    cmon_str_builder * str_builder;
    cmon_str_buf * str_buf;
    cmon_idx main_fn_idx;
    cmon_dyn_arr(_dependency) deps;
    cmon_dyn_arr(cmon_idx) types;
    cmon_dyn_arr(cmon_irk) kinds;
    cmon_dyn_arr(cmon_idx) data;
    cmon_dyn_arr(_binop) binops;
    cmon_dyn_arr(_prefix) prefixes;
    cmon_dyn_arr(_call) calls;
    cmon_dyn_arr(_init) inits;
    cmon_dyn_arr(_idx_pair) idx_pairs;
    cmon_dyn_arr(_var_decl) var_decls;
    cmon_dyn_arr(_fn_decl) fn_data;
    cmon_dyn_arr(cmon_idx) idx_buffer;
    cmon_dyn_arr(cmon_idx) global_vars;
    cmon_dyn_arr(cmon_idx) fns;
    cmon_ir ir; // filled in in cmon_irb_ir
} cmon_irb;

cmon_irb * cmon_irb_create(cmon_allocator * _alloc,
                           size_t _dep_count,
                           size_t _type_count,
                           size_t _fn_count,
                           size_t _global_var_count,
                           size_t _node_count_estimate)
{
    cmon_irb * ret = CMON_CREATE(_alloc, cmon_irb);
    ret->alloc = _alloc;
    ret->str_builder = cmon_str_builder_create(_alloc, 512);
    ret->str_buf = cmon_str_buf_create(_alloc, 1024);
    ret->main_fn_idx = CMON_INVALID_IDX;
    cmon_dyn_arr_init(&ret->deps, _alloc, _dep_count);
    cmon_dyn_arr_init(&ret->types, _alloc, _type_count);
    cmon_dyn_arr_init(&ret->kinds, _alloc, _node_count_estimate);
    cmon_dyn_arr_init(&ret->data, _alloc, _node_count_estimate);
    cmon_dyn_arr_init(&ret->binops, _alloc, 32);
    cmon_dyn_arr_init(&ret->prefixes, _alloc, 8);
    cmon_dyn_arr_init(&ret->calls, _alloc, 16);
    cmon_dyn_arr_init(&ret->inits, _alloc, 16);
    cmon_dyn_arr_init(&ret->idx_pairs, _alloc, 16);
    cmon_dyn_arr_init(&ret->var_decls, _alloc, 32);
    cmon_dyn_arr_init(&ret->idx_buffer, _alloc, _node_count_estimate / 8);
    cmon_dyn_arr_init(&ret->fn_data, _alloc, _fn_count);
    cmon_dyn_arr_init(&ret->global_vars, _alloc, _global_var_count);
    cmon_dyn_arr_init(&ret->fns, _alloc, _fn_count);
    return ret;
}

void cmon_irb_destroy(cmon_irb * _b)
{
    if (!_b)
        return;

    cmon_dyn_arr_dealloc(&_b->fns);
    cmon_dyn_arr_dealloc(&_b->global_vars);
    cmon_dyn_arr_dealloc(&_b->fn_data);
    cmon_dyn_arr_dealloc(&_b->idx_buffer);
    cmon_dyn_arr_dealloc(&_b->var_decls);
    cmon_dyn_arr_dealloc(&_b->idx_pairs);
    cmon_dyn_arr_dealloc(&_b->inits);
    cmon_dyn_arr_dealloc(&_b->calls);
    cmon_dyn_arr_dealloc(&_b->prefixes);
    cmon_dyn_arr_dealloc(&_b->binops);
    cmon_dyn_arr_dealloc(&_b->data);
    cmon_dyn_arr_dealloc(&_b->kinds);
    cmon_dyn_arr_dealloc(&_b->types);
    cmon_dyn_arr_dealloc(&_b->deps);
    cmon_str_buf_destroy(_b->str_buf);
    cmon_str_builder_destroy(_b->str_builder);
    CMON_DESTROY(_b->alloc, _b);
}

void cmon_irb_add_type(cmon_irb * _b, cmon_idx _type_idx)
{
    cmon_dyn_arr_append(&_b->types, _type_idx);
}

void cmon_irb_add_dep(cmon_irb * _b, cmon_idx _mod_idx, const char * _unique_name)
{
    cmon_dyn_arr_append(
        &_b->deps, ((_dependency){ _mod_idx, cmon_str_buf_append(_b->str_buf, _unique_name) }));
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

// static inline cmon_idx _idx_buf_end(cmon_irb * _b)
// {
//     return cmon_dyn_arr_count(&_b->idx_buffer) ? cmon_dyn_arr_count(&_b->idx_buffer) - 1 : 0;
// }

// cmon_idx cmon_irb_add_ident(cmon_irb * _b, const char * _name)
// {
//     return _add_node(_b, cmon_irk_ident, cmon_str_buf_append(_b->str_buf, _name));
// }

cmon_idx cmon_irb_add_ident(cmon_irb * _b, cmon_idx _ref_idx)
{
    assert(_ref_idx < cmon_dyn_arr_count(&_b->kinds));
    return _add_node(_b, cmon_irk_ident, _ref_idx);
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

cmon_idx cmon_irb_add_binary(cmon_irb * _b, char _op, cmon_idx _left, cmon_idx _right)
{
    printf("cmon_irb_add_binary %c\n", _op);
    cmon_dyn_arr_append(&_b->binops, ((_binop){ _op, _left, _right }));
    return _add_node(_b, cmon_irk_binary, cmon_dyn_arr_count(&_b->binops) - 1);
}

cmon_idx cmon_irb_add_prefix(cmon_irb * _b, char _op, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->prefixes, ((_prefix){ _op, _right }));
    return _add_node(_b, cmon_irk_prefix, cmon_dyn_arr_count(&_b->prefixes) - 1);
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

cmon_idx cmon_irb_add_struct_init(cmon_irb * _b,
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
    printf("cmon_irb_add_block %lu\n", begin);
    cmon_dyn_arr_append(&_b->idx_pairs,
                        ((_idx_pair){ begin, cmon_dyn_arr_count(&_b->idx_buffer) }));
    return _add_node(_b, cmon_irk_block, cmon_dyn_arr_count(&_b->idx_pairs) - 1);
}

static inline cmon_idx _add_var_decl(cmon_irb * _b,
                                     const char * _name,
                                     cmon_bool _is_pub,
                                     cmon_bool _is_mut,
                                     cmon_idx _type_idx,
                                     cmon_idx _expr)
{
    cmon_dyn_arr_append(
        &_b->var_decls,
        ((_var_decl){
            cmon_str_buf_append(_b->str_buf, _name), _is_pub, _is_mut, _type_idx, _expr }));
    return _add_node(_b, cmon_irk_var_decl, cmon_dyn_arr_count(&_b->var_decls) - 1);
}

cmon_idx cmon_irb_add_var_decl(
    cmon_irb * _b, const char * _name, cmon_bool _is_mut, cmon_idx _type_idx, cmon_idx _expr)
{
    return _add_var_decl(_b, _name, cmon_false, _is_mut, _type_idx, _expr);
}

cmon_idx cmon_irb_add_fn(cmon_irb * _b,
                         const char * _name,
                         cmon_idx _return_type,
                         cmon_idx * _params,
                         size_t _count,
                         cmon_bool _is_main_fn)
{
    cmon_idx params_begin = _add_indices(_b, _params, _count);

    cmon_dyn_arr_append(&_b->fn_data,
                        ((_fn_decl){ cmon_str_buf_append(_b->str_buf, _name),
                                     _return_type,
                                     params_begin,
                                     cmon_dyn_arr_count(&_b->idx_buffer),
                                     CMON_INVALID_IDX }));

    cmon_idx ret = _add_node(_b, cmon_irk_fn, cmon_dyn_arr_count(&_b->fn_data) - 1);
    if (_is_main_fn)
    {
        assert(!cmon_is_valid_idx(_b->main_fn_idx));
        _b->main_fn_idx = ret;
    }
    cmon_dyn_arr_append(&_b->fns, ret);
    return ret;
}

void cmon_irb_fn_set_body(cmon_irb * _b, cmon_idx _fn, cmon_idx _body)
{
    assert(_fn < cmon_dyn_arr_count(&_b->kinds));
    printf("ir kind %i\n", _b->kinds[_fn]);
    assert(_b->kinds[_fn] == cmon_irk_fn);
    _b->fn_data[_b->data[_fn]].body_idx = _body;
}

cmon_idx cmon_irb_add_global_var_decl(cmon_irb * _b,
                                      const char * _name,
                                      cmon_bool _is_pub,
                                      cmon_bool _is_mut,
                                      cmon_idx _type_idx,
                                      cmon_idx _expr)
{
    cmon_idx ret = _add_var_decl(_b, _name, _is_pub, _is_mut, _type_idx, _expr);
    cmon_dyn_arr_append(&_b->global_vars, ret);
    return ret;
}

cmon_ir * cmon_irb_ir(cmon_irb * _b)
{
    cmon_ir * ret = &_b->ir;
    ret->alloc = NULL;
    ret->str_buf = cmon_str_buf_get(_b->str_buf, 0);
    ret->str_buf_count = cmon_str_buf_count(_b->str_buf);
    ret->main_fn_idx = _b->main_fn_idx;
    ret->deps = _b->deps;
    ret->deps_count = cmon_dyn_arr_count(&_b->deps);
    ret->types = _b->types;
    ret->types_count = cmon_dyn_arr_count(&_b->types);
    ret->kinds = _b->kinds;
    ret->kinds_count = cmon_dyn_arr_count(&_b->kinds);
    ret->data = _b->data;
    ret->data_count = cmon_dyn_arr_count(&_b->data);
    ret->binops = _b->binops;
    ret->binops_count = cmon_dyn_arr_count(&_b->binops);
    ret->prefixes = _b->prefixes;
    ret->prefixes_count = cmon_dyn_arr_count(&_b->prefixes);
    ret->calls = _b->calls;
    ret->calls_count = cmon_dyn_arr_count(&_b->calls);
    ret->inits = _b->inits;
    ret->inits_count = cmon_dyn_arr_count(&_b->inits);
    ret->idx_pairs = _b->idx_pairs;
    ret->idx_pairs_count = cmon_dyn_arr_count(&_b->idx_pairs);
    ret->var_decls = _b->var_decls;
    ret->var_decls_count = cmon_dyn_arr_count(&_b->var_decls);
    ret->fn_data = _b->fn_data;
    ret->fn_data_count = cmon_dyn_arr_count(&_b->fn_data);
    ret->idx_buffer = _b->idx_buffer;
    ret->idx_buffer_count = cmon_dyn_arr_count(&_b->idx_buffer);
    ret->global_vars = _b->global_vars;
    ret->global_vars_count = cmon_dyn_arr_count(&_b->global_vars);
    ret->fns = _b->fns;
    ret->fns_count = cmon_dyn_arr_count(&_b->fns);
    return ret;
}

static inline cmon_idx _idx_buf_get(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_idx < _ir->idx_buffer_count);
    return _ir->idx_buffer[_idx];
}

static inline const char * _ir_str(cmon_ir * _ir, size_t _str_off)
{
    assert(_str_off < _ir->str_buf_count);
    return &_ir->str_buf[_str_off];
}

static inline cmon_idx _ir_data(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_idx < _ir->data_count);
    return _ir->data[_idx];
}

static inline cmon_irk _ir_kind(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_idx < _ir->kinds_count);
    return _ir->kinds[_idx];
}

size_t cmon_ir_dep_count(cmon_ir * _ir)
{
    return _ir->deps_count;
}

cmon_idx cmon_ir_dep_module(cmon_ir * _ir, cmon_idx _dep_idx)
{
    return _ir->deps[_dep_idx].mod_idx;
}

const char * cmon_ir_dep_name(cmon_ir * _ir, cmon_idx _dep_idx)
{
    return _ir_str(_ir, _ir->deps[_dep_idx].name_off);
}

size_t cmon_ir_type_count(cmon_ir * _ir)
{
    return _ir->types_count;
}

cmon_idx cmon_ir_type(cmon_ir * _ir, size_t _idx)
{
    assert(_idx < cmon_ir_type_count(_ir));
    return _ir->types[_idx];
}

size_t cmon_ir_fn_count(cmon_ir * _ir)
{
    return _ir->fns_count;
}

cmon_idx cmon_ir_fn(cmon_ir * _ir, size_t _i)
{
    return _ir->fns[_i];
}

cmon_idx cmon_ir_main_fn(cmon_ir * _ir)
{
    return _ir->main_fn_idx;
}

size_t cmon_ir_global_var_count(cmon_ir * _ir)
{
    return _ir->global_vars_count;
}

cmon_idx cmon_ir_global_var(cmon_ir * _ir, size_t _i)
{
    return _ir->global_vars[_i];
}

cmon_irk cmon_ir_kind(cmon_ir * _ir, cmon_idx _idx)
{
    return _ir_kind(_ir, _idx);
}

const char * cmon_ir_ident_name(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_ident);
    cmon_irk kind = _ir_kind(_ir, _ir_data(_ir, _idx));
    if (kind == cmon_irk_var_decl)
    {
        return cmon_ir_var_decl_name(_ir, _ir_data(_ir, _idx));
    }
    else if (kind == cmon_irk_fn)
    {
        return cmon_ir_fn_name(_ir, _ir_data(_ir, _idx));
    }
    assert(0);
    return "";
}

cmon_bool cmon_ir_bool_lit_value(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_bool_lit);
    return (cmon_bool)_ir_data(_ir, _idx);
}

const char * cmon_ir_float_lit_value(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_float_lit);
    return _ir_str(_ir, _ir_data(_ir, _idx));
}

const char * cmon_ir_int_lit_value(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_int_lit);
    return _ir_str(_ir, _ir_data(_ir, _idx));
}

const char * cmon_ir_string_lit_value(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_string_lit);
    return _ir_str(_ir, _ir_data(_ir, _idx));
}

cmon_idx cmon_ir_addr_expr(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_addr);
    return _ir_data(_ir, _idx);
}

cmon_idx cmon_ir_deref_expr(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_deref);
    return _ir_data(_ir, _idx);
}

char cmon_ir_binary_op(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_binary);
    return _ir->binops[_ir_data(_ir, _idx)].op;
}

cmon_idx cmon_ir_binary_left(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_binary);
    return _ir->binops[_ir_data(_ir, _idx)].left;
}

cmon_idx cmon_ir_binary_right(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_binary);
    return _ir->binops[_ir_data(_ir, _idx)].right;
}

char cmon_ir_prefix_op(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_prefix);
    return _ir->prefixes[_ir_data(_ir, _idx)].op;
}

cmon_idx cmon_ir_prefix_expr(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_prefix);
    return _ir->prefixes[_ir_data(_ir, _idx)].right;
}

cmon_idx cmon_ir_paran_expr(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_paran_expr);
    return _ir_data(_ir, _idx);
}

cmon_idx cmon_ir_call_left(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_call);
    return _ir->calls[_ir_data(_ir, _idx)].left;
}

size_t cmon_ir_call_arg_count(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_call);
    return _ir->calls[_ir_data(_ir, _idx)].args_end - _ir->calls[_ir_data(_ir, _idx)].args_begin;
}

cmon_idx cmon_ir_call_arg(cmon_ir * _ir, cmon_idx _idx, size_t _arg_idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_call);
    return _idx_buf_get(_ir, _ir->calls[_ir_data(_ir, _idx)].args_begin + _arg_idx);
}

cmon_idx cmon_ir_struct_init_type(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_struct_init);
    return _ir->inits[_ir_data(_ir, _idx)].type_idx;
}

size_t cmon_ir_struct_init_expr_count(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_struct_init);
    return _ir->inits[_ir_data(_ir, _idx)].exprs_end - _ir->inits[_ir_data(_ir, _idx)].exprs_begin;
}

cmon_idx cmon_ir_struct_init_expr(cmon_ir * _ir, cmon_idx _idx, size_t _expr_idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_struct_init);
    return _idx_buf_get(_ir, _ir->inits[_ir_data(_ir, _idx)].exprs_begin + _expr_idx);
}

cmon_idx cmon_ir_array_init_type(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_array_init);
    return _ir->inits[_ir_data(_ir, _idx)].type_idx;
}

size_t cmon_ir_array_init_expr_count(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_array_init);
    return _ir->inits[_ir_data(_ir, _idx)].exprs_end - _ir->inits[_ir_data(_ir, _idx)].exprs_begin;
}

cmon_idx cmon_ir_array_init_expr(cmon_ir * _ir, cmon_idx _idx, size_t _expr_idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_array_init);
    return _idx_buf_get(_ir, _ir->inits[_ir_data(_ir, _idx)].exprs_begin + _expr_idx);
}

cmon_idx cmon_ir_selector_left(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_selector);
    return _ir->idx_pairs[_ir_data(_ir, _idx)].left;
}

const char * cmon_ir_selector_name(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_selector);
    return _ir_str(_ir, _ir->idx_pairs[_ir_data(_ir, _idx)].right);
}

cmon_idx cmon_ir_index_left(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_index);
    return _ir->idx_pairs[_ir_data(_ir, _idx)].left;
}

cmon_idx cmon_ir_index_expr(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_index);
    return _ir->idx_pairs[_ir_data(_ir, _idx)].right;
}

size_t cmon_ir_block_child_count(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_block);
    return _ir->idx_pairs[_ir_data(_ir, _idx)].right - _ir->idx_pairs[_ir_data(_ir, _idx)].left;
}

cmon_idx cmon_ir_block_child(cmon_ir * _ir, cmon_idx _idx, size_t _child_idx)
{
    return _idx_buf_get(_ir, _ir->idx_pairs[_ir_data(_ir, _idx)].left + _child_idx);
}

const char * cmon_ir_var_decl_name(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_var_decl);
    return _ir_str(_ir, _ir->var_decls[_ir_data(_ir, _idx)].name_off);
}

cmon_bool cmon_ir_var_decl_is_mut(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_var_decl);
    return _ir->var_decls[_ir_data(_ir, _idx)].is_mut;
}

cmon_idx cmon_ir_var_decl_type(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_var_decl);
    return _ir->var_decls[_ir_data(_ir, _idx)].type_idx;
}

cmon_idx cmon_ir_var_decl_expr(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_ir_kind(_ir, _idx) == cmon_irk_var_decl);
    return _ir->var_decls[_ir_data(_ir, _idx)].expr_idx;
}

static inline _fn_decl * _ir_fn_data(cmon_ir * _ir, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_ir->fn_data));
    return &_ir->fn_data[_idx];
}

const char * cmon_ir_fn_name(cmon_ir * _ir, cmon_idx _idx)
{
    return _ir_str(_ir, _ir_fn_data(_ir, _ir_data(_ir, _idx))->name_off);
}

cmon_idx cmon_ir_fn_return_type(cmon_ir * _ir, cmon_idx _idx)
{
    return _ir_fn_data(_ir, _ir_data(_ir, _idx))->return_type;
}

size_t cmon_ir_fn_param_count(cmon_ir * _ir, cmon_idx _idx)
{
    return _ir_fn_data(_ir, _ir_data(_ir, _idx))->params_end -
           _ir_fn_data(_ir, _ir_data(_ir, _idx))->params_begin;
}

cmon_idx cmon_ir_fn_param(cmon_ir * _ir, cmon_idx _idx, size_t _param_idx)
{
    return _idx_buf_get(_ir, _ir_fn_data(_ir, _ir_data(_ir, _idx))->params_begin + _param_idx);
}

cmon_idx cmon_ir_fn_body(cmon_ir * _ir, cmon_idx _idx)
{
    return _ir_fn_data(_ir, _ir_data(_ir, _idx))->body_idx;
}

static inline void _debug_write_stmt(
    cmon_ir * _ir, cmon_types * _types, cmon_str_builder * _b, cmon_idx _ir_idx, size_t _indent);

static inline void _debug_write_expr(cmon_ir * _ir,
                                     cmon_types * _types,
                                     cmon_str_builder * _b,
                                     cmon_idx _ir_idx);

static inline void _debug_write_type(cmon_ir * _ir,
                                     cmon_types * _types,
                                     cmon_str_builder * _b,
                                     cmon_idx _type_idx)
{
    cmon_str_builder_append(_b, cmon_types_full_name(_types, _type_idx));
}

static inline void _debug_write_var_decl(cmon_ir * _ir,
                                         cmon_types * _types,
                                         cmon_str_builder * _b,
                                         cmon_idx _ir_idx,
                                         cmon_bool _is_global)
{
    cmon_idx expr = cmon_ir_var_decl_expr(_ir, _ir_idx);
    if (_is_global && !cmon_is_valid_idx(expr))
    {
        cmon_str_builder_append(_b, "extern ");
    }
    cmon_str_builder_append_fmt(_b,
                                "%s%s : %s",
                                cmon_ir_var_decl_is_mut(_ir, _ir_idx) ? "mut " : "",
                                cmon_ir_var_decl_name(_ir, _ir_idx),
                                cmon_types_full_name(_types, cmon_ir_var_decl_type(_ir, _ir_idx)));
    if (cmon_is_valid_idx(expr))
    {
        cmon_str_builder_append(_b, " = ");
        _debug_write_expr(_ir, _types, _b, expr);
    }
}

static inline void _debug_indent(cmon_str_builder * _b, size_t _indent)
{
    for (size_t i = 0; i < _indent; ++i)
    {
        cmon_str_builder_append(_b, "    ");
    }
}

static inline void _debug_write_expr(cmon_ir * _ir,
                                     cmon_types * _types,
                                     cmon_str_builder * _b,
                                     cmon_idx _ir_idx)
{
    cmon_irk kind = cmon_ir_kind(_ir, _ir_idx);
    if (kind == cmon_irk_int_lit || kind == cmon_irk_float_lit || kind == cmon_irk_string_lit)
    {
        cmon_str_builder_append(_b, _ir_str(_ir, _ir_data(_ir, _ir_idx)));
    }
    else if (kind == cmon_irk_ident)
    {
        cmon_str_builder_append(_b, cmon_ir_ident_name(_ir, _ir_idx));
    }
    else if (kind == cmon_irk_bool_lit)
    {
        cmon_str_builder_append(_b, cmon_ir_bool_lit_value(_ir, _ir_idx) ? "true" : "false");
    }
    else if (kind == cmon_irk_noinit)
    {
        cmon_str_builder_append(_b, "---");
    }
    else if (kind == cmon_irk_addr)
    {
        cmon_str_builder_append(_b, "&");
        _debug_write_expr(_ir, _types, _b, cmon_ir_addr_expr(_ir, _ir_idx));
    }
    else if (kind == cmon_irk_deref)
    {
        cmon_str_builder_append(_b, "*");
        _debug_write_expr(_ir, _types, _b, cmon_ir_deref_expr(_ir, _ir_idx));
    }
    else if (kind == cmon_irk_paran_expr)
    {
        cmon_str_builder_append(_b, "(");
        _debug_write_expr(_ir, _types, _b, cmon_ir_paran_expr(_ir, _ir_idx));
        cmon_str_builder_append(_b, ")");
    }
    else if (kind == cmon_irk_call)
    {
        _debug_write_expr(_ir, _types, _b, cmon_ir_call_left(_ir, _ir_idx));
        cmon_str_builder_append(_b, "(");
        for (size_t i = 0; i < cmon_ir_call_arg_count(_ir, _ir_idx); ++i)
        {
            _debug_write_expr(_ir, _types, _b, cmon_ir_call_arg(_ir, _ir_idx, i));
            if (i < cmon_ir_call_arg_count(_ir, _ir_idx) - 1)
                cmon_str_builder_append(_b, ", ");
        }
        cmon_str_builder_append(_b, ")");
    }
    else if (kind == cmon_irk_struct_init)
    {
        cmon_str_builder_append_fmt(
            _b, "%s{", cmon_types_unique_name(_types, cmon_ir_struct_init_type(_ir, _ir_idx)));
        for (size_t i = 0; i < cmon_ir_struct_init_expr_count(_ir, _ir_idx); ++i)
        {
            _debug_write_expr(_ir, _types, _b, cmon_ir_struct_init_expr(_ir, _ir_idx, i));
            if (i < cmon_ir_struct_init_expr_count(_ir, _ir_idx) - 1)
                cmon_str_builder_append(_b, ", ");
        }
        cmon_str_builder_append(_b, "}");
    }
    else if (kind == cmon_irk_array_init)
    {
        cmon_str_builder_append_fmt(
            _b, "%s[", cmon_types_unique_name(_types, cmon_ir_array_init_type(_ir, _ir_idx)));
        for (size_t i = 0; i < cmon_ir_array_init_expr_count(_ir, _ir_idx); ++i)
        {
            _debug_write_expr(_ir, _types, _b, cmon_ir_array_init_expr(_ir, _ir_idx, i));
            if (i < cmon_ir_array_init_expr_count(_ir, _ir_idx) - 1)
                cmon_str_builder_append(_b, ", ");
        }
        cmon_str_builder_append(_b, "]");
    }
    else if (kind == cmon_irk_index)
    {
        _debug_write_expr(_ir, _types, _b, cmon_ir_index_left(_ir, _ir_idx));
        cmon_str_builder_append(_b, "[");
        _debug_write_expr(_ir, _types, _b, cmon_ir_index_expr(_ir, _ir_idx));
        cmon_str_builder_append(_b, "]");
    }
    else if (kind == cmon_irk_selector)
    {
        _debug_write_expr(_ir, _types, _b, cmon_ir_selector_left(_ir, _ir_idx));
        cmon_str_builder_append_fmt(_b, ".%s", cmon_ir_selector_name(_ir, _ir_idx));
    }
    else if (kind == cmon_irk_prefix)
    {
        cmon_str_builder_append_fmt(_b, "%c", cmon_ir_prefix_op(_ir, _ir_idx));
        _debug_write_expr(_ir, _types, _b, cmon_ir_prefix_expr(_ir, _ir_idx));
    }
    else if (kind == cmon_irk_binary)
    {
        _debug_write_expr(_ir, _types, _b, cmon_ir_binary_left(_ir, _ir_idx));
        cmon_str_builder_append_fmt(_b, " %c ", cmon_ir_binary_op(_ir, _ir_idx));
        _debug_write_expr(_ir, _types, _b, cmon_ir_binary_right(_ir, _ir_idx));
    }
    else
    {
        assert(0);
    }
}

static inline void _debug_write_stmt(
    cmon_ir * _ir, cmon_types * _types, cmon_str_builder * _b, cmon_idx _ir_idx, size_t _indent)
{
    cmon_irk kind = cmon_ir_kind(_ir, _ir_idx);
    if (kind == cmon_irk_block)
    {
        _debug_indent(_b, _indent);
        cmon_str_builder_append_fmt(_b, "{\n");
        for (size_t i = 0; i < cmon_ir_block_child_count(_ir, _ir_idx); ++i)
        {
            _debug_write_stmt(_ir, _types, _b, cmon_ir_block_child(_ir, _ir_idx, i), _indent + 1);
        }
        _debug_indent(_b, _indent);
        cmon_str_builder_append(_b, "}\n");
    }
    else if (kind == cmon_irk_var_decl)
    {
        _debug_indent(_b, _indent);
        _debug_write_var_decl(_ir, _types, _b, _ir_idx, cmon_false);
        cmon_str_builder_append(_b, "\n");
    }
    else
    {
        // expr stmt
        _debug_indent(_b, _indent);
        _debug_write_expr(_ir, _types, _b, _ir_idx);
        cmon_str_builder_append(_b, "\n");
    }
}

static inline void _debug_write_fn(cmon_ir * _ir,
                                   cmon_types * _types,
                                   cmon_str_builder * _b,
                                   cmon_idx _ir_idx)
{
    printf("a\n");
    cmon_idx body = cmon_ir_fn_body(_ir, _ir_idx);
    if (!cmon_is_valid_idx(body))
    {
        cmon_str_builder_append(_b, "extern ");
    }
    printf("a2\n");
    cmon_str_builder_append_fmt(_b, "fn %s(", cmon_ir_fn_name(_ir, _ir_idx));
    for (size_t i = 0; i < cmon_ir_fn_param_count(_ir, _ir_idx); ++i)
    {
        _debug_write_var_decl(_ir, _types, _b, cmon_ir_fn_param(_ir, _ir_idx, i), cmon_false);
        if (i < cmon_ir_fn_param_count(_ir, _ir_idx) - 1)
        {
            cmon_str_builder_append(_b, ", ");
        }
    }
    printf("a3\n");
    cmon_str_builder_append(_b, ") -> ");
    printf("DA RET TYPE %lu\n", cmon_ir_fn_return_type(_ir, _ir_idx));
    _debug_write_type(_ir, _types, _b, cmon_ir_fn_return_type(_ir, _ir_idx));
    if (cmon_is_valid_idx(body))
    {
        printf("a4\n");
        cmon_str_builder_append(_b, "\n");
        _debug_write_stmt(_ir, _types, _b, body, 0);
    }
    printf("b\n");
}

const char * cmon_ir_debug_str(cmon_ir * _ir, cmon_types * _types, cmon_str_builder * _b)
{
    cmon_str_builder_clear(_b);

    cmon_str_builder_append(_b, "sorted types:\n");
    for (size_t i = 0; i < _ir->types_count; ++i)
    {
        _debug_write_type(_ir, _types, _b, _ir->types[i]);
        cmon_str_builder_append(_b, "\n");
    }

    cmon_str_builder_append(_b, "\nglobal variables:\n");
    for (size_t i = 0; i < _ir->global_vars_count; ++i)
    {
        _debug_write_var_decl(_ir, _types, _b, _ir->global_vars[i], cmon_true);
        cmon_str_builder_append(_b, "\n");
    }

    cmon_str_builder_append(_b, "\nglobal functions:\n");
    for (size_t i = 0; i < _ir->fns_count; ++i)
    {
        _debug_write_fn(_ir, _types, _b, _ir->fns[i]);
        cmon_str_builder_append(_b, "\n");
    }

    return cmon_str_builder_c_str(_b);
}
