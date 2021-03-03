#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_str_builder.h>

typedef struct
{
    size_t name_str_off, path_str_off, prefix_str_off;
    cmon_dyn_arr(cmon_idx) src_files;
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

    for (i = 0; cmon_dyn_arr_count(&_m->mods); ++i)
    {
        cmon_dyn_arr_dealloc(&_m->mods[i].src_files);
    }

    cmon_dyn_arr_dealloc(&_m->mods);
    cmon_str_buf_destroy(_m->str_buf);
    cmon_str_builder_destroy(_m->str_builder);
}

cmon_idx cmon_modules_add(cmon_modules * _m, const char * _path, const char * _name)
{
    cmon_idx ret;
    _module mod;

    ret = cmon_dyn_arr_count(&_m->mods);
    mod.path_str_off = cmon_str_buf_append(_m->str_buf, _path);
    mod.name_str_off = cmon_str_buf_append(_m->str_buf, _name);

    cmon_str_builder_clear(_m->str_builder);
    cmon_str_builder_append_fmt(_m->str_builder, "%s%lu", _name, ret);
    mod.prefix_str_off = cmon_str_buf_append(_m->str_buf, cmon_str_builder_c_str(_m->str_builder));
    cmon_dyn_arr_init(&mod.src_files, _m->alloc, 8);
    cmon_dyn_arr_append(&_m->mods, mod);

    return ret;
}

static inline _module * _get_module(cmon_modules * _m, cmon_idx _mod_idx)
{
    assert(_mod_idx < cmon_dyn_arr_count(&_m->mods));
    return &_m->mods[_mod_idx];
}

void cmon_modules_add_src_file(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _src_file)
{
    cmon_dyn_arr_append(&_get_module(_m, _mod_idx)->src_files, _src_file);
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
