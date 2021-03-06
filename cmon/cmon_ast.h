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
    cmon_astk_type_view,
    cmon_astk_type_array,
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
    cmon_astk_addr,
    cmon_astk_deref,
    cmon_astk_cast,
    cmon_astk_noinit,
    cmon_astk_fn_decl,
    // cmon_astk_range,
    // cmon_astk_expl_template_fn_init,
    cmon_astk_var_decl,
    cmon_astk_struct_field,
    cmon_astk_struct_decl,
    // cmon_astk_interface_decl,
    cmon_astk_exprstmt,
    cmon_astk_return,
    cmon_astk_break,
    cmon_astk_continue,
    cmon_astk_for,
    cmon_astk_for_in,
    cmon_astk_block,
    cmon_astk_import_pair, // the foo.bar as baz part of import foo.bar as baz
    cmon_astk_import,
    cmon_astk_module,
    cmon_astk_typedef,
    cmon_astk_alias,
    cmon_astk_if,
    cmon_astk_defer,
    cmon_astk_paran_expr // i.e. (1 + 2)
} cmon_astk;

typedef struct cmon_ast cmon_ast;
// ast builder
typedef struct cmon_astb cmon_astb;

CMON_API cmon_astb * cmon_astb_create(cmon_allocator * _alloc, cmon_tokens * _tokens);
CMON_API void cmon_astb_destroy(cmon_astb * _b);

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
CMON_API cmon_idx cmon_astb_add_paran(cmon_astb * _b,
                                      cmon_idx _open_tok_idx,
                                      cmon_idx _close_tok_idx,
                                      cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_call(cmon_astb * _b,
    cmon_idx _open_tok_idx,
                                     cmon_idx _close_tok_idx,
                                     cmon_idx _expr_idx,
                                     cmon_idx * _arg_indices,
                                     size_t _count);
CMON_API cmon_idx cmon_astb_add_fn_decl(cmon_astb * _b,
                                        cmon_idx _tok_idx,
                                        cmon_idx _ret_type,
                                        cmon_idx * _params,
                                        size_t _count,
                                        cmon_idx _block_idx);
CMON_API cmon_idx cmon_astb_add_struct_init_field(cmon_astb * _b,
                                                  cmon_idx _first_tok,
                                                  cmon_idx _name_tok,
                                                  cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_struct_init(cmon_astb * _b,
                                            cmon_idx _close_tok_idx,
                                            cmon_idx _parsed_type_idx,
                                            cmon_idx * _fields,
                                            size_t _count);
CMON_API cmon_idx cmon_astb_add_selector(cmon_astb * _b,
                                         cmon_idx _tok_idx,
                                         cmon_idx _left,
                                         cmon_idx _name_tok);
CMON_API cmon_idx cmon_astb_add_array_init(cmon_astb * _b,
                                           cmon_idx _open_tok_idx,
                                           cmon_idx _close_tok_idx,
                                           cmon_idx * _exprs,
                                           size_t _count);
CMON_API cmon_idx cmon_astb_add_index(cmon_astb * _b,
                                      cmon_idx _open_tok_idx,
                                      cmon_idx _close_tok_idx,
                                      cmon_idx _lhs,
                                      cmon_idx _index_expr);

// adding statements
CMON_API cmon_idx cmon_astb_add_var_decl(cmon_astb * _b,
                                         cmon_idx _name_tok_idx,
                                         cmon_bool _is_pub,
                                         cmon_bool _is_mut,
                                         cmon_idx _type,
                                         cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_block(cmon_astb * _b,
                                      cmon_idx _open_tok_idx,
                                      cmon_idx _close_tok_idx,
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

CMON_API cmon_idx cmon_astb_add_alias(cmon_astb * _b,
                                      cmon_idx _tok_idx,
                                      cmon_idx _name_tok,
                                      cmon_bool _is_pub,
                                      cmon_idx _parsed_type_idx);

CMON_API cmon_idx cmon_astb_add_typedef(cmon_astb * _b,
                                        cmon_idx _tok_idx,
                                        cmon_idx _name_tok,
                                        cmon_bool _is_pub,
                                        cmon_idx _parsed_type_idx);

// helpers
CMON_API void cmon_astb_set_root_block(cmon_astb * _b, cmon_idx _idx);

// adding parsed types
CMON_API cmon_idx cmon_astb_add_type_named(cmon_astb * _b,
                                           cmon_idx _mod_tok_idx,
                                           cmon_idx _name_tok_idx);
CMON_API cmon_idx cmon_astb_add_type_ptr(cmon_astb * _b,
                                         cmon_idx _tok_idx,
                                         cmon_bool _is_mut,
                                         cmon_idx _type_idx);
CMON_API cmon_idx cmon_astb_add_type_view(cmon_astb * _b,
                                          cmon_idx _tok_idx,
                                          cmon_bool _is_mut,
                                          cmon_idx _type_idx);
CMON_API cmon_idx cmon_astb_add_type_array(cmon_astb * _b,
                                           cmon_idx _tok_idx,
                                           size_t _count,
                                           cmon_idx _type_idx);
CMON_API cmon_idx cmon_astb_add_type_fn(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _last_tok, cmon_idx _ret_type, cmon_idx * _params, size_t _count);

// adding type declarations
CMON_API cmon_idx cmon_astb_add_struct_field(cmon_astb * _b,
                                             cmon_idx _name_tok,
                                             cmon_idx _type,
                                             cmon_idx _expr);
CMON_API cmon_idx cmon_astb_add_struct_decl(
    cmon_astb * _b, cmon_idx _tok_idx, cmon_idx _close_tok_idx, cmon_bool _is_pub, cmon_idx * _fields, size_t _count);

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
CMON_API cmon_idx cmon_ast_token_first(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_token_last(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_left(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_right(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_extra_data(cmon_ast * _ast, cmon_idx _extra_idx);

// module stmt specific getters
CMON_API cmon_idx cmon_ast_module_name_tok(cmon_ast * _ast, cmon_idx _mod_idx);

// import specific getters
CMON_API size_t cmon_ast_import_pairs_count(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_import_pair(cmon_ast * _ast, cmon_idx _idx, size_t _pidx);
CMON_API cmon_str_view cmon_ast_import_pair_path(cmon_ast * _ast, cmon_idx _importp_idx);
CMON_API cmon_idx cmon_ast_import_pair_path_first_tok(cmon_ast * _ast, cmon_idx _importp_idx);
CMON_API size_t cmon_ast_import_pair_path_token_count(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_import_pair_path_token(cmon_ast * _ast, cmon_idx _idx, size_t _tidx);
CMON_API cmon_idx cmon_ast_import_pair_alias(cmon_ast * _ast, cmon_idx _importp_idx);
CMON_API cmon_idx cmon_ast_import_pair_ident(cmon_ast * _ast, cmon_idx _importp_idx);

// identifier specific getters/setters
CMON_API cmon_str_view cmon_ast_ident_name(cmon_ast * _ast, cmon_idx _tidx);
CMON_API void cmon_ast_ident_set_sym(cmon_ast * _ast, cmon_idx _tidx, cmon_idx _sym);
CMON_API cmon_idx cmon_ast_ident_sym(cmon_ast * _ast, cmon_idx _tidx);

// parsed type specific getters (i.e. foo.Bar or *mut Foo)
CMON_API cmon_idx cmon_ast_type_named_module_tok(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_named_name_tok(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_ptr_type(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_bool cmon_ast_type_ptr_is_mut(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_view_type(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_bool cmon_ast_type_view_is_mut(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_array_type(cmon_ast * _ast, cmon_idx _tidx);
CMON_API size_t cmon_ast_type_array_count(cmon_ast * _ast, cmon_idx _tidx);
CMON_API cmon_idx cmon_ast_type_fn_return_type(cmon_ast * _ast, cmon_idx _tidx);
CMON_API size_t cmon_ast_type_fn_params_count(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_type_fn_param(cmon_ast * _ast, cmon_idx _idx, size_t _pidx);

// var decl specific getters/setters
CMON_API cmon_idx cmon_ast_var_decl_name_tok(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_bool cmon_ast_var_decl_is_pub(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_bool cmon_ast_var_decl_is_mut(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_type(cmon_ast * _ast, cmon_idx _vidx);
CMON_API cmon_idx cmon_ast_var_decl_expr(cmon_ast * _ast, cmon_idx _vidx);
CMON_API void cmon_ast_var_decl_set_sym(cmon_ast * _ast, cmon_idx _vidx, cmon_idx _sym);
CMON_API cmon_idx cmon_ast_var_decl_sym(cmon_ast * _ast, cmon_idx _vidx);

// block specific getters
CMON_API size_t cmon_ast_block_child_count(cmon_ast * _ast, cmon_idx _block_idx);
CMON_API cmon_idx cmon_ast_block_child(cmon_ast * _ast, cmon_idx _block_idx, size_t _idx);

// fn specific getters
CMON_API size_t cmon_ast_fn_params_count(cmon_ast * _ast, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_ast_fn_param(cmon_ast * _ast, cmon_idx _fn_idx, size_t _param_idx);
CMON_API cmon_idx cmon_ast_fn_ret_type(cmon_ast * _ast, cmon_idx _fn_idx);
CMON_API cmon_idx cmon_ast_fn_block(cmon_ast * _ast, cmon_idx _fn_idx);

// struct declaration specific getters
CMON_API cmon_idx cmon_ast_struct_field_name(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_field_type(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_field_expr(cmon_ast * _ast, cmon_idx _idx);
CMON_API size_t cmon_ast_struct_fields_count(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API cmon_idx cmon_ast_struct_field(cmon_ast * _ast, cmon_idx _struct_idx, size_t _field_idx);
CMON_API cmon_bool cmon_ast_struct_is_pub(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API cmon_idx cmon_ast_struct_name(cmon_ast * _ast, cmon_idx _struct_idx);
CMON_API void cmon_ast_struct_set_type(cmon_ast * _ast, cmon_idx _struct_idx, cmon_idx _type_idx);
CMON_API cmon_idx cmon_ast_struct_type(cmon_ast * _ast, cmon_idx _struct_idx);

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

// selector specific setters/getters
CMON_API cmon_idx cmon_ast_selector_left(cmon_ast * _ast, cmon_idx _sel_idx);
CMON_API cmon_idx cmon_ast_selector_name_tok(cmon_ast * _ast, cmon_idx _sel_idx);
CMON_API void cmon_ast_selector_set_sym(cmon_ast * _ast, cmon_idx _sel_idx, cmon_idx _sym);
CMON_API cmon_idx cmon_ast_selector_sym(cmon_ast * _ast, cmon_idx _sel_idx);

// call specific getters
CMON_API cmon_idx cmon_ast_call_left(cmon_ast * _ast, cmon_idx _idx);
CMON_API size_t cmon_ast_call_args_count(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_call_arg(cmon_ast * _ast, cmon_idx _idx, size_t _arg_idx);

// array init getters
CMON_API size_t cmon_ast_array_init_exprs_count(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_array_init_expr(cmon_ast * _ast, cmon_idx _idx, size_t _arg_idx);

// index specific getters
CMON_API cmon_idx cmon_ast_index_left(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_index_expr(cmon_ast * _ast, cmon_idx _idx);

// struct init specific setters/getters
CMON_API cmon_idx cmon_ast_struct_init_field_name_tok(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_init_field_expr(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_init_parsed_type(cmon_ast * _ast, cmon_idx _idx);
CMON_API size_t cmon_ast_struct_init_fields_count(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_struct_init_field(cmon_ast * _ast, cmon_idx _idx, size_t _fidx);
// used by resolver to set an index buffer that holds the sorted field expressions to initialize the
// struct.
CMON_API void cmon_ast_struct_init_set_resolved_field_idx_buf(cmon_ast * _ast,
                                                              cmon_idx _idx,
                                                              cmon_idx _idx_buf);
CMON_API cmon_idx cmon_ast_struct_init_resolved_field_idx_buf(cmon_ast * _ast, cmon_idx _idx);

// alias specific getters
CMON_API cmon_idx cmon_ast_alias_name_tok(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_bool cmon_ast_alias_is_pub(cmon_ast * _ast, cmon_idx _idx);
CMON_API cmon_idx cmon_ast_alias_parsed_type(cmon_ast * _ast, cmon_idx _idx);
CMON_API void cmon_ast_alias_set_sym(cmon_ast * _ast, cmon_idx _idx, cmon_idx _sym);
CMON_API cmon_idx cmon_ast_alias_sym(cmon_ast * _ast, cmon_idx _idx);

// CMON_API cmon_idx cmon_ast_data(cmon_ast * _ast, cmon_idx _idx);
// CMON_API uint64_t cmon_ast_int(cmon_ast * _ast, cmon_idx _idx);
// CMON_API double cmon_ast_float(cmon_ast * _ast, cmon_idx _idx);
// CMON_API cmon_string_view cmon_ast_str(cmon_ast * _ast, cmon_idx _idx);

#endif // CMON_CMON_AST_H
