#ifndef CMON_CMON_IR_H
#define CMON_CMON_IR_H

#include <cmon/cmon_allocator.h>

typedef enum
{
    cmon_irk_ident,
    cmon_irk_int_lit,
    cmon_irk_float_lit,
    cmon_irk_string_lit,
    cmon_irk_bool_lit,
    cmon_irk_call,
    cmon_irk_index,
    cmon_irk_selector,
    cmon_irk_struct_init,
    cmon_irk_array_init,
    cmon_irk_prefix,
    cmon_irk_binary,
    cmon_irk_addr,
    cmon_irk_deref,
    cmon_irk_noinit,
    cmon_irk_var_decl,
    cmon_irk_fn,
    cmon_irk_block,
    cmon_irk_paran_expr
} cmon_irk;

typedef struct cmon_ir cmon_ir;
// ir builder
typedef struct cmon_irb cmon_irb;

CMON_API cmon_irb * cmon_irb_create(cmon_allocator * _alloc,
                                    size_t _type_count,
                                    size_t _fn_count,
                                    size_t _global_var_count,
                                    size_t _node_count_estimate);
CMON_API void cmon_irb_destroy(cmon_irb * _b);

// Types (see cmon_types).
// Must be added in dependency order!
CMON_API void cmon_irb_add_type(cmon_irb * _b, cmon_idx _type_idx);

// expressions
// CMON_API cmon_idx cmon_irb_add_ident(cmon_irb * _b, const char * _name);
CMON_API cmon_idx cmon_irb_add_ident(cmon_irb * _b, cmon_idx _ref_idx);
CMON_API cmon_idx cmon_irb_add_bool_lit(cmon_irb * _b, cmon_bool _value);
CMON_API cmon_idx cmon_irb_add_float_lit(cmon_irb * _b, const char * _value);
CMON_API cmon_idx cmon_irb_add_int_lit(cmon_irb * _b, const char * _value);
CMON_API cmon_idx cmon_irb_add_string_lit(cmon_irb * _b, const char * _value);
CMON_API cmon_idx cmon_irb_add_addr(cmon_irb * _b, cmon_idx _expr);
CMON_API cmon_idx cmon_irb_add_deref(cmon_irb * _b, cmon_idx _expr);
CMON_API cmon_idx cmon_irb_add_binary(cmon_irb * _b, char _op, cmon_idx _left, cmon_idx _right);
CMON_API cmon_idx cmon_irb_add_prefix(cmon_irb * _b, char _op, cmon_idx _right);
CMON_API cmon_idx cmon_irb_add_paran(cmon_irb * _b, cmon_idx _expr);
CMON_API cmon_idx cmon_irb_add_call(cmon_irb * _b,
                                    cmon_idx _expr_idx,
                                    cmon_idx * _arg_indices,
                                    size_t _count);

//@NOTE: At this level, all the types fields expressions need to be passed in the order they are
// defined in the struct, including default expressions.
CMON_API cmon_idx cmon_irb_add_struct_init(cmon_irb * _b,
                                           cmon_idx _struct_type_idx,
                                           cmon_idx * _fields,
                                           size_t _count);
CMON_API cmon_idx cmon_irb_add_array_init(cmon_irb * _b,
                                          cmon_idx _array_type_idx,
                                          cmon_idx * _exprs,
                                          size_t _count);
CMON_API cmon_idx cmon_irb_add_selector(cmon_irb * _b, cmon_idx _left, const char * _name);
CMON_API cmon_idx cmon_irb_add_index(cmon_irb * _b, cmon_idx _lhs, cmon_idx _index_expr);

// statements
CMON_API cmon_idx cmon_irb_add_block(cmon_irb * _b, cmon_idx * _stmt_indices, size_t _count);
CMON_API cmon_idx cmon_irb_add_var_decl(
    cmon_irb * _b, const char * _name, cmon_bool _is_mut, cmon_idx _type_idx, cmon_idx _expr);

// functions
// @NOTE: If body block is CMON_INVALID_IDX, the function will be extern (i.e. defined in another
// module)
CMON_API cmon_idx cmon_irb_add_fn(cmon_irb * _b,
                                  const char * _name,
                                  cmon_idx _return_type,
                                  cmon_idx * _params,
                                  size_t _count,
                                  cmon_bool _is_main_fn);
CMON_API void cmon_irb_fn_set_body(cmon_irb * _b, cmon_idx _fn, cmon_idx _body);

// global variables
//@NOTE: If _expr is CMON_INVALID_IDX the variable will be extern, meaning it will be defined in a
// separate module (and thus compilation unit)
CMON_API cmon_idx cmon_irb_add_global_var_decl(cmon_irb * _b,
                                               const char * _name,
                                               cmon_bool _is_pub,
                                               cmon_bool _is_mut,
                                               cmon_idx _type_idx,
                                               cmon_idx _expr);

// getters
CMON_API cmon_ir * cmon_irb_ir(cmon_irb * _b);
CMON_API size_t cmon_ir_type_count(cmon_ir * _ir);
CMON_API cmon_idx cmon_ir_type(cmon_ir * _ir, size_t _i);
CMON_API size_t cmon_ir_fn_count(cmon_ir * _ir);
CMON_API cmon_idx cmon_ir_fn(cmon_ir * _ir, size_t _i);
CMON_API cmon_idx cmon_ir_main_fn(cmon_ir * _ir);
CMON_API size_t cmon_ir_global_var_count(cmon_ir * _ir);
CMON_API cmon_idx cmon_ir_global_var(cmon_ir * _ir, size_t _i);
CMON_API cmon_irk cmon_ir_kind(cmon_ir * _ir, cmon_idx _idx);
CMON_API const char * cmon_ir_ident_name(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_bool cmon_ir_bool_lit_value(cmon_ir * _ir, cmon_idx _idx);
CMON_API const char * cmon_ir_float_lit_value(cmon_ir * _ir, cmon_idx _idx);
CMON_API const char * cmon_ir_int_lit_value(cmon_ir * _ir, cmon_idx _idx);
CMON_API const char * cmon_ir_string_lit_value(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_addr_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_deref_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_addr_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_deref_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API char cmon_ir_binary_op(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_binary_left(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_binary_right(cmon_ir * _ir, cmon_idx _idx);
CMON_API char cmon_ir_prefix_op(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_prefix_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_paran_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_call_left(cmon_ir * _ir, cmon_idx _idx);
CMON_API size_t cmon_ir_call_arg_count(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_call_arg(cmon_ir * _ir, cmon_idx _idx, size_t _arg_idx);
CMON_API cmon_idx cmon_ir_struct_init_type(cmon_ir * _ir, cmon_idx _idx);
CMON_API size_t cmon_ir_struct_init_expr_count(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_struct_init_expr(cmon_ir * _ir, cmon_idx _idx, size_t _expr_idx);
CMON_API cmon_idx cmon_ir_array_init_type(cmon_ir * _ir, cmon_idx _idx);
CMON_API size_t cmon_ir_array_init_expr_count(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_array_init_expr(cmon_ir * _ir, cmon_idx _idx, size_t _expr_idx);
CMON_API cmon_idx cmon_ir_selector_left(cmon_ir * _ir, cmon_idx _idx);
CMON_API const char * cmon_ir_selector_name(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_index_left(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_index_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API size_t cmon_ir_block_child_count(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_block_child(cmon_ir * _ir, cmon_idx _idx, size_t _child_idx);
CMON_API const char * cmon_ir_var_decl_name(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_bool cmon_ir_var_decl_is_mut(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_var_decl_type(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_var_decl_expr(cmon_ir * _ir, cmon_idx _idx);
CMON_API const char * cmon_ir_fn_name(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_fn_return_type(cmon_ir * _ir, cmon_idx _idx);
CMON_API size_t cmon_ir_fn_param_count(cmon_ir * _ir, cmon_idx _idx);
CMON_API cmon_idx cmon_ir_fn_param(cmon_ir * _ir, cmon_idx _idx, size_t _param_idx);
CMON_API cmon_idx cmon_ir_fn_body(cmon_ir * _ir, cmon_idx _idx);

typedef struct cmon_str_builder cmon_str_builder;
typedef struct cmon_types cmon_types;
CMON_API const char * cmon_ir_debug_str(cmon_ir * _ir, cmon_types * _types, cmon_str_builder * _b);

#endif // CMON_CMON_IR_H
