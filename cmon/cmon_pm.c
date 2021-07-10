#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_exec.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_pm.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tini.h>
#include <cmon/cmon_util.h>

typedef struct
{
    cmon_idx idx;
    char url[CMON_PATH_MAX];
    char version[CMON_FILENAME_MAX];
    char dirname[CMON_FILENAME_MAX];
    cmon_dyn_arr(cmon_idx) deps; // first hand dependencies of _mod
} _mod;

typedef struct
{
    char url[CMON_PATH_MAX];
    char version[CMON_FILENAME_MAX];
    char dirname[CMON_FILENAME_MAX];
} _locked_dep;

typedef struct cmon_pm_lock_file
{
    cmon_allocator * alloc;
    cmon_dyn_arr(_locked_dep) deps;
    char err_msg[CMON_ERR_MSG_MAX + CMON_PATH_MAX * 2];
} cmon_pm_lock_file;

typedef struct cmon_pm
{
    cmon_allocator * alloc;
    cmon_str_builder * str_builder;
    cmon_str_builder * exec_output_builder;
    char deps_dir[CMON_PATH_MAX];
    cmon_dyn_arr(_mod) mods;
    char err_msg[CMON_ERR_MSG_MAX + CMON_PATH_MAX * 2]; // extra long to accomodate long urls and
                                                        // avoid snprintf possible overflow warning
} cmon_pm;

#define _buffer_err(_buf, _buf_size, _goto, _fmt, ...)                                             \
    do                                                                                             \
    {                                                                                              \
        snprintf((_buf), (_buf_size), _fmt, ##__VA_ARGS__);                                        \
        goto _goto;                                                                                \
    } while (0)
#define _err(_pm, _goto, _fmt, ...)                                                                \
    _buffer_err((_pm)->err_msg, sizeof((_pm)->err_msg), _goto, _fmt, ##__VA_ARGS__)

// this macro expects an err flag to be in scope (which will usually get returned at the end of the
// function)
#define _err2(_pm, _goto, _msg, ...)                                                               \
    do                                                                                             \
    {                                                                                              \
        err = cmon_true;                                                                           \
        _err(_pm, _goto, _msg, ##__VA_ARGS__);                                                     \
    } while (0)
#define _err3(_buf, _buf_size, _goto, _msg, ...)                                                   \
    do                                                                                             \
    {                                                                                              \
        err = cmon_true;                                                                           \
        _buffer_err(_buf, _buf_size, _goto, _msg, ##__VA_ARGS__);                                  \
    } while (0)

cmon_pm * cmon_pm_create(cmon_allocator * _a, const char * _dep_dir)
{
    cmon_pm * ret = CMON_CREATE(_a, cmon_pm);
    ret->alloc = _a;
    ret->str_builder = cmon_str_builder_create(_a, CMON_PATH_MAX);
    ret->exec_output_builder = cmon_str_builder_create(_a, 1024);
    strcpy(ret->deps_dir, _dep_dir);
    cmon_dyn_arr_init(&ret->mods, _a, 4);
    return ret;
}

void cmon_pm_destroy(cmon_pm * _pm)
{
    if (!_pm)
    {
        return;
    }
    cmon_str_builder_destroy(_pm->exec_output_builder);
    cmon_str_builder_destroy(_pm->str_builder);
    cmon_dyn_arr_dealloc(&_pm->mods);
}

static inline cmon_idx _find_module(cmon_pm * _pm, cmon_str_view _url, cmon_str_view _version)
{
    for (cmon_idx i = 0; i < cmon_dyn_arr_count(&_pm->mods); ++i)
    {
        //@TODO: For now we only check for version equality. In the future, we might want to add a
        // more sophisticated check. I.e. in the simple case where one version is latest and the
        // other one is explicit, we might just want to pull the latest one. For now we will just
        // have multiple.
        if (cmon_str_view_c_str_cmp(_url, _pm->mods[i].url) == 0 &&
            cmon_str_view_c_str_cmp(_version, _pm->mods[i].version) == 0)
        {
            return i;
        }
    }
    return CMON_INVALID_IDX;
}

static inline void _cpy_str_view(cmon_str_view _sv, char * _buf, size_t _buf_size)
{
    assert(_sv.end - _sv.begin < _buf_size);
    strncpy(_buf, _sv.begin, _sv.end - _sv.begin);
}

static inline _mod * _get_mod(cmon_pm * _pm, cmon_idx _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_pm->mods));
    return &_pm->mods[_idx];
}

static inline void _dep_dirname(char * _buf, size_t _buf_size, cmon_idx _idx)
{
    snprintf(_buf, _buf_size, "dep%02lu", _idx);
}

cmon_idx cmon_pm_find_or_add_module(cmon_pm * _pm, cmon_str_view _url, cmon_str_view _version)
{
    cmon_idx ret = _find_module(_pm, _url, _version);
    if (cmon_is_valid_idx(ret))
        return ret;

    _mod m;
    m.idx = cmon_dyn_arr_count(&_pm->mods);
    _cpy_str_view(_url, m.url, sizeof(m.url));
    _cpy_str_view(_version, m.version, sizeof(m.version));
    // strcpy(m.dirname, cmon_str_builder_tmp_str(_pm->str_builder, "dep%02lu", m.idx + 1));
    _dep_dirname(m.dirname, sizeof(m.dirname), m.idx + 1);
    cmon_dyn_arr_init(&m.deps, _pm->alloc, 4);
    cmon_dyn_arr_append(&_pm->mods, m);
    return m.idx;
}

cmon_idx cmon_pm_find_or_add_module_c_str(cmon_pm * _pm, const char * _url, const char * _version)
{
    return cmon_pm_find_or_add_module(_pm, cmon_str_view_make(_url), cmon_str_view_make(_version));
}

cmon_bool cmon_pm_add_dep(cmon_pm * _pm, cmon_idx _mod, cmon_idx _dep)
{
    cmon_dyn_arr_append(&_get_mod(_pm, _mod)->deps, _dep);
    return cmon_false;
}

typedef void (*_tini_dep_handler)(cmon_str_view _url, cmon_str_view _version, void * _user_data);
static inline cmon_bool _parse_deps_tini(cmon_allocator * _alloc,
                                         const char * _path,
                                         char * _err_buf,
                                         size_t _buf_size,
                                         _tini_dep_handler _handler,
                                         void * _user_data)
{
    cmon_tini_err terr;
    cmon_tini * tini;
    cmon_bool err = cmon_false;

    if ((tini = cmon_tini_parse_file(_alloc, _path, &terr)))
    {
        _err3(_err_buf,
              _buf_size,
              end,
              "failed to load/parse dependency file:%s:%lu:%lu: %s",
              terr.filename,
              terr.line,
              terr.line_offset,
              terr.msg);
    }

    cmon_idx root_obj = cmon_tini_root_obj(tini);
    cmon_idx deps_arr = cmon_tini_obj_find(tini, root_obj, "deps");

    for (size_t i = 0; i < cmon_tini_child_count(tini, deps_arr); ++i)
    {
        cmon_idx child = cmon_tini_child(tini, deps_arr, i);
        if (cmon_tini_kind(tini, child) == cmon_tinik_obj)
        {
            cmon_idx url = cmon_tini_obj_find(tini, root_obj, "url");
            if (!cmon_is_valid_idx(url))
            {
                _err3(_err_buf, _buf_size, end, "missing url in dependency obj");
            }
            cmon_idx version = cmon_tini_obj_find(tini, root_obj, "version");
            if (!cmon_is_valid_idx(version))
            {
                _err3(_err_buf, _buf_size, end, "missing version in dependency obj");
            }

            cmon_idx url_val = cmon_tini_pair_value(tini, url);
            if (cmon_tini_kind(tini, url_val) != cmon_tinik_string)
            {
                _err3(_err_buf, _buf_size, end, "url field has to be a string");
            }

            cmon_idx version_val = cmon_tini_pair_value(tini, version);
            if (cmon_tini_kind(tini, version_val) != cmon_tinik_string)
            {
                _err3(_err_buf, _buf_size, end, "version field has to be a string");
            }

            // call the handler
            _handler(
                cmon_tini_string(tini, url_val), cmon_tini_string(tini, version_val), _user_data);
        }
        else
        {
            //@TODO: silent ignore for now.
        }
    }

end:
    cmon_tini_destroy(tini);
    return err;
}

typedef struct
{
    cmon_pm * pm;
    cmon_idx idx;
} _pm_and_idx;

static inline void _add_dep_to_pm(cmon_str_view _url, cmon_str_view _version, void * _user_data)
{
    _pm_and_idx * pm_idx = _user_data;
    cmon_idx dep = cmon_pm_find_or_add_module(pm_idx->pm, _url, _version);
    cmon_pm_add_dep(pm_idx->pm, pm_idx->idx, dep);
}

cmon_bool cmon_pm_load_deps_file(cmon_pm * _pm, cmon_idx _mod, const char * _path)
{
    _pm_and_idx pm_idx = (_pm_and_idx){ _pm, _mod };
    return _parse_deps_tini(
        _pm->alloc, _path, _pm->err_msg, sizeof(_pm->err_msg), _add_dep_to_pm, &pm_idx);
}

cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, cmon_idx _mod, const char * _path)
{
}

// clones one module into the current working directory
// @NOTE: A lot of useful info over here regarding this topic here
// https://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset
// @TODO: Make this work with older git versions?
static inline cmon_bool _git_clone(cmon_pm * _pm, cmon_idx _idx)
{
    cmon_bool err = cmon_false;
    _mod * m = _get_mod(_pm, _idx);

    // if the folder already exists, don't do anything
    char clone_dir_path[CMON_PATH_MAX];
    cmon_join_paths(_pm->deps_dir, m->dirname, clone_dir_path, sizeof(clone_dir_path));
    if (cmon_fs_exists(clone_dir_path))
    {
        goto end;
    }

    cmon_str_builder_clear(_pm->str_builder);
    cmon_str_builder_append_fmt(
        _pm->str_builder, "git clone --depth=1 --branch %s %s %s", m->version, m->url, m->dirname);

    int status = cmon_exec(cmon_str_builder_c_str(_pm->str_builder), _pm->exec_output_builder);
    if (status != 0)
    {
        _err2(_pm,
              end,
              "Failed to clone git package from %s with version %s\ngit output:%s",
              m->url,
              m->version,
              cmon_str_builder_c_str(_pm->exec_output_builder));
    }

end:
    return err;
}

static inline cmon_bool _clone_and_add_deps(cmon_pm * _pm, cmon_idx _idx)
{
    cmon_bool err = cmon_false;
    _mod * m = _get_mod(_pm, _idx);

    if (_git_clone(_pm, _idx))
    {
        err = cmon_true;
        goto end;
    }

    char deps_tini_path[CMON_PATH_MAX];
    cmon_join_paths(_pm->deps_dir, m->dirname, deps_tini_path, sizeof(deps_tini_path));
    cmon_join_paths(deps_tini_path, "cmon_pm_deps.tini", deps_tini_path, sizeof(deps_tini_path));
    if (cmon_pm_load_deps_file(_pm, _idx, deps_tini_path))
    {
        err = cmon_true;
        goto end;
    }

    for (size_t i = 0; i < cmon_dyn_arr_count(&m->deps); ++i)
    {
        _clone_and_add_deps(_pm, m->deps[i]);
    }

end:

    return err;
}

static inline cmon_bool _change_cwd(const char * _path,
                                    char * _prev_dir,
                                    size_t _buf_size char * _err_buf,
                                    size_t _err_buf_size)
{
    cmon_bool err = cmon_false;
    if (!cmon_fs_getcwd(_prev_dir, _buf_size))
    {
        _err3(_err_buf, _err_buf_size, end, "failed to get current working directory");
    }
    if (cmon_fs_chdir(_path) == -1)
    {
        _err3(_err_buf, _err_buf_size, end, "failed to change working directory");
    }

end:
    return err;
}

static inline cmon_pm_lock_file * _lock_file_create(cmon_allocator * _alloc, size_t _dep_count)
{
    cmon_pm_lock_file * ret = CMON_CREATE(_alloc, cmon_pm_lock_file);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->deps, _alloc, _dep_count);
    return ret;
}

cmon_pm_lock_file * cmon_pm_resolve(cmon_pm * _pm)
{
    cmon_pm_lock_file * ret = NULL;
    cmon_dep_graph * graph = cmon_dep_graph_create(_pm->alloc);
    cmon_dyn_arr(cmon_idx) deps_arr;
    cmon_dyn_arr_init(&deps_arr, _pm->alloc, 16);

    // clone all top level dependencies and recursively add all the other dependencies coming in
    // along the way
    char current_dir[CMON_PATH_MAX];
    if (_change_cwd(
            _pm->deps_dir, current_dir, sizeof(current_dir), _pm->err_msg, sizeof(_pm->err_msg)))
    {
        goto end;
    }
    size_t root_count = cmon_dyn_arr_count(&_pm->mods);
    for (cmon_idx i = 0; i < root_count; ++i)
    {
        _clone_and_add_deps(_pm, i);
    }

    if (cmon_fs_chdir(current_dir))
    {
        _err(_pm, end, "failed to change working directory back to %s", current_dir);
    }

    // At this point all deps should be cloned and added to the pm. Detect circular dependencies!
    for (cmon_idx i = 0; i < cmon_dyn_arr_count(&_pm->mods); ++i)
    {
        cmon_dyn_arr_clear(&deps_arr);
        for (cmon_idx j = 0; j < cmon_dyn_arr_count(&_pm->mods[i].deps); ++j)
        {
            cmon_dyn_arr_append(&deps_arr, _pm->mods[i].deps[j]);
        }
        cmon_dep_graph_add(graph, i, deps_arr, cmon_dyn_arr_count(&deps_arr));
    }

    cmon_dep_graph_result res = cmon_dep_graph_resolve(graph);
    if (!res.array)
    {
        _mod * a = _get_mod(_pm, cmon_dep_graph_conflict_a(graph));
        _mod * b = _get_mod(_pm, cmon_dep_graph_conflict_b(graph));
        _err(_pm,
             end,
             "circular dependency between %s %s and %s %s",
             a->url,
             a->version,
             b->url,
             b->version);
    }

    ret = _lock_file_create(_pm->alloc, res.count);
    //@NOTE: Add the modules to the lock file in module, not dependency order, so we can reproduce
    // the dirname (i.e.: dep01, dep02 etc.) reliably.
    for (cmon_idx i = 0; i < cmon_dyn_arr_count(&_pm->mods); ++i)
    {
        _mod * mod = _get_mod(_pm, i);
        _locked_dep d;
        strcpy(d.url, mod->url);
        strcpy(d.version, mod->version);
        strcpy(d.dirname, mod->dirname);
        cmon_dyn_arr_append(&ret->deps, d);
    }

end:
    cmon_dyn_arr_dealloc(&deps_arr);
    cmon_dep_graph_destroy(graph);
    return ret;
}

const char * cmon_pm_err_msg(cmon_pm * _pm)
{
}

static inline void _add_dep_to_lock_file(cmon_str_view _url,
                                         cmon_str_view _version,
                                         void * _user_data)
{
    cmon_pm_lock_file * lf = _user_data;
    _locked_dep d;
    _cpy_str_view(_url, d.url, sizeof(d.url));
    _cpy_str_view(_version, d.version, sizeof(d.version));
    _dep_dirname(d.dirname, sizeof(d.dirname), cmon_dyn_arr_count(&lf->deps) + 1);
    cmon_dyn_arr_append(&lf->deps, d);
}

cmon_pm_lock_file * cmon_pm_lock_file_load(cmon_allocator * _alloc,
                                           const char * _path,
                                           char * _err_msg_buf,
                                           size_t _buf_size)
{
    cmon_pm_lock_file * ret = _lock_file_create(_alloc, 16);
    if (_parse_deps_tini(_alloc, _path, _err_msg_buf, _buf_size, _add_dep_to_lock_file, ret))
    {
        cmon_pm_lock_file_destroy(ret);
    }
    return ret;
}

void cmon_pm_lock_file_destroy(cmon_pm_lock_file * _lf)
{
    if (!_lf)
    {
        return;
    }

    cmon_dyn_arr_dealloc(&_lf->deps);
    CMON_DESTROY(_lf->alloc, _lf);
}

cmon_bool cmon_pm_lock_file_pull(cmon_pm_lock_file * _lf, const char * _dep_dir)
{
    cmon_bool err = cmon_false;
    char current_dir[CMON_PATH_MAX];
    if (_change_cwd(_dep_dir, current_dir, sizeof(current_dir), _lf->err_msg, sizeof(_lf->err_msg)))
    {
        goto end;
    }
    
    for (size_t i = 0; i < cmon_dyn_arr_count(&_lf->deps); ++i)
    {
        
    }

    if (cmon_fs_chdir(current_dir))
    {
        _err2(_lf, end, "failed to change working directory back to %s", current_dir);
    }
end:
    return err;
}

cmon_bool cmon_pm_lock_file_clean_dep_dir(cmon_pm_lock_file * _lf, const char * _dep_dir)
{
}

cmon_bool cmon_pm_lock_file_save(cmon_pm_lock_file * lf, const char * _path)
{
}

static inline const char * _advance_to_next_digit(const char * _pos)
{
    while (*_pos != '.' && *_pos != '\0')
    {
        ++_pos;
    }
    if (*_pos == '.')
    {
        ++_pos;
    }
    return _pos;
}

static inline cmon_bool _parse_symver(const char * _input,
                                      uint32_t * _out_major,
                                      uint32_t * _out_minor,
                                      uint32_t * _out_patch)
{
    const char * pos = _input;
    *_out_major = atoi(pos);
    if (*(pos = _advance_to_next_digit(pos)) == '\0')
        return cmon_true;
    *_out_minor = atoi(pos);
    if (*(pos = _advance_to_next_digit(pos)) == '\0')
        return cmon_true;
    *_out_patch = atoi(pos);
    return cmon_false;
}

static inline cmon_bool _parse_git_version(const char * _input,
                                           uint32_t * _out_major,
                                           uint32_t * _out_minor,
                                           uint32_t * _out_patch)
{
    const char * pos = _input;
    while ((isspace(*pos) || isalpha(*pos)) && *pos != '\0')
    {
        ++pos;
    }

    if (!isdigit(*pos))
        return cmon_true;

    return _parse_symver(pos, _out_major, _out_minor, _out_patch);
}

// 01. Read all direct deps.
// 02. Clone them
// 03. For each cloned dependency, read deps, add them to the pm if they are not there yet.
// 04. Create a deps_mapping.tini mapping every dependency

//@NOTE: A lot of useful info over here:
// https://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset
// cmon_bool cmon_pm_pull(cmon_pm * _pm)
// {
//     cmon_bool ret = cmon_false;
//     char cwd[CMON_PATH_MAX];
//     if (!cmon_fs_getcwd(cwd, sizeof(cwd)))
//     {
//         _err(_pm, end, "failed to get current working directory");
//     }
//     if (cmon_fs_chdir(_pm->deps_dir) == -1)
//     {
//         ret = cmon_true;
//         _err(_pm, end, "failed to change working directory");
//     }

//     int status = cmon_exec(cmon_str_builder_tmp_str(_pm->str_builder, "git --version"),
//                            _pm->exec_output_builder);
//     if (status != 0)
//     {
//         ret = cmon_true;
//         _err(_pm, end, "Failed to get git version. Do you have git installed?");
//     }

//     uint32_t major, minor, patch;
//     if (_parse_git_version(
//             cmon_str_builder_c_str(_pm->exec_output_builder), &major, &minor, &patch))
//     {
//         ret = cmon_true;
//         _err(_pm,
//              end,
//              "Failed to parse git version from string: %s",
//              cmon_str_builder_c_str(_pm->exec_output_builder));
//     }

//     for (size_t i = 0; i < cmon_dyn_arr_count(&_pm->deps); ++i)
//     {
//         cmon_str_builder_clear(_pm->str_builder);
//         cmon_str_builder_append_fmt(
//             _pm->str_builder,
//             "git clone --depth=1 --branch %s %s %s",
//             _pm->deps[i].version,
//             _pm->deps[i].url,
//             cmon_str_builder_tmp_str(_pm->exec_output_builder, "dep%02i", i));

//         int status = cmon_exec(cmon_str_builder_c_str(_pm->str_builder),
//         _pm->exec_output_builder); if (status != 0)
//         {
//             ret = cmon_true;
//             _err(_pm,
//                  end,
//                  "Failed to clone git package from %s with version %s\ngit output:%s",
//                  _pm->deps[i].url,
//                  _pm->deps[i].version,
//                  cmon_str_builder_c_str(_pm->exec_output_builder));
//         }
//     }

// end:
//     cmon_fs_chdir(cwd);
//     return ret;
// }

// static inline void _cpy_str_view(cmon_str_view _sv, char * _buf, size_t _buf_size)
// {
//     assert(_sv.end - _sv.begin < _buf_size);
//     strncpy(_buf, _sv.begin, _sv.end - _sv.begin);
// }

// cmon_idx cmon_pm_add_dep_git(cmon_pm * _pm,
//                              cmon_idx _mod,
//                              cmon_str_view _url,
//                              cmon_str_view _version)
// {
//     assert(_mod < cmon_dyn_arr_count(&_pm->deps));

//     for (size_t i = 0; i < cmon_dyn_arr_count(&_pm->deps); ++i)
//     {
//         //@TODO: For now we only check for version equality. In the future, we might want to add
//         a
//         // more sophisticated check. I.e. in the simple case where one version is latest and the
//         // other one is explicit, we might just want to pull the latest one. For now we will just
//         // have multiple.
//         if (cmon_str_view_c_str_cmp(_url, _pm->deps[i].url) == 0 &&
//             cmon_str_view_c_str_cmp(_url, _pm->deps[i].version) == 0)
//         {
//             return CMON_INVALID_IDX;
//         }
//     }

//     _dep d;
//     d.idx = cmon_dyn_arr_count(&_pm->deps);
//     _cpy_str_view(_url, d.url, sizeof(d.url));
//     _cpy_str_view(_version, d.version, sizeof(d.version));
//     cmon_dyn_arr_init(&d.deps, _pm->alloc, 4);
//     cmon_dyn_arr_append(&_pm->deps, d);

//     cmon_dyn_arr_append(&_pm->deps[_mod].deps, d.idx);

//     return d.idx;
// }

// cmon_idx cmon_pm_add_dep_git_c_str(cmon_pm * _pm,
//                                    cmon_idx _mod,
//                                    const char * _url,
//                                    const char * _version)
// {
//     return cmon_pm_add_dep_git(_pm, _mod, cmon_str_view_make(_url),
//     cmon_str_view_make(_version));
// }

// // cmon_bool cmon_pm_remove(cmon_pm * _pm, const char * _url, const char * _version)
// // {
// // }

// cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, cmon_idx _mod_idx, const char * _path)
// {
// }

// cmon_idx cmon_pm_load_deps_file(cmon_pm * _pm, const char * _path)
// {
//     cmon_tini_err terr;
//     cmon_tini * tini;
//     cmon_idx ret;

//     ret = cmon_pm_add_module(_pm, _mod_path);
//     if (!cmon_is_valid_idx(ret))
//     {
//         _err(_pm, end, "module '%s' already exists in the package manager", _mod_path);
//     }

//     if (tini = cmon_tini_parse_file(_pm->alloc, _path, &terr))
//     {
//         _err(_pm,
//              end,
//              "failed to load/parse dependency file:%s:%lu:%lu: %s",
//              terr.filename,
//              terr.line,
//              terr.line_offset,
//              terr.msg);
//     }

//     cmon_idx root_obj = cmon_tini_root_obj(tini);
//     cmon_idx deps_arr = cmon_tini_obj_find(tini, root_obj, "deps");

//     for (size_t i = 0; i < cmon_tini_child_count(tini, deps_arr); ++i)
//     {
//         cmon_idx child = cmon_tini_child(tini, deps_arr, i);
//         if (cmon_tini_kind(tini, child) == cmon_tinik_obj)
//         {
//             cmon_idx url = cmon_tini_obj_find(tini, root_obj, "url");
//             if (!cmon_is_valid_idx(url))
//             {
//                 _err(_pm, end, "missing url in dependency obj");
//             }
//             cmon_idx version = cmon_tini_obj_find(tini, root_obj, "version");
//             if (!cmon_is_valid_idx(version))
//             {
//                 _err(_pm, end, "missing version in dependency obj");
//             }

//             cmon_idx url_val = cmon_tini_pair_value(tini, url);
//             if (cmon_tini_kind(tini, url_val) != cmon_tinik_string)
//             {
//                 _err(_pm, end, "url field has to be a string");
//             }

//             cmon_idx version_val = cmon_tini_pair_value(tini, version);
//             if (cmon_tini_kind(tini, version_val) != cmon_tinik_string)
//             {
//                 _err(_pm, end, "version field has to be a string");
//             }

//             //@TODO: Check if a valid index is returned? If not it means we had a duplicate entry
//             in
//             //the tini file...not really breaking though so ignoring it for the time being seems
//             //good
//             cmon_pm_add_dep_git(
//                 _pm, ret, cmon_tini_string(tini, url_val), cmon_tini_string(tini, version_val));
//         }
//         else
//         {
//             //@TODO: silent ignore for now.
//         }
//     }

// end:
//     cmon_tini_destroy(tini);
//     return ret;
// }
