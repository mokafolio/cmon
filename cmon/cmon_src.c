#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_src.h>
#include <cmon/cmon_util.h>

typedef struct
{
    char path[CMON_PATH_MAX];
    char filename[CMON_FILENAME_MAX];
    char * code;
    cmon_ast * ast;
    cmon_tokens * tokens;
    cmon_idx mod_src_idx; //src file index within the module
} cmon_src_file;

typedef struct cmon_src
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_src_file) files;
} cmon_src;

static inline cmon_src_file * _get_file(cmon_src * _src, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_src->files));
    return &_src->files[_idx];
}

cmon_src * cmon_src_create(cmon_allocator * _alloc)
{
    cmon_src * ret = CMON_CREATE(_alloc, cmon_src);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->files, _alloc, 32);
    return ret;
}

void cmon_src_destroy(cmon_src * _src)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_src->files); ++i)
    {
        cmon_c_str_free(_src->alloc, _src->files[i].code);
    }
    cmon_dyn_arr_dealloc(&_src->files);
    CMON_DESTROY(_src->alloc, _src);
}

cmon_idx cmon_src_add(cmon_src * _src, const char * _path, const char * _filename)
{
    cmon_src_file f;
    strcpy(f.path, _path);
    strcpy(f.filename, _filename);
    f.code = NULL;
    f.ast = NULL;
    f.tokens = NULL;
    f.mod_src_idx = CMON_INVALID_IDX;
    cmon_dyn_arr_append(&_src->files, f);
    return cmon_dyn_arr_count(&_src->files) - 1;
}

cmon_bool cmon_src_load_code(cmon_src * _src, cmon_idx _file_idx)
{
    
}

void cmon_src_set_code(cmon_src * _src, cmon_idx _file_idx, const char * _code)
{
    _get_file(_src, _file_idx)->code = cmon_c_str_copy(_src->alloc, _code);
}

void cmon_src_set_ast(cmon_src * _src, cmon_idx _file_idx, cmon_ast * _ast)
{
    _get_file(_src, _file_idx)->ast = _ast;
}

cmon_ast * cmon_src_ast(cmon_src * _src, cmon_idx _file_idx)
{
    return _get_file(_src, _file_idx)->ast;
}

void cmon_src_set_tokens(cmon_src * _src, cmon_idx _file_idx, cmon_tokens * _tokens)
{
    _get_file(_src, _file_idx)->tokens = _tokens;
}

void cmon_src_set_mod_src_idx(cmon_src * _src, cmon_idx _file_idx, cmon_idx _idx)
{
    _get_file(_src, _file_idx)->mod_src_idx = _idx;
}

cmon_tokens * cmon_src_tokens(cmon_src * _src, cmon_idx _file_idx)
{
    printf("da file count %lu %lu\n", _file_idx, cmon_dyn_arr_count(&_src->files));
    return _get_file(_src, _file_idx)->tokens;
}

const char * cmon_src_path(cmon_src * _src, cmon_idx _file_idx)
{
    return _get_file(_src, _file_idx)->path;
}

const char * cmon_src_filename(cmon_src * _src, cmon_idx _file_idx)
{
    return _get_file(_src, _file_idx)->filename;
}

const char * cmon_src_code(cmon_src * _src, cmon_idx _file_idx)
{
    return _get_file(_src, _file_idx)->code;
}

cmon_idx cmon_src_mod_src_idx(cmon_src * _src, cmon_idx _file_idx)
{
    return _get_file(_src, _file_idx)->mod_src_idx;
}

size_t cmon_src_count(cmon_src * _src)
{
    return cmon_dyn_arr_count(&_src->files);
}
