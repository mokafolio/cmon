#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_hashmap.h>
#include <cmon/cmon_symbols.h>
#include <cmon/cmon_util.h>

typedef struct
{
    cmon_idx parent;
    cmon_str_view name;
    cmon_dyn_arr(cmon_idx) symbols;
    cmon_dyn_arr(cmon_idx) children;
    cmon_hashmap(const char *, cmon_idx) name_map;
} _scope;

typedef struct
{
    cmon_str_view name;
    cmon_symbol_kind kind;
    cmon_bool is_pub;
    cmon_idx ast_idx;
    union
    {
        cmon_idx idx;
    } data;
} _symbol;

typedef struct cmon_symbols
{
    cmon_allocator * alloc;
    cmon_dyn_arr(_symbol) symbols;
    cmon_dyn_arr(_scope) scopes;

    // //scopes
    // cmon_dyn_arr(cmon_idx) parents;
    // cmon_dyn_arr(cmon_idx) children_begins;
    // cmon_dyn_arr(cmon_idx) children_ends;
    // cmon_dyn_arr(cmon_idx) children_list;

    // //decls
    // cmon_dyn_arr(cmon_symbol_kind) kinds;
    // cmon_dyn_arr(cmon_bool) is_pub;

} cmon_symbols;

static inline _scope * _get_scope(cmon_symbols * _s, cmon_idx _scope)
{
    assert(_scope < cmon_dyn_arr_count(&_s->scopes));
    return &_s->scopes[_scope];
}

static inline _symbol * _get_symbol(cmon_symbols * _s, cmon_idx _sym)
{
    assert(_sym < cmon_dyn_arr_count(&_s->symbols));
    return &_s->symbols[_sym];
}

static inline cmon_idx _add_scope(cmon_symbols * _s, cmon_idx _parent_scope, cmon_str_view _name)
{
    _scope s;
    s.parent = _parent_scope;
    s.name = _name;
    cmon_dyn_arr_init(&s.symbols, _s->alloc, 16);
    cmon_dyn_arr_init(&s.children, _s->alloc, 4);
    cmon_hashmap_str_key_init(&s.name_map, _s->alloc);
    cmon_dyn_arr_append(&_s->scopes, s);
    if (cmon_is_valid_idx(_parent_scope))
    {
        cmon_dyn_arr_append(&_get_scope(_s, _parent_scope)->children,
                            cmon_dyn_arr_count(&_s->scopes) - 1);
    }
    return cmon_dyn_arr_count(&_s->scopes) - 1;
}

static inline cmon_idx _add_symbol(cmon_symbols * _s,
                                   cmon_idx _scope,
                                   cmon_str_view _name,
                                   cmon_symbol_kind _kind,
                                   cmon_bool _is_pub,
                                   cmon_idx _ast_idx)
{
    _symbol s;
    s.name = _name;
    s.kind = _kind;
    s.is_pub = _is_pub;
    s.ast_idx = _ast_idx;
    cmon_dyn_arr_append(&_get_scope(_s, _scope)->symbols, cmon_dyn_arr_count(&_s->symbols));
    cmon_dyn_arr_append(&_s->symbols, s);
    return cmon_dyn_arr_count(&_s->symbols) - 1;
}

cmon_symbols * cmon_symbols_create(cmon_allocator * _alloc)
{
    cmon_symbols * ret = CMON_CREATE(_alloc, cmon_symbols);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->symbols, _alloc, 256);
    cmon_dyn_arr_init(&ret->scopes, _alloc, 256);
    return ret;
}

void cmon_symbols_destroy(cmon_symbols * _s)
{
}

cmon_idx cmon_symbols_scope_begin(cmon_symbols * _s, cmon_idx _scope)
{
    return _add_scope(_s, _scope, cmon_str_view_make(""));
}

cmon_idx cmon_symbols_scope_end(cmon_symbols * _s, cmon_idx _scope)
{
    return _get_scope(_s, _scope)->parent;
}

cmon_bool cmon_symbols_scope_is_global(cmon_symbols * _s, cmon_idx _scope)
{
}

cmon_bool cmon_symbols_scope_is_file(cmon_symbols * _s, cmon_idx _scope)
{
}

cmon_idx cmon_symbols_scope_add_var(cmon_symbols * _s,
                                    cmon_idx _scope,
                                    cmon_str_view _name,
                                    cmon_idx _type_idx,
                                    cmon_idx _ast_idx,
                                    cmon_bool _is_pub)
{
    cmon_idx ret = _add_symbol(_s, _scope, _name, cmon_sk_var, _is_pub, _ast_idx);
    _get_symbol(_s, ret)->data.idx = _type_idx;
    return ret;
}

cmon_idx cmon_symbols_scope_add_type(cmon_symbols * _s,
                                     cmon_idx _scope,
                                     cmon_idx _type_idx,
                                     cmon_idx _ast_idx)
{
}

cmon_idx cmon_symbols_scope_add_import(cmon_symbols * _s,
                                       cmon_str_view _alias_name,
                                       cmon_idx _mod_idx,
                                       cmon_idx _ast_idx)
{
}

cmon_idx cmon_symbols_find_local_before(cmon_symbols * _s,
                                        cmon_idx _scope,
                                        cmon_str_view _name,
                                        cmon_idx _tok)
{
}

cmon_idx cmon_symbols_find_before(cmon_symbols * _s,
                                  cmon_idx _scope,
                                  cmon_str_view _name,
                                  cmon_idx _tok)
{
}

cmon_idx cmon_symbols_find_local(cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name)
{
}

cmon_idx cmon_symbols_find(cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name)
{
}
