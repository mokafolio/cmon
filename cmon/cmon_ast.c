#include <cmon/cmon_ast.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_tokens.h>
#include <cmon/cmon_util.h>
#include <stdarg.h>

typedef struct
{
    cmon_idx left;
    cmon_idx right;
} _left_right;

typedef struct cmon_ast
{
    cmon_allocator * alloc;
    cmon_astk * kinds;
    cmon_idx * tokens;
    _left_right * left_right;
    size_t count;
    cmon_idx root_block_idx;
    cmon_idx * extra_data;
    size_t extra_data_count;
} cmon_ast;

typedef struct cmon_astb
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_astk) kinds;
    cmon_dyn_arr(cmon_idx) tokens;
    cmon_dyn_arr(_left_right) left_right;
    cmon_dyn_arr(cmon_idx) extra_data;
    cmon_dyn_arr(cmon_idx) imports; // we put all imports in one additional list for easy dependency
                                    // tree building later on
    cmon_idx root_block_idx;
    cmon_ast ast; // filled in in cmon_astb_ast
} cmon_astb;

cmon_astb * cmon_astb_create(cmon_allocator * _alloc)
{
    cmon_astb * ret = CMON_CREATE(_alloc, cmon_astb);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->kinds, _alloc, 256);
    cmon_dyn_arr_init(&ret->tokens, _alloc, 256);
    cmon_dyn_arr_init(&ret->left_right, _alloc, 256);
    cmon_dyn_arr_init(&ret->extra_data, _alloc, 256);
    cmon_dyn_arr_init(&ret->imports, _alloc, 8);
    ret->root_block_idx = CMON_INVALID_IDX;
    return ret;
}

void cmon_astb_destroy(cmon_astb * _b)
{
    cmon_dyn_arr_dealloc(&_b->imports);
    cmon_dyn_arr_dealloc(&_b->extra_data);
    cmon_dyn_arr_dealloc(&_b->left_right);
    cmon_dyn_arr_dealloc(&_b->tokens);
    cmon_dyn_arr_dealloc(&_b->kinds);
    CMON_DESTROY(_b->alloc, _b);
}

static inline cmon_idx _add_extra_data_impl(cmon_astb * _b, cmon_idx * _data, size_t _count, ...)
{
    size_t i;
    cmon_idx begin, idx;
    va_list args;

    begin = cmon_dyn_arr_count(&_b->extra_data);

    va_start(args, _count);
    while (cmon_is_valid_idx(idx = va_arg(args, cmon_idx)))
    {
        printf("adding %lu\n", idx);
        cmon_dyn_arr_append(&_b->extra_data, idx);
    }
    va_end(args);

    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _data[i]);
    }
    printf("BEGIN %lu END %lu COUNT %lu\n", begin, cmon_dyn_arr_count(&_b->extra_data), _count);

    return begin;
}

#define _add_extra_data_m(_b, _data, _count, ...)                                                  \
    _add_extra_data_impl(_b, _data, _count, _CMON_VARARG_APPEND_LAST(CMON_INVALID_IDX, __VA_ARGS__))
#define _add_extra_data(_b, _data, _count)                                                         \
    _add_extra_data_impl(_b, _data, _count, (cmon_idx)(CMON_INVALID_IDX))

static inline cmon_idx _add_node(
    cmon_astb * _b, cmon_astk _kind, cmon_idx _tok_idx, cmon_idx _left, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->kinds, _kind);
    cmon_dyn_arr_append(&_b->tokens, _tok_idx);
    printf("adding node %lu %lu %lu\n", _left, _right, cmon_dyn_arr_count(&_b->extra_data));
    cmon_dyn_arr_append(&_b->left_right, ((_left_right){ _left, _right }));
    return cmon_dyn_arr_count(&_b->kinds) - 1;
}

static size_t _extra_data_count(cmon_astb * _b)
{
    return cmon_dyn_arr_count(&_b->extra_data);
}

// adding expressions
cmon_idx cmon_astb_add_ident(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_astk_ident, _tok_idx, CMON_INVALID_IDX, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_float_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_astk_float_literal, _tok_idx, CMON_INVALID_IDX, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_bool_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_astk_bool_literal, _tok_idx, CMON_INVALID_IDX, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_int_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_astk_int_literal, _tok_idx, CMON_INVALID_IDX, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_string_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_astk_string_literal, _tok_idx, CMON_INVALID_IDX, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_addr(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr)
{
    return _add_node(_b, cmon_astk_addr, _tok_idx, CMON_INVALID_IDX, _expr);
}

cmon_idx cmon_astb_add_deref(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr)
{
    return _add_node(_b, cmon_astk_deref, _tok_idx, CMON_INVALID_IDX, _expr);
}

cmon_idx cmon_astb_add_binary(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _left, cmon_idx _right)
{
    return _add_node(_b, cmon_astk_binary, _op_tok_idx, _left, _right);
}

cmon_idx cmon_astb_add_prefix(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _right)
{
    return _add_node(_b, cmon_astk_prefix, _op_tok_idx, CMON_INVALID_IDX, _right);
}

cmon_idx cmon_astb_add_paran(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr)
{
    return _add_node(_b, cmon_astk_paran_expr, _tok_idx, _expr, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_call(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr_idx, cmon_idx * _arg_indices, size_t _count)
{
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data(_b, _arg_indices, _count);
    return _add_node(_b, cmon_astk_call, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

// adding statements
cmon_idx cmon_astb_add_fn_decl(cmon_astb * _b,
                               cmon_idx _tok_idx,
                               cmon_idx _ret_type,
                               cmon_idx * _params,
                               size_t _count,
                               cmon_idx _block_idx)
{
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _params, _count, _ret_type, _block_idx);
    return _add_node(_b, cmon_astk_fn_decl, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_struct_init_field(cmon_astb * _b,
                                         cmon_idx _first_tok,
                                         cmon_idx _name_tok,
                                         cmon_idx _expr)
{
    return _add_node(_b, cmon_astk_struct_init_field, _first_tok, _name_tok, _expr);
}

cmon_idx cmon_astb_add_struct_init(cmon_astb * _b,
                                   cmon_idx _tok_idx,
                                   cmon_idx * _fields,
                                   size_t _count)
{
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data(_b, _fields, _count);
    return _add_node(
        _b, cmon_astk_struct_init, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_var_decl(cmon_astb * _b,
                                cmon_idx _name_tok_idx,
                                cmon_bool _is_pub,
                                cmon_bool _is_mut,
                                cmon_idx _type,
                                cmon_idx _expr)
{
    // cmon_idx extra_data =
    //     _add_node(_b, cmon_astk_var_decl_data, (cmon_idx)_is_pub, (cmon_idx)_is_mut, _type);
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, NULL, 0, (cmon_idx)_is_pub, (cmon_idx)_is_mut, _type);
    return _add_node(_b, cmon_astk_var_decl, _name_tok_idx, left, _expr);
}

cmon_idx cmon_astb_add_var_decl_list(cmon_astb * _b,
                                     cmon_idx * _name_toks,
                                     size_t _count,
                                     cmon_bool _is_pub,
                                     cmon_bool _is_mut,
                                     cmon_idx _type,
                                     cmon_idx _expr)
{
    assert(_count);
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _name_toks, _count, _is_pub, _is_mut, _type, _expr);
    return _add_node(
        _b, cmon_astk_var_decl_list, _name_toks[0], left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_block(cmon_astb * _b,
                             cmon_idx _tok_idx,
                             cmon_idx * _stmt_indices,
                             size_t _count)
{
    // @NOTE: in C, the evaluation order of function arguments is unspecified. We need to make sure
    // _add_extra_data is evaluated before getting the array count, hence we need to put it into a
    // tmp variable :/
    cmon_idx left = _add_extra_data(_b, _stmt_indices, _count);
    return _add_node(_b, cmon_astk_block, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_module(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _name_tok_idx)
{
    return _add_node(_b, cmon_astk_module, _tok_idx, _name_tok_idx, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_import_pair(cmon_astb * _b,
                                   cmon_idx * _path_toks,
                                   size_t _count,
                                   cmon_idx _alias_tok_idx)
{
    cmon_idx ret, left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _path_toks, _count, _alias_tok_idx);
    ret = _add_node(
        _b, cmon_astk_import_pair, _path_toks[0], left, cmon_dyn_arr_count(&_b->extra_data));
    cmon_dyn_arr_append(&_b->imports, ret);
    return ret;
}

cmon_idx cmon_astb_add_import(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx * _pairs, size_t _count)
{
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data(_b, _pairs, _count);
    return _add_node(_b, cmon_astk_import, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_fn_param(cmon_astb * _b,
                                cmon_idx _name_tok_idx,
                                cmon_bool _is_mut,
                                cmon_idx _type)
{
    return _add_node(_b, cmon_astk_fn_param, _name_tok_idx, _is_mut, _type);
}

cmon_idx cmon_astb_add_fn_param_list(
    cmon_astb * _b, cmon_idx * _name_toks, size_t _count, cmon_bool _is_mut, cmon_idx _type)
{
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _name_toks, _count, _is_mut, _type);
    return _add_node(_b,
                     cmon_astk_fn_param_list,
                     _name_toks[0],
                     _add_extra_data_m(_b, _name_toks, _count, _is_mut, _type),
                     cmon_dyn_arr_count(&_b->extra_data));
}

void cmon_astb_set_root_block(cmon_astb * _b, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_b->kinds));
    assert(_b->kinds[_idx] == cmon_astk_block);
    _b->root_block_idx = _idx;
}

// cmon_idx cmon_astb_root_block(cmon_astb * _b)
// {
//     return _b->root_block_idx;
// }

// adding parsed types
cmon_idx cmon_astb_add_type_named(cmon_astb * _b, cmon_idx _mod_tok_idx, cmon_idx _tok_idx)
{
    return _add_node(_b,
                     cmon_astk_type_named,
                     cmon_is_valid_idx(_mod_tok_idx) ? _mod_tok_idx : _tok_idx,
                     _mod_tok_idx,
                     _tok_idx);
}

cmon_idx cmon_astb_add_type_ptr(cmon_astb * _b,
                                cmon_idx _tok_idx,
                                cmon_bool _is_mut,
                                cmon_idx _type_idx)
{
    return _add_node(_b, cmon_astk_type_ptr, _tok_idx, (cmon_idx)_is_mut, _type_idx);
}

cmon_idx cmon_astb_add_type_fn(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _ret_type, cmon_idx * _params, size_t _count)
{
    assert(_count);
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _params, _count, _ret_type);
    return _add_node(_b, cmon_astk_type_fn, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

// adding type declarations
cmon_idx cmon_astb_add_struct_field(cmon_astb * _b,
                                    cmon_idx _name_tok,
                                    cmon_idx _type,
                                    cmon_idx _expr)
{
    return _add_node(_b, cmon_astk_struct_field, _name_tok, _type, _expr);
}

cmon_idx cmon_astb_add_struct_field_list(
    cmon_astb * _b, cmon_idx * _name_toks, size_t _count, cmon_idx _type, cmon_idx _expr)
{
    assert(_count);
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _name_toks, _count, _type, _expr);
    return _add_node(
        _b, cmon_astk_struct_field_list, _name_toks[0], left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_struct_decl(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_bool _is_pub, cmon_idx * _fields, size_t _count)
{
    cmon_idx left;
    //@NOTE: see note in cmon_astb_add_block
    left = _add_extra_data_m(_b, _fields, _count, _is_pub);
    return _add_node(
        _b, cmon_astk_struct_decl, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

// getting the ast
cmon_ast * cmon_astb_ast(cmon_astb * _b)
{
    _b->ast.alloc = NULL;
    _b->ast.kinds = _b->kinds;
    _b->ast.tokens = _b->tokens;
    _b->ast.left_right = _b->left_right;
    _b->ast.count = cmon_dyn_arr_count(&_b->kinds);
    _b->ast.root_block_idx = _b->root_block_idx;
    _b->ast.extra_data = _b->extra_data;
    _b->ast.extra_data_count = cmon_dyn_arr_count(&_b->extra_data);
    return &_b->ast;
}

cmon_ast * cmon_astb_copy_ast(cmon_astb * _b, cmon_allocator * _alloc)
{
}

void cmon_ast_destroy(cmon_ast * _ast)
{
}

// ast getters
static inline cmon_astk _get_kind(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->kinds[_idx];
}

static inline cmon_idx _get_token(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->tokens[_idx];
}

static inline cmon_idx _get_extra_data(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->extra_data_count);
    return _ast->extra_data[_idx];
}

cmon_idx cmon_ast_root_block(cmon_ast * _ast)
{
    return _ast->root_block_idx;
}

cmon_astk cmon_ast_kind(cmon_ast * _ast, cmon_idx _idx)
{
    return _get_kind(_ast, _idx);
}

cmon_idx cmon_ast_token(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->tokens[_idx];
}

cmon_idx cmon_ast_left(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->left_right[_idx].left;
}

cmon_idx cmon_ast_right(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->left_right[_idx].right;
}

cmon_idx cmon_ast_extra_data(cmon_ast * _ast, cmon_idx _extra_idx)
{
    assert(_extra_idx < _ast->extra_data_count);
    return _ast->extra_data[_extra_idx];
}

cmon_idx cmon_ast_iter_next(cmon_ast * _ast, cmon_ast_iter * _it)
{
    cmon_idx idx = _it->idx;
    _it->idx = ++_it->idx == _it->end ? CMON_INVALID_IDX : _it->idx;
    return idx != CMON_INVALID_IDX ? _ast->extra_data[idx] : idx;
}

cmon_idx cmon_ast_module_name_tok(cmon_ast * _ast, cmon_idx _mod_idx)
{
    assert(_get_kind(_ast, _mod_idx) == cmon_astk_module);
    return _ast->left_right[_mod_idx].left;
}

cmon_idx cmon_ast_import_begin(cmon_ast * _ast, cmon_idx _import_idx)
{
    assert(_get_kind(_ast, _import_idx) == cmon_astk_import);
    return _ast->left_right[_import_idx].left;
}

cmon_idx cmon_ast_import_end(cmon_ast * _ast, cmon_idx _import_idx)
{
    assert(_get_kind(_ast, _import_idx) == cmon_astk_import);
    return _ast->left_right[_import_idx].right;
}

cmon_ast_iter cmon_ast_import_iter(cmon_ast * _ast, cmon_idx _import_idx)
{
    return (cmon_ast_iter){ cmon_ast_import_begin(_ast, _import_idx),
                            cmon_ast_import_end(_ast, _import_idx) };
}

cmon_str_view cmon_ast_import_pair_path(cmon_ast * _ast, cmon_idx _importp_idx)
{
    cmon_str_view b, e;
    assert(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    b = cmon_tokens_str_view(_ast->tokens, cmon_ast_import_pair_path_begin(_ast, _importp_idx));
    e = cmon_tokens_str_view(_ast->tokens, cmon_ast_import_pair_path_end(_ast, _importp_idx));
    return (cmon_str_view){ b.begin, e.end };
}

cmon_idx cmon_ast_import_pair_path_begin(cmon_ast * _ast, cmon_idx _importp_idx)
{
    assert(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    return _ast->left_right[_importp_idx].left + 1;
}

cmon_idx cmon_ast_import_pair_path_end(cmon_ast * _ast, cmon_idx _importp_idx)
{
    assert(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    return _ast->left_right[_importp_idx].right;
}

cmon_ast_iter cmon_ast_import_pair_path_iter(cmon_ast * _ast, cmon_idx _importp_idx)
{
    return (cmon_ast_iter){ cmon_ast_import_pair_path_begin(_ast, _importp_idx),
                            cmon_ast_import_pair_path_end(_ast, _importp_idx) };
}

cmon_idx cmon_ast_import_pair_alias(cmon_ast * _ast, cmon_idx _importp_idx)
{
    assert(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    return _get_extra_data(_ast, _ast->left_right[_importp_idx].left);
}

cmon_idx cmon_ast_type_named_module_tok(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_named);
    return _ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_named_name_tok(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_named);
    return _ast->left_right[_tidx].right;
}

cmon_idx cmon_ast_type_ptr_type(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_ptr);
    return _ast->left_right[_tidx].right;
}

cmon_bool cmon_ast_type_ptr_is_mut(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_ptr);
    return (cmon_bool)_ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_fn_return_type(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_fn);
    return _get_extra_data(_ast, _ast->left_right[_tidx].left);
}

cmon_idx cmon_ast_type_fn_params_begin(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_fn);
    return _get_extra_data(_ast, _ast->left_right[_tidx].left + 1);
}

cmon_idx cmon_ast_type_fn_params_end(cmon_ast * _ast, cmon_idx _tidx)
{
    assert(_get_kind(_ast, _tidx) == cmon_astk_type_fn);
    return _get_extra_data(_ast, _ast->left_right[_tidx].right);
}

cmon_ast_iter cmon_ast_type_fn_params_iter(cmon_ast * _ast, cmon_idx _tidx)
{
    return (cmon_ast_iter){ cmon_ast_type_fn_params_begin(_ast, _tidx),
                            cmon_ast_type_fn_params_end(_ast, _tidx) };
}

cmon_idx cmon_ast_var_decl_name_tok(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return cmon_ast_token(_ast, _vidx);
}

cmon_bool cmon_ast_var_decl_is_pub(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left);
}

cmon_bool cmon_ast_var_decl_is_mut(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 1);
}

cmon_idx cmon_ast_var_decl_type(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 2);
}

cmon_idx cmon_ast_var_decl_expr(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _ast->left_right[_vidx].right;
}

cmon_idx cmon_ast_var_decl_list_names_begin(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl_list);
    return _ast->left_right[_vidx].left + 4;
}

cmon_idx cmon_ast_var_decl_list_names_end(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl_list);
    return _ast->left_right[_vidx].right;
}

cmon_ast_iter cmon_ast_var_decl_list_names_iter(cmon_ast * _ast, cmon_idx _vidx)
{
    return (cmon_ast_iter){ cmon_ast_var_decl_list_names_begin(_ast, _vidx),
                            cmon_ast_var_decl_list_names_end(_ast, _vidx) };
}

cmon_bool cmon_ast_var_decl_list_is_pub(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl_list);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left);
}

cmon_bool cmon_ast_var_decl_list_is_mut(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl_list);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 1);
}

cmon_idx cmon_ast_var_decl_list_type(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl_list);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 2);
}

cmon_idx cmon_ast_var_decl_list_expr(cmon_ast * _ast, cmon_idx _vidx)
{
    assert(_get_kind(_ast, _vidx) == cmon_astk_var_decl_list);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 3);
}

cmon_idx cmon_ast_block_begin(cmon_ast * _ast, cmon_idx _block_idx)
{
    assert(_get_kind(_ast, _block_idx) == cmon_astk_block);
    return _ast->left_right[_block_idx].left;
}

cmon_idx cmon_ast_block_end(cmon_ast * _ast, cmon_idx _block_idx)
{
    assert(_get_kind(_ast, _block_idx) == cmon_astk_block);
    return _ast->left_right[_block_idx].right;
}

cmon_ast_iter cmon_ast_block_iter(cmon_ast * _ast, cmon_idx _block_idx)
{
    return (cmon_ast_iter){ cmon_ast_block_begin(_ast, _block_idx),
                            cmon_ast_block_end(_ast, _block_idx) };
}

cmon_idx cmon_ast_fn_params_begin(cmon_ast * _ast, cmon_idx _fn_idx)
{
    assert(_get_kind(_ast, _fn_idx) == cmon_astk_fn_decl);
    return _ast->left_right[_fn_idx].left + 2;
}

cmon_idx cmon_ast_fn_params_end(cmon_ast * _ast, cmon_idx _fn_idx)
{
    assert(_get_kind(_ast, _fn_idx) == cmon_astk_fn_decl);
    return _ast->left_right[_fn_idx].right;
}

cmon_ast_iter cmon_ast_fn_params_iter(cmon_ast * _ast, cmon_idx _fn_idx)
{
    return (cmon_ast_iter){ cmon_ast_fn_params_begin(_ast, _fn_idx),
                            cmon_ast_fn_params_end(_ast, _fn_idx) };
}

cmon_idx cmon_ast_fn_ret_type(cmon_ast * _ast, cmon_idx _fn_idx)
{
    assert(_get_kind(_ast, _fn_idx) == cmon_astk_fn_decl);
    return _get_extra_data(_ast, _ast->left_right[_fn_idx].left);
}

cmon_idx cmon_ast_fn_block(cmon_ast * _ast, cmon_idx _fn_idx)
{
    assert(_get_kind(_ast, _fn_idx) == cmon_astk_fn_decl);
    return _get_extra_data(_ast, _ast->left_right[_fn_idx].left + 1);
}

cmon_idx cmon_ast_struct_fields_begin(cmon_ast * _ast, cmon_idx _struct_idx)
{
    assert(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return _get_extra_data(_ast, _ast->left_right[_struct_idx].left + 1);
}

cmon_idx cmon_ast_struct_fields_end(cmon_ast * _ast, cmon_idx _struct_idx)
{
    assert(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return _get_extra_data(_ast, _ast->left_right[_struct_idx].right);
}

cmon_ast_iter cmon_ast_struct_fields_iter(cmon_ast * _ast, cmon_idx _struct_idx)
{
    return (cmon_ast_iter){ cmon_ast_struct_fields_begin(_ast, _struct_idx),
                            cmon_ast_struct_fields_end(_ast, _struct_idx) };
}

cmon_bool cmon_ast_struct_is_pub(cmon_ast * _ast, cmon_idx _struct_idx)
{
    assert(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return _get_extra_data(_ast, _ast->left_right[_struct_idx].left);
}

cmon_bool cmon_ast_struct_name(cmon_ast * _ast, cmon_idx _struct_idx)
{
    assert(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return cmon_ast_token(_ast, _struct_idx);
}

cmon_idx cmon_ast_addr_expr(cmon_ast * _ast, cmon_idx _addr_idx)
{
    assert(_get_kind(_ast, _addr_idx) == cmon_astk_addr);
    return _ast->left_right[_addr_idx].right;
}

cmon_idx cmon_ast_deref_expr(cmon_ast * _ast, cmon_idx _deref_idx)
{
    assert(_get_kind(_ast, _deref_idx) == cmon_astk_deref);
    return _ast->left_right[_deref_idx].right;
}

cmon_idx cmon_ast_prefix_op_tok(cmon_ast * _ast, cmon_idx _pref_idx)
{
    assert(_get_kind(_ast, _pref_idx) == cmon_astk_prefix);
    return cmon_ast_token(_ast, _pref_idx);
}

cmon_idx cmon_ast_prefix_expr(cmon_ast * _ast, cmon_idx _pref_idx)
{
    assert(_get_kind(_ast, _pref_idx) == cmon_astk_prefix);
    return _ast->left_right[_pref_idx].right;
}

cmon_idx cmon_ast_binary_op_tok(cmon_ast * _ast, cmon_idx _bin_idx)
{
    assert(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return cmon_ast_token(_ast, _bin_idx);
}

cmon_idx cmon_ast_binary_left(cmon_ast * _ast, cmon_idx _bin_idx)
{
    assert(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return _ast->left_right[_bin_idx].left;
}

cmon_idx cmon_ast_binary_right(cmon_ast * _ast, cmon_idx _bin_idx)
{
    assert(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return _ast->left_right[_bin_idx].right;
}
