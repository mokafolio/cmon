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
    cmon_hashmap(cmon_str_view, cmon_idx) name_map;
} _scope;

typedef struct
{
    cmon_idx type_idx;
    cmon_bool is_mut;
} _var_data;

typedef struct
{
    cmon_str_view name;
    cmon_symk kind;
    cmon_bool is_pub;
    cmon_idx src_file_idx;
    cmon_idx ast_idx;
    cmon_idx scope_idx;
    union
    {
        cmon_idx idx;
        _var_data var;
    } data;
} _symbol;

typedef struct cmon_symbols
{
    cmon_allocator * alloc;
    cmon_modules * mods;
    cmon_dyn_arr(_symbol) symbols;
    cmon_dyn_arr(_scope) scopes;
} cmon_symbols;

static inline uint64_t _str_view_hash(cmon_str_view _view)
{
    return _cmon_str_range_hash(_view.begin, _view.end);
}

static inline cmon_bool _str_view_cmp(const void * _stra, const void * _strb, size_t _byte_count)
{
    return cmon_str_view_cmp(*(cmon_str_view *)_stra, *(cmon_str_view *)_strb) == 0;
}

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

static inline cmon_idx _get_sym_tok_idx(cmon_symbols * _s, cmon_idx _sym)
{
    cmon_src * src;
    _symbol * sym;
    src = cmon_modules_src(_s->mods);
    sym = _get_symbol(_s, _sym);
    return cmon_ast_token(cmon_src_ast(src, sym->src_file_idx), sym->ast_idx);
}

static inline cmon_idx _add_scope(cmon_symbols * _s, cmon_idx _parent_scope, cmon_str_view _name)
{
    _scope s;
    s.parent = _parent_scope;
    s.name = _name;
    cmon_dyn_arr_init(&s.symbols, _s->alloc, 16);
    cmon_dyn_arr_init(&s.children, _s->alloc, 4);
    cmon_hashmap_init(&s.name_map, _s->alloc, _str_view_hash, _str_view_cmp);
    cmon_dyn_arr_append(&_s->scopes, s);
    if (cmon_is_valid_idx(_parent_scope))
    {
        cmon_dyn_arr_append(&_get_scope(_s, _parent_scope)->children,
                            cmon_dyn_arr_count(&_s->scopes) - 1);
    }
    return cmon_dyn_arr_count(&_s->scopes) - 1;
}

static inline cmon_idx _add_symbol(cmon_symbols * _s,
                                   cmon_idx _scp,
                                   cmon_str_view _name,
                                   cmon_symk _kind,
                                   cmon_bool _is_pub,
                                   cmon_idx _src_file_idx,
                                   cmon_idx _ast_idx)
{
    _symbol s;
    _scope * scope;
    s.name = _name;
    s.kind = _kind;
    s.is_pub = _is_pub;
    s.src_file_idx = _src_file_idx;
    s.ast_idx = _ast_idx;
    s.scope_idx = _scp;

    scope = _get_scope(_s, _scp);
    cmon_dyn_arr_append(&scope->symbols, cmon_dyn_arr_count(&_s->symbols));
    cmon_dyn_arr_append(&_s->symbols, s);
    cmon_hashmap_set(&scope->name_map, _name, cmon_dyn_arr_count(&_s->symbols) - 1);
    return cmon_dyn_arr_count(&_s->symbols) - 1;
}

cmon_symbols * cmon_symbols_create(cmon_allocator * _alloc, cmon_modules * _mods)
{
    cmon_symbols * ret = CMON_CREATE(_alloc, cmon_symbols);
    ret->alloc = _alloc;
    ret->mods = _mods;
    cmon_dyn_arr_init(&ret->symbols, _alloc, 256);
    cmon_dyn_arr_init(&ret->scopes, _alloc, 256);
    return ret;
}

void cmon_symbols_destroy(cmon_symbols * _s)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_s->scopes); ++i)
    {
        cmon_hashmap_dealloc(&_s->scopes[i].name_map);
        cmon_dyn_arr_dealloc(&_s->scopes[i].children);
        cmon_dyn_arr_dealloc(&_s->scopes[i].symbols);
    }
    cmon_dyn_arr_dealloc(&_s->scopes);
    cmon_dyn_arr_dealloc(&_s->symbols);
    CMON_DESTROY(_s->alloc, _s);
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
    return _get_scope(_s, _scope)->parent == CMON_INVALID_IDX;
}

cmon_bool cmon_symbols_scope_is_file(cmon_symbols * _s, cmon_idx _scope)
{
    cmon_idx parent = _get_scope(_s, _scope)->parent;
    return cmon_is_valid_idx(parent) && cmon_symbols_scope_is_global(_s, parent);
}

cmon_idx cmon_symbols_scope_parent(cmon_symbols * _s, cmon_idx _scope)
{
    return _get_scope(_s, _scope)->parent;
}

cmon_idx cmon_symbols_scope_add_var(cmon_symbols * _s,
                                    cmon_idx _scope,
                                    cmon_str_view _name,
                                    cmon_idx _type_idx,
                                    cmon_bool _is_pub,
                                    cmon_bool _is_mut,
                                    cmon_idx _src_file_idx,
                                    cmon_idx _ast_idx)
{
    cmon_idx ret = _add_symbol(_s, _scope, _name, cmon_symk_var, _is_pub, _src_file_idx, _ast_idx);
    _get_symbol(_s, ret)->data.var.type_idx = _type_idx;
    _get_symbol(_s, ret)->data.var.is_mut = _is_mut;
    return ret;
}

void cmon_symbols_var_set_type(cmon_symbols * _s, cmon_idx _sym, cmon_idx _type)
{   
    assert(_get_symbol(_s, _sym)->kind == cmon_symk_var);
    _get_symbol(_s, _sym)->data.var.type_idx = _type;
}

cmon_idx cmon_symbols_scope_add_type(cmon_symbols * _s,
                                     cmon_idx _scope,
                                     cmon_str_view _name,
                                     cmon_idx _type_idx,
                                     cmon_bool _is_pub,
                                     cmon_idx _src_file_idx,
                                     cmon_idx _ast_idx)
{
    cmon_idx ret = _add_symbol(_s, _scope, _name, cmon_symk_type, _is_pub, _src_file_idx, _ast_idx);
    _get_symbol(_s, ret)->data.idx = _type_idx;
    return ret;
}

cmon_idx cmon_symbols_scope_add_import(cmon_symbols * _s,
                                       cmon_idx _scope,
                                       cmon_str_view _alias_name,
                                       cmon_idx _mod_idx,
                                       cmon_idx _src_file_idx,
                                       cmon_idx _ast_idx)
{
    cmon_idx ret =
        _add_symbol(_s, _scope, _alias_name, cmon_symk_import, cmon_false, _src_file_idx, _ast_idx);
    _get_symbol(_s, ret)->data.idx = _mod_idx;
    return ret;
}

cmon_idx cmon_symbols_find_local_before(cmon_symbols * _s,
                                        cmon_idx _scope_idx,
                                        cmon_str_view _name,
                                        cmon_idx _tok)
{
    cmon_idx * fidx;
    _scope * scope;

    scope = _get_scope(_s, _scope_idx);
    if ((fidx = cmon_hashmap_get(&scope->name_map, _name)))
    {
        if (!cmon_is_valid_idx(_tok) || _get_sym_tok_idx(_s, *fidx) < _tok)
            return *fidx;
    }
    return CMON_INVALID_IDX;
}

cmon_idx cmon_symbols_find_before(cmon_symbols * _s,
                                  cmon_idx _scope,
                                  cmon_str_view _name,
                                  cmon_idx _tok)
{
    cmon_idx scope, ret;

    scope = _scope;
    do
    {
        if (cmon_is_valid_idx(ret = cmon_symbols_find_local_before(_s, scope, _name, _tok)))
            return ret;
        scope = _get_scope(_s, scope)->parent;
    } while (cmon_is_valid_idx(scope));

    return CMON_INVALID_IDX;
}

cmon_idx cmon_symbols_find_local(cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name)
{
    return cmon_symbols_find_local_before(_s, _scope, _name, CMON_INVALID_IDX);
}

cmon_idx cmon_symbols_find(cmon_symbols * _s, cmon_idx _scope, cmon_str_view _name)
{
    return cmon_symbols_find_before(_s, _scope, _name, CMON_INVALID_IDX);
}

cmon_symk cmon_symbols_kind(cmon_symbols * _s, cmon_idx _sym)
{
    return _get_symbol(_s, _sym)->kind;
}

cmon_idx cmon_symbols_scope(cmon_symbols * _s, cmon_idx _sym)
{
    return _get_symbol(_s, _sym)->scope_idx;
}

cmon_str_view cmon_symbols_name(cmon_symbols * _s, cmon_idx _sym)
{
    return _get_symbol(_s, _sym)->name;
}

cmon_bool cmon_symbols_is_pub(cmon_symbols * _s, cmon_idx _sym)
{
    return _get_symbol(_s, _sym)->is_pub;
}

cmon_idx cmon_symbols_src_file(cmon_symbols * _s, cmon_idx _sym)
{
    return _get_symbol(_s, _sym)->src_file_idx;
}

cmon_idx cmon_symbols_ast(cmon_symbols * _s, cmon_idx _sym)
{
    return _get_symbol(_s, _sym)->ast_idx;
}

cmon_idx cmon_symbols_import_module(cmon_symbols * _s, cmon_idx _sym)
{
    assert(_get_symbol(_s, _sym)->kind == cmon_symk_import);
    return _get_symbol(_s, _sym)->data.idx;
}

cmon_idx cmon_symbols_type(cmon_symbols * _s, cmon_idx _sym)
{
    assert(_get_symbol(_s, _sym)->kind == cmon_symk_type);
    return _get_symbol(_s, _sym)->data.idx;
}

size_t cmon_symbols_scope_symbol_count(cmon_symbols * _s, cmon_idx _scope)
{
    return cmon_dyn_arr_count(&_get_scope(_s, _scope)->symbols);
}

cmon_idx cmon_symbols_scope_symbol(cmon_symbols * _s, cmon_idx _scope, cmon_idx _idx)
{
    assert(_idx < cmon_symbols_scope_symbol_count(_s, _scope));
    return _get_scope(_s, _scope)->symbols[_idx];
}

size_t cmon_symbols_scope_child_count(cmon_symbols * _s, cmon_idx _scope)
{
    return cmon_dyn_arr_count(&_get_scope(_s, _scope)->children);
}

cmon_idx cmon_symbols_scope_child(cmon_symbols * _s, cmon_idx _scope, cmon_idx _idx)
{
    assert(_idx < cmon_symbols_scope_child_count(_s, _scope));
    return _get_scope(_s, _scope)->children[_idx];
}
