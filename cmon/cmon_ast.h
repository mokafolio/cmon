#ifndef CMON_CMON_AST_H
#define CMON_CMON_AST_H

#include <cmon/cmon_allocator.h>

typedef enum
{
    cmon_ast_kind_ident,
    cmon_ast_kind_int_literal,
    cmon_ast_kind_float_literal,
    cmon_ast_kind_string_literal,
    cmon_ast_kind_bool_literal,
    cmon_ast_kind_type_named,
    cmon_ast_kind_type_ptr,
    cmon_ast_kind_type_tuple,
    cmon_ast_kind_call,
    cmon_ast_kind_index,
    cmon_ast_kind_selector,
    cmon_ast_kind_struct_init,
    cmon_ast_kind_array_init,
    cmon_ast_kind_tuple_init,
    cmon_ast_kind_view_init,
    cmon_ast_kind_prefix,
    cmon_ast_kind_binary,
    // cmon_ast_kind_postfix,
    cmon_ast_kind_addr,
    cmon_ast_kind_deref,
    cmon_ast_kind_cast,
    // cmon_ast_kind_view,
    cmon_ast_kind_noinit,
    cmon_ast_kind_fn_param,      // i.e. mut bar : s32
    cmon_ast_kind_fn_param_list, // i.e. foo, bar : s32
    cmon_ast_kind_fn_decl,
    // cmon_ast_kind_range,
    // cmon_ast_kind_expl_template_fn_init,
    cmon_ast_kind_var_decl, 
    cmon_ast_kind_var_decl_list, //i.e. foo, bar : s32 = 3
    cmon_ast_kind_var_decl_data,
    cmon_ast_kind_struct_field,
    cmon_ast_kind_struct_decl,
    // cmon_ast_kind_interface_decl,
    cmon_ast_kind_exprstmt,
    cmon_ast_kind_return,
    cmon_ast_kind_break,
    cmon_ast_kind_continue,
    cmon_ast_kind_for,
    cmon_ast_kind_for_in,
    // cmon_ast_kind_c_for,
    cmon_ast_kind_block,
    cmon_ast_kind_import_pair, // the foo.bar as baz part of import foo.bar as baz
    cmon_ast_kind_import,
    cmon_ast_kind_module,
    cmon_ast_kind_typedef,
    cmon_ast_kind_typealias,
    cmon_ast_kind_if,
    cmon_ast_kind_defer,
} cmon_ast_kind;

// typedef struct cmon_ast_node
// {
//     cmon_ast_kind kind;
//     size_t token_idx, left_idx, right_idx;
// } cmon_ast_node;

typedef struct cmon_ast cmon_ast;
// ast builder
typedef struct cmon_astb cmon_astb;

CMON_API cmon_astb * cmon_astb_create(cmon_allocator * _alloc);
CMON_API void cmon_astb_destroy(cmon_astb * _b);

// buffering extra data (i.e. all the statement indices of a block)
// CMON_API cmon_idx cmon_astb_add_extra_data(cmon_astb * _b, cmon_idx _idx);

// adding expressions
CMON_API cmon_idx cmon_astb_add_ident(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_bool_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_float_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_int_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_string_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_binary(cmon_astb * _b,
                                       cmon_idx _op_tok_idx,
                                       cmon_idx _left,
                                       cmon_idx _right);
CMON_API cmon_idx cmon_astb_add_prefix(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _right);
CMON_API cmon_idx cmon_astb_add_call(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr_idx, cmon_idx * _arg_indices, size_t _count);
CMON_API cmon_idx cmon_astb_add_fn_decl(cmon_astb * _b,
                                        cmon_idx _tok_idx,
                                        cmon_idx _ret_type,
                                        cmon_idx * _params, //param and param lists
                                        size_t _count,
                                        cmon_idx _block_idx);

// adding statements
CMON_API cmon_idx cmon_astb_add_var_decl(cmon_astb * _b,
                                         cmon_idx _name_tok_idx,
                                         cmon_bool _is_pub,
                                         cmon_bool _is_mut,
                                         cmon_idx _type,
                                         cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_var_decl_list(cmon_astb * _b,
                                              cmon_idx * _name_toks,
                                              size_t _count,
                                              cmon_bool _is_pub,
                                              cmon_bool _is_mut,
                                              cmon_idx _type,
                                              cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_block(cmon_astb * _b,
                                      cmon_idx _tok_idx,
                                      cmon_idx * _stmt_indices,
                                      size_t _count);
CMON_API cmon_idx cmon_astb_add_module(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _name_tok_idx);

CMON_API cmon_idx cmon_astb_add_import_pair(cmon_astb * _b,
                                            cmon_idx * _path_toks,
                                            size_t _count,
                                            cmon_idx _alias_tok_idx);
CMON_API cmon_idx cmon_astb_add_import(cmon_astb * _b,
                                       cmon_idx _tok_idx,
                                       cmon_idx * _pairs,
                                       size_t _count);

// helpers
CMON_API cmon_idx cmon_astb_add_fn_param(cmon_astb * _b,
                                         cmon_idx _name_tok_idx,
                                         cmon_bool _is_mut,
                                         cmon_idx _type);
CMON_API cmon_idx cmon_astb_add_fn_param_list(
    cmon_astb * _b, cmon_idx * _name_toks, size_t _count, cmon_bool _is_mut, cmon_idx _type);
CMON_API cmon_idx cmon_astb_add_struct_field(cmon_astb * _b, cmon_idx * _name_toks);

CMON_API void cmon_astb_set_root_block(cmon_astb * _b, cmon_idx _idx);

// adding parsed types
CMON_API cmon_idx cmon_astb_add_type_named(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_type_ptr(cmon_astb * _b,
                                         cmon_idx _tok_idx,
                                         cmon_bool _is_mut,
                                         cmon_idx _type_idx);

// adding type declarations

// getting the ast without taking ownership
CMON_API cmon_ast * cmon_astb_get_ast(cmon_astb * _b);
// taking ownership of the ast
CMON_API cmon_ast * cmon_astb_copy_ast(cmon_astb * _b, cmon_allocator * _alloc);

// destroy an ast that has been copied via cmon_astb_copy_ast
CMON_API void cmon_ast_destroy(cmon_ast * _ast);

// ast getters
CMON_API cmon_ast_kind cmon_ast_node_kind(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_node_token(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_node_left(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_node_right(cmon_ast * _ast, cmon_idx _idx);

// CMON_API cmon_idx cmon_ast_data(cmon_ast * _ast, cmon_idx _idx);
// CMON_API uint64_t cmon_ast_int(cmon_ast * _ast, cmon_idx _idx);
// CMON_API double cmon_ast_float(cmon_ast * _ast, cmon_idx _idx);
// CMON_API cmon_string_view cmon_ast_str(cmon_ast * _ast, cmon_idx _idx);

#endif // CMON_CMON_AST_H
