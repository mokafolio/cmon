#include <cmon/cmon_ast.h>
#include <cmon/cmon_dyn_arr.h>

typedef struct cmon_ast
{
    cmon_allocator * alloc;
    cmon_ast_kind * kinds;
    cmon_idx * tokens;
    cmon_idx * lefts;
    cmon_idx * rights;
    size_t count;
} cmon_ast;

typedef struct cmon_astb
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_ast_kind) kinds;
    cmon_dyn_arr(cmon_idx) tokens;
    cmon_dyn_arr(cmon_idx) lefts;
    cmon_dyn_arr(cmon_idx) rights;
    cmon_ast ast; // filled in in cmon_astb_get_ast
} cmon_astb;

cmon_astb * cmon_astb_create(cmon_allocator * _alloc)
{
    cmon_astb * ret = CMON_CREATE(_alloc, cmon_astb);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->kinds, _alloc, 256);
    cmon_dyn_arr_init(&ret->tokens, _alloc, 256);
    cmon_dyn_arr_init(&ret->lefts, _alloc, 256);
    cmon_dyn_arr_init(&ret->rights, _alloc, 256);
    return ret;
}

void cmon_astb_destroy(cmon_astb * _b)
{
    cmon_dyn_arr_dealloc(&_b->rights);
    cmon_dyn_arr_dealloc(&_b->lefts);
    cmon_dyn_arr_dealloc(&_b->tokens);
    cmon_dyn_arr_dealloc(&_b->kinds);
    CMON_DESTROY(_b->alloc, _b);
}

static inline cmon_idx _add_node(
    cmon_astb * _b, cmon_ast_kind _kind, cmon_idx _tok_idx, cmon_idx _left, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->kinds, _kind);
    cmon_dyn_arr_append(&_b->tokens, _tok_idx);
    cmon_dyn_arr_append(&_b->lefts, _left);
    cmon_dyn_arr_append(&_b->rights, _right);
    return cmon_dyn_arr_count(&_b->kinds);
}

// adding expressions
cmon_idx cmon_astb_add_ident(cmon_astb * _b, cmon_idx _tok_idx)
{
    return _add_node(_b, cmon_ast_kind_ident, _tok_idx, -1, -1);
}

cmon_idx cmon_astb_add_binary(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _left, cmon_idx _right)
{
    return _add_node(_b, cmon_ast_kind_binary, _op_tok_idx, _left, _right);
}

cmon_idx cmon_astb_add_prefix(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _right)
{
    return _add_node(_b, cmon_ast_kind_prefix, _op_tok_idx, -1, _right);
}

// adding statements
cmon_idx cmon_astb_add_block(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _begin, cmon_idx _end)
{
    return _add_node(_b, cmon_ast_kind_block, _tok_idx, _begin, _end);
}

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
    _b->ast.lefts = _b->lefts;
    _b->ast.rights = _b->rights;
    _b->ast.count = cmon_dyn_arr_count(&_b->kinds);
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
    return _ast->lefts[_idx];
}

cmon_idx cmon_ast_node_right(cmon_ast * _ast, cmon_idx _idx)
{
    assert(_idx < _ast->count);
    return _ast->rights[_idx];
}
