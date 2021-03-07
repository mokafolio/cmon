#ifndef CMON_CMON_SYM_TBL_H
#define CMON_CMON_SYM_TBL_H

#include <cmon/cmon_types.h>

typedef struct cmon_symbols cmon_symbols;

CMON_API cmon_symbols * cmon_symbols_create(cmon_allocator * _alloc);
CMON_API void cmon_symbols_destroy(cmon_symbols * _s);
CMON_API cmon_idx cmon_symbols_scope_begin(cmon_symbols * _s, cmon_idx _scope);
CMON_API cmon_idx cmon_symbols_scope_end(cmon_symbols * _s, cmon_idx _scope);

// check if a sym table represents the global scope
CMON_API cmon_bool cmon_symbols_scope_is_global(cmon_symbols * _s, cmon_idx _scope);
// checks if the table represents a file level scope
CMON_API cmon_bool cmon_symbols_scope_is_file(cmon_symbols * _s, cmon_idx _scope);

CMON_API cmon_idx cmon_symbols_scope_add_var(
    cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name, cmon_idx _type_idx, cmon_idx _ast_idx);

CMON_API cmon_idx cmon_symbols_scope_add_type(cmon_symbols * _s,
                                              cmon_idx _scope,
                                              cmon_idx _type_idx,
                                              cmon_idx _ast_idx);

CMON_API cmon_idx cmon_symbols_scope_add_import(cmon_symbols * _s,
                                                cmon_str_view _alias_name,
                                                cmon_idx _mod_idx,
                                                cmon_idx _ast_idx);

// find symbols
CMON_API cmon_idx cmon_symbols_find_local_before(cmon_symbols * _s,
                                                 cmon_idx _scope,
                                                 cmon_string_view _name,
                                                 cmon_token * _tok);
CMON_API cmon_idx cmon_symbols_find_before(cmon_symbols * _s,
                                           cmon_idx _scope,
                                           cmon_string_view _name,
                                           cmon_token * _tok);

CMON_API cmon_idx cmon_symbols_find_local(cmon_symbols * _s,
                                          cmon_idx _scope,
                                          cmon_string_view _name);
CMON_API cmon_idx cmon_symbols_find(cmon_symbols * _s, cmon_idx _scope, cmon_string_view _name);

// #include <cmon/cmon_hashmap.h>
// // #include <cmon/cmon_struct.h>
// // #include <cmon/cmon_thread.h>
// // #include <cmon/cmon_types.h>

// typedef struct cmon_sym cmon_sym;
// typedef struct cmon_sym_tbl cmon_sym_tbl;
// typedef struct cmon_module cmon_module;

// _cmon_map_type_decl(cmon_string_view_idx_map, cmon_string_view, size_t);

// typedef struct
// {
//     cmon_type_info * type;
//     cmon_bool is_mut;
//     cmon_bool is_global;
//     cmon_ast_stmt * ast_stmt; // the ast stmt that declared this var
//     // void * data; //cmon_fn * for a function, cmon_ast_expr * for any expression
// } cmon_sym_var_decl;

// typedef struct
// {
//     // is_mut stored in name_type
//     // cmon_name_type_pair name_type;
//     cmon_dyn_arr(cmon_token *) path_toks;
//     cmon_token * alias_tok;
//     cmon_module * module;
// } cmon_sym_import_decl;

// typedef enum
// {
//     cmon_sym_kind_var,
//     cmon_sym_kind_generic_param,
//     cmon_sym_kind_fn_generic,
//     cmon_sym_kind_type,
//     // cmon_sym_kind_builtin,
//     cmon_sym_kind_import,
//     cmon_sym_kind_placeholder
// } cmon_sym_kind;

// typedef union
// {
//     // NULL until after resolve phase
//     cmon_type_info * ti;
//     cmon_sym_var_decl var;
//     cmon_sym_import_decl import;
//     cmon_ast_stmt * stmt;
// } cmon_sym_data;

// typedef struct cmon_sym
// {
//     // for placeholders, name_tok is NULL as the definition has not been seen yet, but we still
//     need
//     // the string_view name to find it. Builtin types/names don't have a token attached either.
//     cmon_string_view name;
//     cmon_token * name_tok;
//     cmon_sym_kind kind;
//     cmon_sym_data data;
//     cmon_bool is_pub;
//     struct cmon_sym_tbl * table;
//     size_t idx;
//     //filled in during ir generation
//     size_t ir_idx;
// } cmon_sym;

// typedef struct cmon_sym_tbl
// {
//     cmon_allocator * alloc;
//     struct cmon_sym_tbl * parent;
//     cmon_dyn_arr(struct cmon_sym_tbl *) children;
//     cmon_dyn_arr(cmon_sym *) symbols;
//     cmon_string_view_idx_map idx_map;
//     cmon_mutex mtx;
// } cmon_sym_tbl;

// // create/destroy a sym table
// CMON_API cmon_sym_tbl * cmon_sym_tbl_create(cmon_allocator * _alloc, cmon_sym_tbl * _parent);
// CMON_API void cmon_sym_tbl_destroy(cmon_sym_tbl * _tbl);

// // start a new child scope
// CMON_API cmon_sym_tbl * cmon_sym_tbl_begin_scope(cmon_sym_tbl * _tbl);

// // end a scope, returns the parent (or null if the ended scope is global, which really should
// never
// // happen)
// CMON_API cmon_sym_tbl * cmon_sym_tbl_end_scope(cmon_sym_tbl * _tbl);

// // check if a sym table represents the global scope
// CMON_API cmon_bool cmon_sym_tbl_is_global(cmon_sym_tbl * _tbl);

// // checks if the table represents a file level scope
// CMON_API cmon_bool cmon_sym_tbl_is_file(cmon_sym_tbl * _tbl);

// //@NOTE: all the functions to add symbols don't check for duplicate names, that is handled during
// // parsing/resolving!
// // declare a variable
// CMON_API cmon_sym * cmon_sym_tbl_decl_var(cmon_sym_tbl * _s,
//                                           cmon_token * _name_tok,
//                                           cmon_bool _is_mut,
//                                           cmon_bool _is_pub,
//                                           cmon_type_info * _ti);

// CMON_API cmon_sym * cmon_sym_tbl_decl_fn_generic(cmon_sym_tbl * _s,
//                                                  cmon_token * _name_tok,
//                                                  cmon_ast_stmt * _fn);

// CMON_API cmon_sym * cmon_sym_tbl_decl_generic_param(cmon_sym_tbl * _s,
//                                                     cmon_token * _name_tok,
//                                                     cmon_type_info * _ti);

// CMON_API cmon_sym * cmon_sym_tbl_decl_self(cmon_sym_tbl * _s, cmon_type_info * _self_ti);

// // declare a function
// //@NOTE: the cmon_fn objects are owned by the sym table
// // CMON_API cmon_sym * cmon_sym_tbl_decl_fn(cmon_sym_tbl * _s, cmon_fn * _fn);

// // declare an import
// CMON_API cmon_sym * cmon_sym_tbl_decl_import(cmon_sym_tbl * _s,
//                                              cmon_dyn_arr(cmon_token *) _path_toks,
//                                              cmon_token * _alias_tok,
//                                              cmon_module * _imported_module);

// // functions to declare named types
// // CMON_API cmon_sym * cmon_sym_tbl_decl_struct(cmon_sym_tbl * _s, cmon_struct * _struct);

// // declare a type symbol
// //@NOTE: The cmon_type_info's are not owned by the sym table but by the type registry
// CMON_API cmon_sym * cmon_sym_tbl_decl_type(cmon_sym_tbl * _s,
//                                            cmon_type_info * _ti,
//                                            cmon_bool _is_public);

// // find symbol or create placeholder
// CMON_API cmon_sym * cmon_sym_tbl_find_or_placeholder(cmon_sym_tbl * _s, cmon_string_view _name);

// // find symbols
// CMON_API cmon_sym * cmon_sym_tbl_find_local_before_tok(cmon_sym_tbl * _s,
//                                                        cmon_string_view _name,
//                                                        cmon_token * _tok);
// CMON_API cmon_sym * cmon_sym_tbl_find_before_tok(cmon_sym_tbl * _s,
//                                                  cmon_string_view _name,
//                                                  cmon_token * _tok);
// CMON_API cmon_sym * cmon_sym_tbl_find_local(cmon_sym_tbl * _s, cmon_string_view _name);
// CMON_API cmon_sym * cmon_sym_tbl_find(cmon_sym_tbl * _s, cmon_string_view _name);

#endif // CMON_CMON_SYM_TBL_H
