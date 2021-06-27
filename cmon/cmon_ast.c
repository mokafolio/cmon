#include <cmon/cmon_ast.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_util.h>
#include <stdarg.h>

typedef struct
{
    cmon_idx left;
    cmon_idx right;
} _left_right;

//@NOTE: For simplycity and nice error messages, we save three tokens for each node for now (which
// is raher wasteful). As a future optimization we can only save the main token and algorithmically
// reconstruct first and last in cmon_ast_token_first and cmon_ast_token_last.
typedef struct
{
    cmon_idx main; // the token of interest, for most nodes this is equivalent to first. For binary
                   // expressions it will be the op token for example.
    cmon_idx first;
    cmon_idx last;
} _tok_range;

typedef struct cmon_ast
{
    cmon_allocator * alloc;
    cmon_tokens * tokens;
    cmon_astk * kinds;
    cmon_idx * main_tokens;
    _left_right * left_right;
    size_t count;
    cmon_idx root_block_idx;
    cmon_idx * extra_data;
    size_t extra_data_count;
} cmon_ast;

typedef struct cmon_astb
{
    cmon_allocator * alloc;
    cmon_tokens * tokens;
    cmon_dyn_arr(cmon_astk) kinds;
    cmon_dyn_arr(cmon_idx) main_tokens;
    cmon_dyn_arr(_left_right) left_right;
    cmon_dyn_arr(cmon_idx) extra_data;
    cmon_dyn_arr(cmon_idx) imports; // we put all imports in one additional list for easy dependency
                                    // tree building later on
    cmon_idx root_block_idx;
    cmon_ast ast; // filled in in cmon_astb_ast
} cmon_astb;

cmon_astb * cmon_astb_create(cmon_allocator * _alloc, cmon_tokens * _tokens)
{
    cmon_astb * ret = CMON_CREATE(_alloc, cmon_astb);
    ret->alloc = _alloc;
    ret->tokens = _tokens;
    cmon_dyn_arr_init(&ret->kinds, _alloc, 256);
    cmon_dyn_arr_init(&ret->main_tokens, _alloc, 256);
    cmon_dyn_arr_init(&ret->left_right, _alloc, 256);
    cmon_dyn_arr_init(&ret->extra_data, _alloc, 256);
    cmon_dyn_arr_init(&ret->imports, _alloc, 8);
    ret->root_block_idx = CMON_INVALID_IDX;
    return ret;
}

void cmon_astb_destroy(cmon_astb * _b)
{
    if (!_b)
        return;

    cmon_dyn_arr_dealloc(&_b->imports);
    cmon_dyn_arr_dealloc(&_b->extra_data);
    cmon_dyn_arr_dealloc(&_b->left_right);
    cmon_dyn_arr_dealloc(&_b->main_tokens);
    cmon_dyn_arr_dealloc(&_b->kinds);
    CMON_DESTROY(_b->alloc, _b);
}

static inline cmon_idx _add_extra_data(cmon_astb * _b, cmon_idx _data)
{
    cmon_dyn_arr_append(&_b->extra_data, _data);
    return cmon_dyn_arr_count(&_b->extra_data) - 1;
}

static inline cmon_idx _add_extra_data_arr(cmon_astb * _b, cmon_idx * _data, size_t _count)
{
    cmon_idx begin = cmon_dyn_arr_count(&_b->extra_data);
    size_t i;
    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_b->extra_data, _data[i]);
    }

    return begin;
}

// static inline cmon_idx _add_extra_data_impl(cmon_astb * _b, cmon_idx * _data, size_t _count, ...)
// {
//     size_t i;
//     cmon_idx begin, idx;
//     va_list args;

//     begin = cmon_dyn_arr_count(&_b->extra_data);

//     va_start(args, _count);
//     while (cmon_is_valid_idx(idx = va_arg(args, cmon_idx)))
//     {
//         printf("adding %lu\n\n\n\n\n", idx);
//         cmon_dyn_arr_append(&_b->extra_data, idx);
//     }
//     va_end(args);

//     for (i = 0; i < _count; ++i)
//     {
//         cmon_dyn_arr_append(&_b->extra_data, _data[i]);
//     }
//     printf("BEGIN %lu END %lu COUNT %lu\n", begin, cmon_dyn_arr_count(&_b->extra_data), _count);

//     return begin;
// }

// #define _add_extra_data_m(_b, _data, _count, ...)                                                  \
//     _add_extra_data_impl(_b, _data, _count, _CMON_VARARG_APPEND_LAST(CMON_INVALID_IDX, __VA_ARGS__))
// #define _add_extra_data(_b, _data, _count)                                                         \
//     _add_extra_data_impl(_b, _data, _count, (cmon_idx)(CMON_INVALID_IDX))

static inline cmon_idx _add_node(
    cmon_astb * _b, cmon_astk _kind, cmon_idx _main_tok, cmon_idx _left, cmon_idx _right)
{
    cmon_dyn_arr_append(&_b->kinds, _kind);
    cmon_dyn_arr_append(&_b->main_tokens, _main_tok);
    cmon_dyn_arr_append(&_b->left_right, ((_left_right){ _left, _right }));
    return cmon_dyn_arr_count(&_b->kinds) - 1;
}

static inline size_t _extra_data_count(cmon_astb * _b)
{
    return cmon_dyn_arr_count(&_b->extra_data);
}

// static inline cmon_idx _node_first_tok(cmon_astb * _b, cmon_idx _idx)
// {
//     CMON_ASSERT(_idx < cmon_dyn_arr_count(&_b->main_tokens));
//     return _b->main_tokens[_idx].first;
// }

// static inline cmon_idx _node_last_tok(cmon_astb * _b, cmon_idx _idx)
// {
//     CMON_ASSERT(_idx < cmon_dyn_arr_count(&_b->main_tokens));
//     return _b->main_tokens[_idx].last;
// }

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

cmon_idx cmon_astb_add_paran(cmon_astb * _b,
                             cmon_idx _open_tok_idx,
                             cmon_idx _close_tok_idx,
                             cmon_idx _expr)
{
    return _add_node(_b, cmon_astk_paran_expr, _open_tok_idx, _expr, _close_tok_idx);
}

cmon_idx cmon_astb_add_call(cmon_astb * _b,
                            cmon_idx _open_tok_idx,
                            cmon_idx _close_tok_idx,
                            cmon_idx _expr_idx,
                            cmon_idx * _arg_indices,
                            size_t _count)
{
    //@NOTE: see note in cmon_astb_add_block
    cmon_idx left = _add_extra_data(_b, _close_tok_idx);
    _add_extra_data(_b, _expr_idx);
    _add_extra_data_arr(_b, _arg_indices, _count);
    return _add_node(_b, cmon_astk_call, _open_tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

// adding statements
cmon_idx cmon_astb_add_fn_decl(cmon_astb * _b,
                               cmon_idx _tok_idx,
                               cmon_idx _ret_type,
                               cmon_idx * _params,
                               size_t _count,
                               cmon_idx _block_idx)
{
    cmon_idx left = _add_extra_data(_b, _ret_type);
    _add_extra_data(_b, _block_idx);
    _add_extra_data_arr(_b, _params, _count);
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
                                   cmon_idx _close_tok_idx,
                                   cmon_idx _parsed_type_idx,
                                   cmon_idx * _fields,
                                   size_t _count)
{
    cmon_idx left = _add_extra_data(_b, _close_tok_idx);
    _add_extra_data(_b, _parsed_type_idx);
    // reserve one extra index for cmon_ast_struct_init_set_resolved_field_idx_buf
    _add_extra_data(_b, CMON_INVALID_IDX);
    _add_extra_data_arr(_b, _fields, _count);
    return _add_node(_b,
                     cmon_astk_struct_init,
                     _b->main_tokens[_parsed_type_idx],
                     left,
                     cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_var_decl(cmon_astb * _b,
                                cmon_idx _name_tok_idx,
                                cmon_bool _is_pub,
                                cmon_bool _is_mut,
                                cmon_idx _type,
                                cmon_idx _expr)
{
    cmon_idx left = _add_extra_data(_b, (cmon_idx)_is_pub);
    _add_extra_data(_b, (cmon_idx)_is_mut);
    _add_extra_data(_b, _type);
    _add_extra_data(_b, CMON_INVALID_IDX);
    return _add_node(_b, cmon_astk_var_decl, _name_tok_idx, left, _expr);
}

cmon_idx cmon_astb_add_selector(cmon_astb * _b,
                                cmon_idx _tok_idx,
                                cmon_idx _left,
                                cmon_idx _name_tok)
{
    //@TODO: store selector tok (_tok_idx)
    cmon_idx right = _add_extra_data(_b, _name_tok);
    _add_extra_data(_b, CMON_INVALID_IDX);
    return _add_node(_b, cmon_astk_selector, _tok_idx, _left, right);
}

cmon_idx cmon_astb_add_array_init(cmon_astb * _b,
                                  cmon_idx _open_tok_idx,
                                  cmon_idx _close_tok_idx,
                                  cmon_idx * _exprs,
                                  size_t _count)
{
    cmon_idx left = _add_extra_data(_b, _close_tok_idx);
    _add_extra_data_arr(_b, _exprs, _count);
    return _add_node(
        _b, cmon_astk_array_init, _open_tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_index(cmon_astb * _b,
                             cmon_idx _open_tok_idx,
                             cmon_idx _close_tok_idx,
                             cmon_idx _lhs,
                             cmon_idx _index_expr)
{
    cmon_idx left = _add_extra_data(_b, _close_tok_idx);
    _add_extra_data(_b, _index_expr);
    return _add_node(_b, cmon_astk_index, _open_tok_idx, left, _lhs);
}

cmon_idx cmon_astb_add_block(cmon_astb * _b,
                             cmon_idx _open_tok_idx,
                             cmon_idx _close_tok_idx,
                             cmon_idx * _stmt_indices,
                             size_t _count)
{
    // @NOTE: in C, the evaluation order of function arguments is unspecified. We need to make sure
    // _add_extra_data is evaluated before getting the array count, hence we need to put it into a
    // tmp variable :/
    cmon_idx left = _add_extra_data(_b, _close_tok_idx);
    _add_extra_data_arr(_b, _stmt_indices, _count);
    return _add_node(_b, cmon_astk_block, _open_tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
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
    cmon_idx left = _add_extra_data(_b, _alias_tok_idx);
    _add_extra_data_arr(_b, _path_toks, _count);
    cmon_idx ret = _add_node(
        _b, cmon_astk_import_pair, _path_toks[0], left, cmon_dyn_arr_count(&_b->extra_data));
    cmon_dyn_arr_append(&_b->imports, ret);
    return ret;
}

cmon_idx cmon_astb_add_import(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx * _pairs, size_t _count)
{
    CMON_ASSERT(_count);
    cmon_idx left = _add_extra_data_arr(_b, _pairs, _count);
    return _add_node(_b, cmon_astk_import, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

cmon_idx cmon_astb_add_alias(cmon_astb * _b,
                             cmon_idx _tok_idx,
                             cmon_idx _name_tok,
                             cmon_bool _is_pub,
                             cmon_idx _parsed_type_idx)
{
    cmon_idx left = _add_extra_data(_b, _name_tok);
    _add_extra_data(_b, (cmon_idx)_is_pub);
    _add_extra_data(_b, CMON_INVALID_IDX);
    return _add_node(_b, cmon_astk_alias, _tok_idx, left, _parsed_type_idx);
}

cmon_idx cmon_astb_add_typedef(cmon_astb * _b,
                               cmon_idx _tok_idx,
                               cmon_idx _name_tok,
                               cmon_bool _is_pub,
                               cmon_idx _parsed_type_idx)
{
    cmon_idx left = _add_extra_data(_b, _name_tok);
    _add_extra_data(_b, (cmon_idx)_is_pub);
    _add_extra_data(_b, CMON_INVALID_IDX);
    return _add_node(_b, cmon_astk_typedef, _tok_idx, left, _parsed_type_idx);
}

void cmon_astb_set_root_block(cmon_astb * _b, cmon_idx _idx)
{
    CMON_ASSERT(_idx < cmon_dyn_arr_count(&_b->kinds));
    CMON_ASSERT(_b->kinds[_idx] == cmon_astk_block);
    _b->root_block_idx = _idx;
}

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

cmon_idx cmon_astb_add_type_view(cmon_astb * _b,
                                 cmon_idx _tok_idx,
                                 cmon_bool _is_mut,
                                 cmon_idx _type_idx)
{
    return _add_node(_b, cmon_astk_type_view, _tok_idx, (cmon_idx)_is_mut, _type_idx);
}

cmon_idx cmon_astb_add_type_array(cmon_astb * _b,
                                  cmon_idx _tok_idx,
                                  size_t _count,
                                  cmon_idx _type_idx)
{
    return _add_node(_b, cmon_astk_type_array, _tok_idx, _count, _type_idx);
}

cmon_idx cmon_astb_add_type_fn(cmon_astb * _b,
                               cmon_idx _tok_idx,
                               cmon_idx _last_tok,
                               cmon_idx _ret_type,
                               cmon_idx * _params,
                               size_t _count)
{
    cmon_idx left = _add_extra_data(_b, _last_tok);
    _add_extra_data(_b, _ret_type);
    _add_extra_data_arr(_b, _params, _count);
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

cmon_idx cmon_astb_add_struct_decl(cmon_astb * _b,
                                   cmon_idx _tok_idx,
                                   cmon_idx _close_tok_idx,
                                   cmon_bool _is_pub,
                                   cmon_idx * _fields,
                                   size_t _count)
{
    //@NOTE: see note in cmon_astb_add_block
    cmon_idx left = _add_extra_data(_b, _close_tok_idx);
    _add_extra_data(_b, (cmon_idx)_is_pub);
    // reserve an index that can be used to store the resolved type index for the struct
    _add_extra_data(_b, CMON_INVALID_IDX);
    _add_extra_data_arr(_b, _fields, _count);
    return _add_node(
        _b, cmon_astk_struct_decl, _tok_idx, left, cmon_dyn_arr_count(&_b->extra_data));
}

// getting the ast
cmon_ast * cmon_astb_ast(cmon_astb * _b)
{
    _b->ast.alloc = NULL;
    _b->ast.kinds = _b->kinds;
    _b->ast.tokens = _b->tokens;
    _b->ast.main_tokens = _b->main_tokens;
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
    CMON_ASSERT(_idx < _ast->count);
    return _ast->kinds[_idx];
}

// static inline cmon_idx _get_token(cmon_ast * _ast, cmon_idx _idx)
// {
//     CMON_ASSERT(_idx < _ast->count);
//     return _ast->main_tokens[_idx].left;
// }

static inline cmon_idx _get_extra_data(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_idx < _ast->extra_data_count);
    return _ast->extra_data[_idx];
}

#define _extra_data_count_def(_name, _kind, _off)                                                  \
    size_t _name(cmon_ast * _ast, cmon_idx _idx)                                                   \
    {                                                                                              \
        CMON_ASSERT(_get_kind(_ast, _idx) == _kind);                                                    \
        return _ast->left_right[_idx].right - (_ast->left_right[_idx].left + _off);                \
    }

#define _extra_data_getter_def(_name, _count_fn_name, _off)                                        \
    cmon_idx _name(cmon_ast * _ast, cmon_idx _idx, size_t _gidx)                                   \
    {                                                                                              \
        CMON_ASSERT(_gidx < _count_fn_name(_ast, _idx));                                                \
        return _get_extra_data(_ast, _ast->left_right[_idx].left + _gidx + _off);                  \
    }

cmon_idx cmon_ast_root_block(cmon_ast * _ast)
{
    return _ast->root_block_idx;
}

size_t cmon_ast_count(cmon_ast * _ast)
{
    return _ast->count;
}

cmon_astk cmon_ast_kind(cmon_ast * _ast, cmon_idx _idx)
{
    return _get_kind(_ast, _idx);
}

cmon_idx cmon_ast_token(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_idx < _ast->count);
    return _ast->main_tokens[_idx];
}

cmon_idx cmon_ast_token_first(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_idx < _ast->count);

    cmon_astk kind = _get_kind(_ast, _idx);
    if (kind == cmon_astk_binary)
    {
        return cmon_ast_token_first(_ast, cmon_ast_binary_left(_ast, _idx));
    }
    else if (kind == cmon_astk_selector)
    {
        return cmon_ast_token_first(_ast, cmon_ast_selector_left(_ast, _idx));
    }
    else if (kind == cmon_astk_index)
    {
        return cmon_ast_token_first(_ast, cmon_ast_index_left(_ast, _idx));
    }

    return cmon_ast_token(_ast, _idx);
}

cmon_idx cmon_ast_token_last(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_idx < _ast->count);

    cmon_astk kind = _get_kind(_ast, _idx);
    if (kind == cmon_astk_binary)
    {
        return cmon_ast_token_last(_ast, cmon_ast_binary_right(_ast, _idx));
    }
    else if (kind == cmon_astk_prefix)
    {
        return cmon_ast_token_last(_ast, cmon_ast_prefix_expr(_ast, _idx));
    }
    else if (kind == cmon_astk_paran_expr)
    {
        return cmon_ast_right(_ast, _idx);
    }
    else if (kind == cmon_astk_struct_field)
    {
        cmon_idx expr = cmon_ast_struct_field_expr(_ast, _idx);
        return cmon_ast_token_last(
            _ast, cmon_is_valid_idx(expr) ? expr : cmon_ast_struct_field_type(_ast, _idx));
    }
    else if (kind == cmon_astk_struct_init_field)
    {
        return cmon_ast_token_last(_ast, cmon_ast_struct_init_field_expr(_ast, _idx));
    }
    else if (kind == cmon_astk_fn_decl)
    {
        return cmon_ast_token_last(_ast, cmon_ast_fn_block(_ast, _idx));
    }
    else if (kind == cmon_astk_deref)
    {
        return cmon_ast_token_last(_ast, cmon_ast_deref_expr(_ast, _idx));
    }
    else if (kind == cmon_astk_addr)
    {
        return cmon_ast_token_last(_ast, cmon_ast_addr_expr(_ast, _idx));
    }
    else if (kind == cmon_astk_selector)
    {
        return cmon_ast_selector_name_tok(_ast, _idx);
    }
    else if (kind == cmon_astk_var_decl)
    {
        return cmon_ast_token_last(_ast, cmon_ast_var_decl_expr(_ast, _idx));
    }
    else if (kind == cmon_astk_import)
    {
        return cmon_ast_token_last(
            _ast, cmon_ast_import_pair(_ast, _idx, cmon_ast_import_pairs_count(_ast, _idx) - 1));
    }
    else if (kind == cmon_astk_import_pair)
    {
        if (cmon_is_valid_idx(cmon_ast_import_pair_alias(_ast, _idx)))
        {
            return cmon_ast_import_pair_alias(_ast, _idx);
        }
        else
        {
            return cmon_ast_import_pair_path_token(
                _ast, _idx, cmon_ast_import_pair_path_token_count(_ast, _idx) - 1);
        }
    }
    else if (kind == cmon_astk_index || kind == cmon_astk_struct_init ||
             kind == cmon_astk_array_init || kind == cmon_astk_call || kind == cmon_astk_fn_decl ||
             kind == cmon_astk_struct_decl || kind == cmon_astk_type_fn ||
             kind == cmon_astk_alias || kind == cmon_astk_typedef || kind == cmon_astk_block)
    {
        return _get_extra_data(_ast, _ast->left_right[_idx].left);
    }
    else if (kind == cmon_astk_ident || kind == cmon_astk_int_literal ||
             kind == cmon_astk_float_literal || kind == cmon_astk_bool_literal ||
             kind == cmon_astk_string_literal)
    {
        return cmon_ast_token(_ast, _idx);
    }

    CMON_ASSERT(0);
    return CMON_INVALID_IDX;
}

cmon_idx cmon_ast_left(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_idx < _ast->count);
    return _ast->left_right[_idx].left;
}

cmon_idx cmon_ast_right(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_idx < _ast->count);
    return _ast->left_right[_idx].right;
}

cmon_idx cmon_ast_extra_data(cmon_ast * _ast, cmon_idx _extra_idx)
{
    CMON_ASSERT(_extra_idx < _ast->extra_data_count);
    return _ast->extra_data[_extra_idx];
}

cmon_idx cmon_ast_module_name_tok(cmon_ast * _ast, cmon_idx _mod_idx)
{
    CMON_ASSERT(_get_kind(_ast, _mod_idx) == cmon_astk_module);
    return _ast->left_right[_mod_idx].left;
}

_extra_data_count_def(cmon_ast_import_pairs_count, cmon_astk_import, 0);
_extra_data_getter_def(cmon_ast_import_pair, cmon_ast_import_pairs_count, 0);

cmon_str_view cmon_ast_import_pair_path(cmon_ast * _ast, cmon_idx _importp_idx)
{
    cmon_str_view b, e;
    CMON_ASSERT(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    b = cmon_tokens_str_view(_ast->tokens, cmon_ast_import_pair_path_token(_ast, _importp_idx, 0));
    e = cmon_tokens_str_view(
        _ast->tokens,
        cmon_ast_import_pair_path_token(
            _ast, _importp_idx, cmon_ast_import_pair_path_token_count(_ast, _importp_idx) - 1));
    return (cmon_str_view){ b.begin, e.end };
}

cmon_idx cmon_ast_import_pair_path_first_tok(cmon_ast * _ast, cmon_idx _importp_idx)
{
    CMON_ASSERT(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    return cmon_ast_import_pair_path_token(_ast, _importp_idx, 0);
}

_extra_data_count_def(cmon_ast_import_pair_path_token_count, cmon_astk_import_pair, 1);
_extra_data_getter_def(cmon_ast_import_pair_path_token, cmon_ast_import_pair_path_token_count, 1);

cmon_idx cmon_ast_import_pair_alias(cmon_ast * _ast, cmon_idx _importp_idx)
{
    CMON_ASSERT(_get_kind(_ast, _importp_idx) == cmon_astk_import_pair);
    return _get_extra_data(_ast, _ast->left_right[_importp_idx].left);
}

cmon_idx cmon_ast_import_pair_ident(cmon_ast * _ast, cmon_idx _importp_idx)
{
    cmon_idx ret = cmon_ast_import_pair_alias(_ast, _importp_idx);
    if (cmon_is_valid_idx(ret))
        return ret;
    return cmon_ast_import_pair_path_token(
        _ast, _importp_idx, cmon_ast_import_pair_path_token_count(_ast, _importp_idx) - 1);
}

cmon_str_view cmon_ast_ident_name(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_ident);
    return cmon_tokens_str_view(_ast->tokens, cmon_ast_token(_ast, _tidx));
}

void cmon_ast_ident_set_sym(cmon_ast * _ast, cmon_idx _tidx, cmon_idx _sym)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_ident);
    _ast->left_right[_tidx].left = _sym;
}

cmon_idx cmon_ast_ident_sym(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_ident);
    return _ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_named_module_tok(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_named);
    return _ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_named_name_tok(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_named);
    return _ast->left_right[_tidx].right;
}

cmon_idx cmon_ast_type_ptr_type(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_ptr);
    return _ast->left_right[_tidx].right;
}

cmon_bool cmon_ast_type_ptr_is_mut(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_ptr);
    return (cmon_bool)_ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_view_type(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_view);
    return _ast->left_right[_tidx].right;
}

cmon_bool cmon_ast_type_view_is_mut(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_view);
    return (cmon_bool)_ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_array_type(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_array);
    return _ast->left_right[_tidx].right;
}

size_t cmon_ast_type_array_count(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_array);
    return (size_t)_ast->left_right[_tidx].left;
}

cmon_idx cmon_ast_type_fn_return_type(cmon_ast * _ast, cmon_idx _tidx)
{
    CMON_ASSERT(_get_kind(_ast, _tidx) == cmon_astk_type_fn);
    return _get_extra_data(_ast, _ast->left_right[_tidx].left + 1);
}

_extra_data_count_def(cmon_ast_type_fn_params_count, cmon_astk_type_fn, 2);
_extra_data_getter_def(cmon_ast_type_fn_param, cmon_ast_type_fn_params_count, 2);

cmon_idx cmon_ast_var_decl_name_tok(cmon_ast * _ast, cmon_idx _vidx)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return cmon_ast_token(_ast, _vidx);
}

cmon_bool cmon_ast_var_decl_is_pub(cmon_ast * _ast, cmon_idx _vidx)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left);
}

cmon_bool cmon_ast_var_decl_is_mut(cmon_ast * _ast, cmon_idx _vidx)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 1);
}

cmon_idx cmon_ast_var_decl_type(cmon_ast * _ast, cmon_idx _vidx)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 2);
}

cmon_idx cmon_ast_var_decl_expr(cmon_ast * _ast, cmon_idx _vidx)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _ast->left_right[_vidx].right;
}

void cmon_ast_var_decl_set_sym(cmon_ast * _ast, cmon_idx _vidx, cmon_idx _sym)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    cmon_idx sym_ed_idx = _ast->left_right[_vidx].left + 3;
    return _ast->extra_data[sym_ed_idx] = _sym;
}

cmon_idx cmon_ast_var_decl_sym(cmon_ast * _ast, cmon_idx _vidx)
{
    CMON_ASSERT(_get_kind(_ast, _vidx) == cmon_astk_var_decl);
    return _get_extra_data(_ast, _ast->left_right[_vidx].left + 3);
}

_extra_data_count_def(cmon_ast_block_child_count, cmon_astk_block, 1);
_extra_data_getter_def(cmon_ast_block_child, cmon_ast_block_child_count, 1);
_extra_data_count_def(cmon_ast_fn_params_count, cmon_astk_fn_decl, 2);
_extra_data_getter_def(cmon_ast_fn_param, cmon_ast_fn_params_count, 2);

cmon_idx cmon_ast_fn_ret_type(cmon_ast * _ast, cmon_idx _fn_idx)
{
    CMON_ASSERT(_get_kind(_ast, _fn_idx) == cmon_astk_fn_decl);
    return _get_extra_data(_ast, _ast->left_right[_fn_idx].left);
}

cmon_idx cmon_ast_fn_block(cmon_ast * _ast, cmon_idx _fn_idx)
{
    CMON_ASSERT(_get_kind(_ast, _fn_idx) == cmon_astk_fn_decl);
    return _get_extra_data(_ast, _ast->left_right[_fn_idx].left + 1);
}

_extra_data_count_def(cmon_ast_struct_fields_count, cmon_astk_struct_decl, 3);
_extra_data_getter_def(cmon_ast_struct_field, cmon_ast_struct_fields_count, 3);

cmon_bool cmon_ast_struct_is_pub(cmon_ast * _ast, cmon_idx _struct_idx)
{
    CMON_ASSERT(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return _get_extra_data(_ast, _ast->left_right[_struct_idx].left + 1);
}

cmon_idx cmon_ast_struct_name(cmon_ast * _ast, cmon_idx _struct_idx)
{
    CMON_ASSERT(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return cmon_ast_token(_ast, _struct_idx);
}

void cmon_ast_struct_set_type(cmon_ast * _ast, cmon_idx _struct_idx, cmon_idx _type_idx)
{
    CMON_ASSERT(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    _ast->extra_data[_ast->left_right[_struct_idx].left + 1] = _type_idx;
}

cmon_idx cmon_ast_struct_type(cmon_ast * _ast, cmon_idx _struct_idx)
{
    CMON_ASSERT(_get_kind(_ast, _struct_idx) == cmon_astk_struct_decl);
    return _get_extra_data(_ast, _ast->left_right[_struct_idx].left + 1);
}

cmon_idx cmon_ast_addr_expr(cmon_ast * _ast, cmon_idx _addr_idx)
{
    CMON_ASSERT(_get_kind(_ast, _addr_idx) == cmon_astk_addr);
    return _ast->left_right[_addr_idx].right;
}

cmon_idx cmon_ast_deref_expr(cmon_ast * _ast, cmon_idx _deref_idx)
{
    CMON_ASSERT(_get_kind(_ast, _deref_idx) == cmon_astk_deref);
    return _ast->left_right[_deref_idx].right;
}

cmon_idx cmon_ast_prefix_op_tok(cmon_ast * _ast, cmon_idx _pref_idx)
{
    CMON_ASSERT(_get_kind(_ast, _pref_idx) == cmon_astk_prefix);
    return cmon_ast_token(_ast, _pref_idx);
}

cmon_idx cmon_ast_prefix_expr(cmon_ast * _ast, cmon_idx _pref_idx)
{
    CMON_ASSERT(_get_kind(_ast, _pref_idx) == cmon_astk_prefix);
    return _ast->left_right[_pref_idx].right;
}

cmon_idx cmon_ast_binary_op_tok(cmon_ast * _ast, cmon_idx _bin_idx)
{
    CMON_ASSERT(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return cmon_ast_token(_ast, _bin_idx);
}

cmon_idx cmon_ast_binary_left(cmon_ast * _ast, cmon_idx _bin_idx)
{
    CMON_ASSERT(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return _ast->left_right[_bin_idx].left;
}

cmon_idx cmon_ast_binary_right(cmon_ast * _ast, cmon_idx _bin_idx)
{
    CMON_ASSERT(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return _ast->left_right[_bin_idx].right;
}

cmon_bool cmon_ast_binary_is_assignment(cmon_ast * _ast, cmon_idx _bin_idx)
{
    CMON_ASSERT(_get_kind(_ast, _bin_idx) == cmon_astk_binary);
    return cmon_tokens_is(_ast->tokens, cmon_ast_binary_op_tok(_ast, _bin_idx), CMON_ASSIGN_TOKS);
}

cmon_idx cmon_ast_paran_expr(cmon_ast * _ast, cmon_idx _paran_idx)
{
    CMON_ASSERT(_get_kind(_ast, _paran_idx) == cmon_astk_paran_expr);
    return _ast->left_right[_paran_idx].left;
}

cmon_idx cmon_ast_selector_left(cmon_ast * _ast, cmon_idx _sel_idx)
{
    CMON_ASSERT(_get_kind(_ast, _sel_idx) == cmon_astk_selector);
    return _ast->left_right[_sel_idx].left;
}

cmon_idx cmon_ast_selector_name_tok(cmon_ast * _ast, cmon_idx _sel_idx)
{
    CMON_ASSERT(_get_kind(_ast, _sel_idx) == cmon_astk_selector);
    return _get_extra_data(_ast, _ast->left_right[_sel_idx].right);
}

void cmon_ast_selector_set_sym(cmon_ast * _ast, cmon_idx _sel_idx, cmon_idx _sym)
{
    _ast->extra_data[_ast->left_right[_sel_idx].right + 1] = _sym;
}

cmon_idx cmon_ast_selector_sym(cmon_ast * _ast, cmon_idx _sel_idx)
{
    return _get_extra_data(_ast, _ast->left_right[_sel_idx].right + 1);
}

cmon_idx cmon_ast_call_left(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_call);
    return _get_extra_data(_ast, _ast->left_right[_idx].left + 1);
}

_extra_data_count_def(cmon_ast_call_args_count, cmon_astk_call, 2);
_extra_data_getter_def(cmon_ast_call_arg, cmon_ast_call_args_count, 2);
_extra_data_count_def(cmon_ast_array_init_exprs_count, cmon_astk_array_init, 1);
_extra_data_getter_def(cmon_ast_array_init_expr, cmon_ast_array_init_exprs_count, 1);

cmon_idx cmon_ast_index_left(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_index);
    return _ast->left_right[_idx].right;
}

cmon_idx cmon_ast_index_expr(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_index);
    return _get_extra_data(_ast, _ast->left_right[_idx].left + 1);
}

cmon_idx cmon_ast_struct_init_field_name_tok(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_init_field);
    return _ast->left_right[_idx].left;
}

cmon_idx cmon_ast_struct_init_field_expr(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_init_field);
    return _ast->left_right[_idx].right;
}

cmon_idx cmon_ast_struct_init_parsed_type(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_init);
    return _get_extra_data(_ast, _ast->left_right[_idx].left + 1);
}

cmon_idx cmon_ast_struct_field_name(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_field);
    return cmon_ast_token(_ast, _idx);
}

cmon_idx cmon_ast_struct_field_type(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_field);
    return _ast->left_right[_idx].left;
}

cmon_idx cmon_ast_struct_field_expr(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_field);
    return _ast->left_right[_idx].right;
}

_extra_data_count_def(cmon_ast_struct_init_fields_count, cmon_astk_struct_init, 3);
_extra_data_getter_def(cmon_ast_struct_init_field, cmon_ast_struct_init_fields_count, 3);

void cmon_ast_struct_init_set_resolved_field_idx_buf(cmon_ast * _ast,
                                                     cmon_idx _idx,
                                                     cmon_idx _idx_buf)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_init);
    _ast->extra_data[_ast->left_right[_idx].left + 1] = _idx_buf;
}

cmon_idx cmon_ast_struct_init_resolved_field_idx_buf(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_struct_init);
    return _get_extra_data(_ast, _ast->left_right[_idx].left + 1);
}

cmon_idx cmon_ast_alias_name_tok(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_alias);
    return _get_extra_data(_ast, _ast->left_right[_idx].left);
}

cmon_bool cmon_ast_alias_is_pub(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_alias);
    return (cmon_bool)_get_extra_data(_ast, _ast->left_right[_idx].left + 1);
}

cmon_idx cmon_ast_alias_parsed_type(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_alias);
    return _ast->left_right[_idx].right;
}

void cmon_ast_alias_set_sym(cmon_ast * _ast, cmon_idx _idx, cmon_idx _sym)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_alias);
    return _ast->extra_data[_ast->left_right[_idx].left + 2] = _sym;
}

cmon_idx cmon_ast_alias_sym(cmon_ast * _ast, cmon_idx _idx)
{
    CMON_ASSERT(_get_kind(_ast, _idx) == cmon_astk_alias);
    return _get_extra_data(_ast, _ast->left_right[_idx].left + 2);
}
