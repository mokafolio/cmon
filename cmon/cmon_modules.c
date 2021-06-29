#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>

typedef struct
{
    cmon_idx mod_idx;
    cmon_idx src_file_idx;
    cmon_idx import_name_tok_idx;
} _dep;

typedef struct
{
    size_t name_str_off, path_str_off, prefix_str_off;
    cmon_dyn_arr(cmon_idx) src_files;
    cmon_dyn_arr(_dep) deps; // module indices that this module depends on
    cmon_idx global_scope;
    cmon_resolver * resolver;
} _module;

typedef struct cmon_modules
{
    cmon_allocator * alloc;
    cmon_src * src;
    cmon_str_builder * str_builder;
    cmon_str_buf * str_buf;
    cmon_dyn_arr(_module) mods;
} cmon_modules;

cmon_modules * cmon_modules_create(cmon_allocator * _a, cmon_src * _src)
{
    cmon_modules * ret = CMON_CREATE(_a, cmon_modules);
    ret->alloc = _a;
    ret->src = _src;
    ret->str_builder = cmon_str_builder_create(_a, 256);
    ret->str_buf = cmon_str_buf_create(_a, 256);
    cmon_dyn_arr_init(&ret->mods, _a, 16);
    return ret;
}

void cmon_modules_destroy(cmon_modules * _m)
{
    size_t i;

    if (!_m)
        return;

    for (i = 0; i < cmon_dyn_arr_count(&_m->mods); ++i)
    {
        cmon_dyn_arr_dealloc(&_m->mods[i].deps);
        cmon_dyn_arr_dealloc(&_m->mods[i].src_files);
    }

    cmon_dyn_arr_dealloc(&_m->mods);
    cmon_str_buf_destroy(_m->str_buf);
    cmon_str_builder_destroy(_m->str_builder);
    CMON_DESTROY(_m->alloc, _m);
}

// cmon_src * cmon_modules_src(cmon_modules * _m)
// {
//     return _m->src;
// }

cmon_idx cmon_modules_add(cmon_modules * _m, const char * _path, const char * _name)
{
    cmon_idx ret;
    _module mod;

    ret = cmon_dyn_arr_count(&_m->mods);
    mod.path_str_off = cmon_str_buf_append(_m->str_buf, _path);
    mod.name_str_off = cmon_str_buf_append(_m->str_buf, _name);
    mod.global_scope = CMON_INVALID_IDX;
    mod.resolver = NULL;

    size_t count = 0;
    for(size_t i=0; i<cmon_dyn_arr_count(&_m->mods); ++i)
    {
        if(strcmp(_name, cmon_str_buf_get(_m->str_buf, _m->mods[i].name_str_off)) == 0)
        {
            ++count;
        }
    }

    cmon_str_builder_clear(_m->str_builder);
    if(count)
    {
        cmon_str_builder_append_fmt(_m->str_builder, "%s%lu", _name, count);
    }
    else
    {
        cmon_str_builder_append(_m->str_builder, _name);
    }
    mod.prefix_str_off = cmon_str_buf_append(_m->str_buf, cmon_str_builder_c_str(_m->str_builder));
    cmon_dyn_arr_init(&mod.src_files, _m->alloc, 8);
    cmon_dyn_arr_init(&mod.deps, _m->alloc, 4);
    cmon_dyn_arr_append(&_m->mods, mod);

    return ret;
}

static inline _module * _get_module(cmon_modules * _m, cmon_idx _mod_idx)
{
    assert(_mod_idx < cmon_dyn_arr_count(&_m->mods));
    return &_m->mods[_mod_idx];
}

void cmon_modules_add_dep(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _mod_dep_idx, cmon_idx _src_file_idx, cmon_idx _import_tok_idx)
{
    cmon_dyn_arr_append(&_get_module(_m, _mod_idx)->deps, ((_dep){_mod_dep_idx, _src_file_idx, _import_tok_idx}));
}

void cmon_modules_add_src_file(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _src_file)
{
    cmon_dyn_arr_append(&_get_module(_m, _mod_idx)->src_files, _src_file);
    cmon_src_set_mod_src_idx(
        _m->src, _src_file, cmon_dyn_arr_count(&_get_module(_m, _mod_idx)->src_files) - 1);
}

void cmon_modules_set_global_scope(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _scope)
{
    assert(!cmon_is_valid_idx(_get_module(_m, _mod_idx)->global_scope));
    _get_module(_m, _mod_idx)->global_scope = _scope;
}

void cmon_modules_set_resolver(cmon_modules * _m, cmon_idx _mod_idx, cmon_resolver * _r)
{
    _get_module(_m, _mod_idx)->resolver = _r;
}

cmon_resolver * cmon_modules_resolver(cmon_modules * _m, cmon_idx _mod_idx)
{
    return _get_module(_m, _mod_idx)->resolver;
}

cmon_idx cmon_modules_find(cmon_modules * _m, cmon_str_view _path)
{
    printf("cmon_modules_find\n\n\n");
    //@NOTE: for now we just linear search. maybe hashmap in the future
    cmon_idx i;
    for (i = 0; i < cmon_dyn_arr_count(&_m->mods); ++i)
    {
        printf("cmp bruh %.*s %s\n", _path.end - _path.begin, _path.begin, cmon_modules_path(_m, i));
        if (cmon_str_view_c_str_cmp(_path, cmon_modules_path(_m, i)) == 0)
        {
            return i;
        }
    }
    return CMON_INVALID_IDX;
}

size_t cmon_modules_count(cmon_modules * _m)
{
    return cmon_dyn_arr_count(&_m->mods);
}

const char * cmon_modules_path(cmon_modules * _m, cmon_idx _mod_idx)
{
    return cmon_str_buf_get(_m->str_buf, _get_module(_m, _mod_idx)->path_str_off);
}

const char * cmon_modules_name(cmon_modules * _m, cmon_idx _mod_idx)
{
    return cmon_str_buf_get(_m->str_buf, _get_module(_m, _mod_idx)->name_str_off);
}

const char * cmon_modules_prefix(cmon_modules * _m, cmon_idx _mod_idx)
{
    return cmon_str_buf_get(_m->str_buf, _get_module(_m, _mod_idx)->prefix_str_off);
}

cmon_idx cmon_modules_src_file(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _src_file_idx)
{
    assert(_src_file_idx < cmon_dyn_arr_count(&_get_module(_m, _mod_idx)->src_files));
    return _get_module(_m, _mod_idx)->src_files[_src_file_idx];
}

size_t cmon_modules_src_file_count(cmon_modules * _m, cmon_idx _mod_idx)
{
    return cmon_dyn_arr_count(&_get_module(_m, _mod_idx)->src_files);
}

cmon_idx cmon_modules_global_scope(cmon_modules * _m, cmon_idx _mod_idx)
{
    return _get_module(_m, _mod_idx)->global_scope;
}

cmon_idx cmon_modules_dep_mod_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_idx)
{
    return _get_module(_m, _mod_idx)->deps[_dep_idx].mod_idx;
}

cmon_idx cmon_modules_dep_tok_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_idx)
{
    return _get_module(_m, _mod_idx)->deps[_dep_idx].import_name_tok_idx;
}

cmon_idx cmon_modules_dep_src_file_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_idx)
{
    return _get_module(_m, _mod_idx)->deps[_dep_idx].src_file_idx;
}

size_t cmon_modules_dep_count(cmon_modules * _m, cmon_idx _mod_idx)
{
    return cmon_dyn_arr_count(&_get_module(_m, _mod_idx)->deps);
}

cmon_idx cmon_modules_find_dep_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_mod_idx)
{
    size_t i;
    
    for(i=0; i<cmon_dyn_arr_count(&_get_module(_m, _mod_idx)->deps); ++i)
    {
        printf("Wopp %lu %lu\n", _dep_mod_idx, cmon_modules_dep_mod_idx(_m, _mod_idx, i));
        if(cmon_modules_dep_mod_idx(_m, _mod_idx, i) == _dep_mod_idx)
        {
            return i;
        }
    }

    return CMON_INVALID_IDX;
}
