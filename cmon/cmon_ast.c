#include <cmon/cmon_ast.h>
#include <cmon/cmon_dyn_arr.h>

typedef struct
{
    size_t left;
    size_t right;
} _left_right;

typedef struct cmon_ast
{
    cmon_allocator * alloc;
    cmon_ast_kind * kinds;
    cmon_idx * tokens;
    _left_right * left_right;
    size_t count;
    cmon_idx root_block_idx;
} cmon_ast;

typedef struct cmon_astb
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_ast_kind) kinds;
    cmon_dyn_arr(cmon_idx) tokens;
    cmon_dyn_arr(_left_right) left_right;
    cmon_dyn_arr(cmon_idx) extra_data;
    cmon_idx root_block_idx;
    cmon_ast ast; // filled in in cmon_astb_get_ast
} cmon_astb;

cmon_astb * cmon_astb_create(cmon_allocator * _alloc)
{
    cmon_astb * ret = CMON_CREATE(_alloc, cmon_astb);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->kinds, _alloc, 256);
    cmon_dyn_arr_init(&ret->tokens, _alloc, 256);
    cmon_dyn_arr_init(&ret->left_right, _alloc, 256);
    cmon_dyn_arr_init(&ret->extra_data, _alloc, 256);
    ret->root_block_idx = -1;
    return ret;
}

void cmon_astb_destroy(cmon_astb * _b)
{
    cmon_dyn_arr_dealloc(&_b->extra_data);
    cmon_dyn_arr_dealloc(&_b->left_right);
    cmon_dyn_arr_dealloc(&_b->tokens);
    cmon_dyn_arr_dealloc(&_b->kinds);
    CMON_DESTROY(_b->alloc, _b);
}

static inline cmon_idx _add_node(
    cmon_astb * _b, cmon_ast_kind _kind, cmon_idx _tok_idx, cmon_idx _left, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->kinds, _kind);
    cmon_dyn_arr_append(&_b->tokens, _tok_idx);
    cmon_dyn_arr_append(&_b->left_right, ((_left_right){ _left, _right }));
    return cmon_dyn_arr_count(&_b->kinds);
}

// adding expressions
cmon_idx cmon_astb_add_ident(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_ident, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_float_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_float_literal, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_bool_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_bool_literal, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_int_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_int_literal, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_string_lit(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_string_literal, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_binary(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _left, cmon_idx _right)
{
    return _add_node(_b, cmon_ast_kind_binary, _op_tok_idx, _left, _right);
}

cmon_idx cmon_astb_add_prefix(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _right)
{
    return _add_node(_b, cmon_ast_kind_prefix, _op_tok_idx, -1, _right);
}

cmon_idx cmon_astb_add_call(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr_idx, cmon_idx * _arg_indices, size_t _count)
{
    size_t i;
    cmon_idx begin;

    begin = cmon_dyn_arr_count(&_b->extra_data);
    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _arg_indices[i]);
    }

    return _add_node(_b, cmon_ast_kind_call, _tok_idx, begin, cmon_dyn_arr_count(&_b->extra_data));
}

// adding statements
cmon_idx cmon_astb_add_fn_param(cmon_astb * _b,
                                cmon_idx _name_tok_idx,
                                cmon_bool _is_mut,
                                cmon_idx _type)
{
    return _add_node(_b, cmon_ast_kind_fn_param, _name_tok_idx, _is_mut, _type);
}

cmon_idx cmon_astb_add_fn_decl(cmon_astb * _b,
                               cmon_idx _tok_idx,
                               cmon_idx _ret_type,
                               cmon_idx * _params,
                               size_t _count,
                               cmon_idx _block_idx)
{
    size_t i;
    cmon_idx begin;

    begin = cmon_dyn_arr_count(&_b->extra_data);
    cmon_dyn_arr_append(&_b->extra_data, _ret_type);
    cmon_dyn_arr_append(&_b->extra_data, _count);
    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _params[i]);
    }

    return _add_node(_b, cmon_ast_kind_fn_decl, _tok_idx, begin, _block_idx);
}

cmon_idx cmon_astb_add_var_decl(cmon_astb * _b,
                                cmon_idx _name_tok_idx,
                                cmon_bool _is_pub,
                                cmon_bool _is_mut,
                                cmon_idx _type,
                                cmon_idx _expr)
{
    cmon_idx extra_data =
        _add_node(_b, cmon_ast_kind_var_decl_data, (cmon_idx)_is_pub, (cmon_idx)_is_mut, _type);
    return _add_node(_b, cmon_ast_kind_var_decl, _name_tok_idx, extra_data, _expr);
}

cmon_idx cmon_astb_add_block(cmon_astb * _b,
                             cmon_idx _tok_idx,
                             cmon_idx * _stmt_indices,
                             size_t _count)
{
    size_t i;
    cmon_idx begin;

    begin = cmon_dyn_arr_count(&_b->extra_data);

    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _stmt_indices[i]);
    }

    return _add_node(_b, cmon_ast_kind_block, _tok_idx, begin, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_module(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _name_tok_idx)
{
    return _add_node(_b, cmon_ast_kind_module, _tok_idx, _name_tok_idx, CMON_INVALID_IDX);
}

cmon_idx cmon_astb_add_import_pair(cmon_astb * _b,
                                   cmon_idx * _path_toks,
                                   size_t _count,
                                   cmon_idx _alias_tok_idx)
{
    size_t i;
    cmon_idx begin;

    begin = cmon_dyn_arr_count(&_b->extra_data);

    cmon_dyn_arr_append(&_b->extra_data, _count);
    for (i = 1; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _path_toks[i]);
    }
    return _add_node(_b, cmon_ast_kind_import_pair, _path_toks[0], begin, _alias_tok_idx);
}

cmon_idx cmon_astb_add_import(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx * _pairs, size_t _count)
{
    size_t i;
    cmon_idx begin;

    begin = cmon_dyn_arr_count(&_b->extra_data);

    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _pairs[i]);
    }
    return _add_node(
        _b, cmon_ast_kind_import, _tok_idx, begin, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_bool cmon_astb_set_root_block(cmon_astb * _b, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(_b->kinds));
    if (_b->kinds[_idx] == cmon_ast_kind_block)
    {
        _b->root_block_idx = _idx;
        return cmon_false;
    }
    return cmon_true;
}

// cmon_idx cmon_astb_root_block(cmon_astb * _b)
// {
//     return _b->root_block_idx;
// }

// adding parsed types
cmon_idx cmon_astb_add_type_named(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_type_named, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_type_ptr(cmon_astb * _b,
                                cmon_idx _tok_idx,
                                cmon_bool _is_mut,
                                cmon_idx _type_idx)
{
    return _add_node(_b, cmon_ast_kind_type_ptr, _tok_idx, (cmon_idx)_is_mut, _type_idx);
}

// adding type declarations

// getting the ast
cmon_ast * cmon_astb_get_ast(cmon_astb * _b)
{
    _b->ast.alloc = NULL;
    _b->ast.kinds = _b->kinds;
    _b->ast.tokens = _b->tokens;
    _b->ast.left_right = _b->left_right;
    _b->ast.count = cmon_dyn_arr_count(&_b->kinds);
    _b->ast.root_block_idx = _b->root_block_idx;
    return &_b->ast;
}

cmon_ast * cmon_astb_copy_ast(cmon_astb * _b, cmon_allocator * _alloc)
{
}

void cmon_ast_destroy(cmon_ast * _ast)
{
}

// ast getters
cmon_ast_kind cmon_ast_node_kind(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->kinds[_idx];
}

cmon_idx cmon_ast_node_token(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->tokens[_idx];
}

cmon_idx cmon_ast_node_left(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->left_right[_idx].left;
}

cmon_idx cmon_ast_node_right(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->left_right[_idx].right;
}
