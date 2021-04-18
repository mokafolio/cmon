#ifndef CMON_CMON_AST_H
#define CMON_CMON_AST_H

#include <cmon/cmon_tokens.h>

typedef enum
{
    cmon_astk_ident,
    cmon_astk_int_literal,
    cmon_astk_float_literal,
    cmon_astk_string_literal,
    cmon_astk_bool_literal,
    cmon_astk_type_named,
    cmon_astk_type_ptr,
    cmon_astk_type_fn,
    cmon_astk_type_tuple,
    cmon_astk_call,
    cmon_astk_index,
    cmon_astk_selector,
    cmon_astk_struct_init,
    cmon_astk_struct_init_field, // the bar: 1 portion of Foo{bar: 1}
    cmon_astk_array_init,
    cmon_astk_tuple_init,
    cmon_astk_view_init,
    cmon_astk_prefix,
    cmon_astk_binary,
    // cmon_astk_postfix,
    cmon_astk_addr,
    cmon_astk_deref,
    cmon_astk_cast,
    // cmon_astk_view,
    cmon_astk_noinit,
    cmon_astk_fn_param,      // i.e. mut bar : s32
    cmon_astk_fn_param_list, // i.e. foo, bar : s32
    cmon_astk_fn_decl,
    // cmon_astk_range,
    // cmon_astk_expl_template_fn_init,
    cmon_astk_var_decl,
    cmon_astk_var_decl_list, // i.e. foo, bar : s32 = 3
    cmon_astk_var_decl_data,
    cmon_astk_struct_field,
    cmon_astk_struct_field_list,
    cmon_astk_struct_decl,
    // cmon_astk_interface_decl,
    cmon_astk_exprstmt,
    cmon_astk_return,
    cmon_astk_break,
    cmon_astk_continue,
    cmon_astk_for,
    cmon_astk_for_in,
    // cmon_astk_c_for,
    cmon_astk_block,
    cmon_astk_import_pair, // the foo.bar as baz part of import foo.bar as baz
    cmon_astk_import,
    cmon_astk_module,
    cmon_astk_typedef,
    cmon_astk_typealias,
    cmon_astk_if,
    cmon_astk_defer,
    cmon_astk_paran_expr // i.e. (1 + 2)
} cmon_astk;

// typedef struct
// {
//     cmon_idx begin;
// } cmon_ast_iter;

// typedef struct cmon_ast_node
// {
//     cmon_astk kind;
//     size_t token_idx, left_idx, right_idx;
// } cmon_ast_node;

typedef struct cmon_ast cmon_ast;
// ast builder
typedef struct cmon_astb cmon_astb;

CMON_API cmon_astb * cmon_astb_create(cmon_allocator * _alloc, cmon_tokens * _tokens);
CMON_API void cmon_astb_destroy(cmon_astb * _b);

// buffering extra data (i.e. all the statement indices of a block)
// CMON_API cmon_idx cmon_astb_add_extra_data(cmon_astb * _b, cmon_idx _idx);

// adding expressions
CMON_API cmon_idx cmon_astb_add_ident(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_bool_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_float_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_int_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_string_lit(cmon_astb * _b, cmon_idx _tok_idx);
CMON_API cmon_idx cmon_astb_add_addr(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_deref(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_binary(cmon_astb * _b,
                                       cmon_idx _op_tok_idx,
                                       cmon_idx _left,
                                       cmon_idx _right);
CMON_API cmon_idx cmon_astb_add_prefix(cmon_astb * _b, cmon_idx _op_tok_idx, cmon_idx _right);
CMON_API cmon_idx cmon_astb_add_paran(cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_call(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _expr_idx, cmon_idx * _arg_indices, size_t _count);
CMON_API cmon_idx cmon_astb_add_fn_decl(cmon_astb * _b,
                                        cmon_idx _tok_idx,
                                        cmon_idx _ret_type,
                                        cmon_idx * _params, // param and param lists
                                        size_t _count,
                                        cmon_idx _block_idx);
CMON_API cmon_idx cmon_astb_add_struct_init_field(cmon_astb * _b,
                                                  cmon_idx _first_tok,
                                                  cmon_idx _name_tok,
                                                  cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_struct_init(cmon_astb * _b,
                                            cmon_idx _parsed_type_idx,
                                            cmon_idx * _fields,
                                            size_t _count);
CMON_API cmon_idx cmon_astb_add_selector(cmon_astb * _b,
                                         cmon_idx _tok_idx,
                                         cmon_idx _left,
                                         cmon_idx _name_tok);
CMON_API cmon_idx cmon_astb_add_array_init(cmon_astb * _b,
                                           cmon_idx _tok_idx,
                                           cmon_idx * _exprs,
                                           size_t _count);
CMON_API cmon_idx cmon_astb_add_index(cmon_astb * _b,
                                      cmon_idx _tok_idx,
                                      cmon_idx _lhs,
                                      cmon_idx _index_expr);

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
CMON_API void cmon_astb_set_root_block(cmon_astb * _b, cmon_idx _idx);

// adding parsed types
CMON_API cmon_idx cmon_astb_add_type_named(cmon_astb * _b,
                                           cmon_idx _mod_tok_idx,
                                           cmon_idx _name_tok_idx);
CMON_API cmon_idx cmon_astb_add_type_ptr(cmon_astb * _b,
                                         cmon_idx _tok_idx,
                                         cmon_bool _is_mut,
                                         cmon_idx _type_idx);
CMON_API cmon_idx cmon_astb_add_type_fn(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _ret_type, cmon_idx * _params, size_t _count);

// adding type declarations
CMON_API cmon_idx cmon_astb_add_struct_field(cmon_astb * _b,
                                             cmon_idx _name_tok,
                                             cmon_idx _type,
                                             cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_struct_field_list(
    cmon_astb * _b, cmon_idx * _name_toks, size_t _count, cmon_idx _type, cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_struct_decl(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_bool _is_pub, cmon_idx * _fields, size_t _count);

// getting the ast without taking ownership
CMON_API cmon_ast * cmon_astb_ast(cmon_astb * _b);
// taking ownership of the ast
CMON_API cmon_ast * cmon_astb_copy_ast(cmon_astb * _b, cmon_allocator * _alloc);

// destroy an ast that has been copied via cmon_astb_copy_ast
CMON_API void cmon_ast_destroy(cmon_ast * _ast);

// ast getters
CMON_API cmon_idx cmon_ast_root_block(cmon_ast * _ast);
CMON_API size_t cmon_ast_count(cmon_ast * _ast);
CMON_API cmon_astk cmon_ast_kind(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_token(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_left(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_right(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_extra_data(cmon_ast * _ast, cmon_idx _extra_idx);

// simple iterator interface to iterate over ranges of ast nodes
typedef struct cmon_ast_iter
{
    cmon_idx idx;
    cmon_idx end;
} cmon_ast_iter;

CMON_API cmon_idx cmon_ast_iter_next(cmon_ast * _ast, cmon_ast_iter * _it);

// module stmt specific getters
CMON_API cmon_idx cmon_ast_module_name_tok(cmon_ast * _ast, cmon_idx _mod_idx);

// import specific getters
CMON_API cmon_idx cmon_ast_import_begin(cmon_ast * _ast, cmon_idx _import_idx);
CMON_API cmon_idx cmon_ast_import_end(cmon_ast * _ast, cmon_idx _import_idx);
CMON_API cmon_ast_iter cmon_ast_import_iter(cmon_ast * _ast, cmon_idx _import_idx);
CMON_API cmon_str_view cmon_ast_import_pair_path(cmon_ast * _ast, cmon_idx _importp_idx);
CMON_API cmon_idx cmon_ast_import_pair_path_begin(cmon_ast * _ast, cmon_idx _importp_idx);
CMON_API cmon_idx cmon_ast_import_pair_path_end(cmon_ast * _ast, cmon_idx _importp_idx);
CMON_API cmon_ast_iter cmon_ast_import_pair_path_iter(cmon_ast * _ast, cmon_idx _import_idx);
CMON_API cmon_idx cmon_ast_import_pair_alias(cmon_ast * _ast, cmon_idx _importp_idx);

// identifier specific getters/setters
CMON_API cmon_str_view cmon_ast_ident_name(cmon_ast * _ast, cmon_idx _tidx);
CMON_API void cmon_ast_ident_set_sym(cmon_ast * _ast, cmon_idx _tidx, cmon_idx _sym);
CMON_API cmon_idx cmon_ast_ident_sym(cmon_ast * _ast, cmon_idx _tidx);

// parsed type specific getters (i.e. foo.Bar or *mut Foo)
CMON_API cmon_idx cmon_ast_type_named_module_tok(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_named_name_tok(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_ptr_type(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_bool cmon_ast_type_ptr_is_mut(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_fn_return_type(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_fn_params_begin(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_fn_params_end(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_ast_iter cmon_ast_type_fn_params_iter(cmon_ast * _ast, cmon_idx _tidx);

// var decl specific getters
CMON_API cmon_idx cmon_ast_var_decl_name_tok(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_bool cmon_ast_var_decl_is_pub(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_bool cmon_ast_var_decl_is_mut(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_type(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_expr(cmon_ast * _ast, cmon_idx _vidx);

// var decl list specific getters
CMON_API cmon_idx cmon_ast_var_decl_list_names_begin(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_list_names_end(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_ast_iter cmon_ast_var_decl_list_names_iter(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_bool cmon_ast_var_decl_list_is_pub(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_bool cmon_ast_var_decl_list_is_mut(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_list_type(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_list_expr(cmon_ast * _ast, cmon_idx _vidx);

// block specific getters
CMON_API cmon_idx cmon_ast_block_begin(cmon_ast * _ast, cmon_idx _block_idx);
CMON_API cmon_idx cmon_ast_block_end(cmon_ast * _ast, cmon_idx _block_idx);
CMON_API cmon_ast_iter cmon_ast_block_iter(cmon_ast * _ast, cmon_idx _block_idx);

// fn specific getters
CMON_API cmon_idx cmon_ast_fn_params_begin(cmon_ast * _ast, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_ast_fn_params_end(cmon_ast * _ast, cmon_idx _fn_idx);
CMON_API cmon_ast_iter cmon_ast_fn_params_iter(cmon_ast * _ast, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_ast_fn_ret_type(cmon_ast * _ast, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_ast_fn_block(cmon_ast * _ast, cmon_idx _fn_idx);

// struct declaration specific getters
CMON_API cmon_idx cmon_ast_struct_fields_begin(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API cmon_idx cmon_ast_struct_fields_end(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API cmon_ast_iter cmon_ast_struct_fields_iter(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API cmon_bool cmon_ast_struct_is_pub(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API cmon_bool cmon_ast_struct_name(cmon_ast * _ast, cmon_idx _struct_idx);

// addr/deref expr specific getters
CMON_API cmon_idx cmon_ast_addr_expr(cmon_ast * _ast, cmon_idx _addr_idx);
CMON_API cmon_idx cmon_ast_deref_expr(cmon_ast * _ast, cmon_idx _deref_idx);

// prefix expr specific getters
CMON_API cmon_idx cmon_ast_prefix_op_tok(cmon_ast * _ast, cmon_idx _pref_idx);
CMON_API cmon_idx cmon_ast_prefix_expr(cmon_ast * _ast, cmon_idx _pref_idx);

// binary expr specific getters
CMON_API cmon_idx cmon_ast_binary_op_tok(cmon_ast * _ast, cmon_idx _bin_idx);
CMON_API cmon_idx cmon_ast_binary_left(cmon_ast * _ast, cmon_idx _bin_idx);
CMON_API cmon_idx cmon_ast_binary_right(cmon_ast * _ast, cmon_idx _bin_idx);
CMON_API cmon_bool cmon_ast_binary_is_assignment(cmon_ast * _ast, cmon_idx _bin_idx);

// paran expression specific getters
CMON_API cmon_idx cmon_ast_paran_expr(cmon_ast * _ast, cmon_idx _paran_idx);

// selector specific getters
CMON_API cmon_idx cmon_ast_selector_left(cmon_ast * _ast, cmon_idx _sel_idx);
CMON_API cmon_idx cmon_ast_selector_name_tok(cmon_ast * _ast, cmon_idx _sel_idx);

// array init getters
CMON_API cmon_idx cmon_ast_array_init_exprs_begin(cmon_ast * _ast, cmon_idx _ai_idx);
CMON_API cmon_idx cmon_ast_array_init_exprs_end(cmon_ast * _ast, cmon_idx _ai_idx);
CMON_API cmon_ast_iter cmon_ast_array_init_exprs_iter(cmon_ast * _ast, cmon_idx _ai_idx);

// index specific getters
CMON_API cmon_idx cmon_ast_index_left(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_index_expr(cmon_ast * _ast, cmon_idx _idx);

// struct init specific getters
CMON_API cmon_idx cmon_ast_struct_init_parsed_type(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_init_fields_begin(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_init_fields_end(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_ast_iter cmon_ast_struct_init_fields_iter(cmon_ast * _ast, cmon_idx _idx);

// CMON_API cmon_idx cmon_ast_data(cmon_ast * _ast, cmon_idx _idx);
// CMON_API uint64_t cmon_ast_int(cmon_ast * _ast, cmon_idx _idx);
// CMON_API double cmon_ast_float(cmon_ast * _ast, cmon_idx _idx);
// CMON_API cmon_string_view cmon_ast_str(cmon_ast * _ast, cmon_idx _idx);

#endif // CMON_CMON_AST_H
