#ifndef CMON_CMON_SYMBOLS_H
#define CMON_CMON_SYMBOLS_H

#include <cmon/cmon_modules.h>
#include <cmon/cmon_types.h>

typedef enum
{
    cmon_symk_var,
    cmon_symk_import,
    cmon_symk_type
} cmon_symk;

typedef struct cmon_symbols cmon_symbols;

CMON_API cmon_symbols * cmon_symbols_create(cmon_allocator * _alloc, cmon_modules * _mods);
CMON_API void cmon_symbols_destroy(cmon_symbols * _s);
CMON_API cmon_idx cmon_symbols_scope_begin(cmon_symbols * _s, cmon_idx _scope);
CMON_API cmon_idx cmon_symbols_scope_end(cmon_symbols * _s, cmon_idx _scope);

// check if a sym table represents the global scope
CMON_API cmon_bool cmon_symbols_scope_is_global(cmon_symbols * _s, cmon_idx _scope);
// checks if the table represents a file level scope
CMON_API cmon_bool cmon_symbols_scope_is_file(cmon_symbols * _s, cmon_idx _scope);
CMON_API cmon_idx cmon_symbols_scope_parent(cmon_symbols * _s, cmon_idx _scope);

CMON_API cmon_idx cmon_symbols_scope_add_var(cmon_symbols * _s,
                                             cmon_idx _scope,
                                             cmon_str_view _name,
                                             cmon_idx _type_idx,
                                             cmon_bool _is_pub,
                                             cmon_bool _is_mut,
                                             cmon_idx _src_file_idx,
                                             cmon_idx _ast_idx);
CMON_API void cmon_symbols_var_set_type(cmon_symbols * _s, cmon_idx _sym, cmon_idx _type);

CMON_API cmon_idx cmon_symbols_scope_add_type(cmon_symbols * _s,
                                              cmon_idx _scope,
                                              cmon_str_view _name,
                                              cmon_idx _type_idx,
                                              cmon_bool _is_pub,
                                              cmon_idx _src_file_idx,
                                              cmon_idx _ast_idx);

CMON_API cmon_idx cmon_symbols_scope_add_import(cmon_symbols * _s,
                                                cmon_idx _scope,
                                                cmon_str_view _alias_name,
                                                cmon_idx _mod_idx,
                                                cmon_idx _src_file_idx,
                                                cmon_idx _ast_idx);

// find symbols
CMON_API cmon_idx cmon_symbols_find_local_before(cmon_symbols * _s,
                                                 cmon_idx _scope,
                                                 cmon_str_view _name,
                                                 cmon_idx _tok);
CMON_API cmon_idx cmon_symbols_find_before(cmon_symbols * _s,
                                           cmon_idx _scope,
                                           cmon_str_view _name,
                                           cmon_idx _tok);

CMON_API cmon_idx cmon_symbols_find_local(cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name);
CMON_API cmon_idx cmon_symbols_find(cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name);

// get symbol info
CMON_API cmon_symk cmon_symbols_kind(cmon_symbols * _s, cmon_idx _sym);
CMON_API cmon_idx cmon_symbols_scope(cmon_symbols * _s, cmon_idx _sym);
CMON_API cmon_str_view cmon_symbols_name(cmon_symbols * _s, cmon_idx _sym);
CMON_API cmon_bool cmon_symbols_is_pub(cmon_symbols * _s, cmon_idx _sym);
CMON_API cmon_idx cmon_symbols_src_file(cmon_symbols * _s, cmon_idx _sym);
CMON_API cmon_idx cmon_symbols_ast(cmon_symbols * _s, cmon_idx _sym);

// get import specific symbol info
CMON_API cmon_idx cmon_symbols_import_module(cmon_symbols * _s, cmon_idx _sym);

// get type specific symbol info
CMON_API cmon_idx cmon_symbols_type(cmon_symbols * _s, cmon_idx _sym);

//get var decl specific symbol info
CMON_API cmon_idx cmon_symbols_var_type(cmon_symbols * _s, cmon_idx _sym);
CMON_API cmon_bool cmon_symbols_var_is_mut(cmon_symbols * _s, cmon_idx _sym);

//get scope info
CMON_API size_t cmon_symbols_scope_symbol_count(cmon_symbols * _s, cmon_idx _scope);
CMON_API cmon_idx cmon_symbols_scope_symbol(cmon_symbols * _s, cmon_idx _scope, cmon_idx _idx);
CMON_API size_t cmon_symbols_scope_child_count(cmon_symbols * _s, cmon_idx _scope);
CMON_API cmon_idx cmon_symbols_scope_child(cmon_symbols * _s, cmon_idx _scope, cmon_idx _idx);

#endif // CMON_CMON_SYMBOLS_H
