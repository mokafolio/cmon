#ifndef CMON_CMON_IR_H
#define CMON_CMON_IR_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_ir cmon_ir;
// ir builder
typedef struct cmon_irb cmon_irb;

CMON_API cmon_irb * cmon_irb_create(cmon_allocator * _alloc);
CMON_API void cmon_irb_destroy(cmon_irb * _b);

// expressions
CMON_API cmon_idx cmon_irb_add_ident(cmon_irb * _b, const char * _name);
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

CMON_API cmon_idx cmon_irb_add_struct_init_field(cmon_irb * _b,
                                                 cmon_idx _field_idx,
                                                 cmon_idx _expr);
CMON_API cmon_idx cmon_irb_add_struct_init(cmon_irb * _b,
                                           cmon_idx _struct_type_idx,
                                           cmon_idx * _fields,
                                           size_t _count);
CMON_API cmon_idx cmon_irb_add_array_init(cmon_irb * _b, cmon_idx * _exprs, size_t _count);
CMON_API cmon_idx cmon_irb_add_selector(cmon_irb * _b, cmon_idx _left, const char * _name);
CMON_API cmon_idx cmon_irb_add_index(cmon_irb * _b, cmon_idx _lhs, cmon_idx _index_expr);

// statements
CMON_API cmon_idx cmon_irb_add_block(cmon_irb * _b, cmon_idx * _stmt_indices, size_t _count);
CMON_API cmon_idx cmon_irb_add_var_decl(cmon_irb * _b,
                                        const char * _name,
                                        cmon_bool _is_pub,
                                        cmon_bool _is_mut,
                                        cmon_idx _type_idx,
                                        cmon_idx _expr);
CMON_API cmon_idx cmon_irb_add_alias(cmon_irb * _b,
                                     const char * _name,
                                     cmon_bool _is_pub,
                                     cmon_idx _type_idx);

// functions
// @NOTE: If body block is CMON_INVALID_IDX, the function will be extern (i.e. defined in another module)
CMON_API cmon_idx cmon_irb_add_fn(cmon_irb * _b, const char * _name, cmon_idx _return_type, cmon_idx _body_block);
CMON_API void cmon_irb_fn_add_param(cmon_irb * _b, cmon_idx _fn, const char * _name, cmon_idx _type_idx);

// global variables


#endif // CMON_CMON_IR_H
