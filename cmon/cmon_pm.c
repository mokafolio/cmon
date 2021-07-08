#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_exec.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_pm.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tini.h>

typedef struct
{
    char url[CMON_PATH_MAX];
    char version[CMON_FILENAME_MAX];
} dep;

typedef struct cmon_pm
{
    cmon_allocator * alloc;
    cmon_str_builder * str_builder;
    cmon_str_builder * exec_output_builder;
    char deps_dir[CMON_PATH_MAX];
    cmon_dyn_arr(dep) deps;
    char err_msg[CMON_ERR_MSG_MAX * 2]; // extra long to accomodate long urls and avoid snprintf
                                        // possible overflow warning
} cmon_pm;

#define _err(_pm, _goto, _fmt, ...)                                                                \
    do                                                                                             \
    {                                                                                              \
        snprintf((_pm)->err_msg, sizeof((_pm)->err_msg), _fmt, ##__VA_ARGS__);                     \
        goto _goto;                                                                                \
    } while (0)

cmon_pm * cmon_pm_create(cmon_allocator * _a, const char * _dep_dir)
{
    cmon_pm * ret = CMON_CREATE(_a, cmon_pm);
    ret->alloc = _a;
    ret->str_builder = cmon_str_builder_create(_a, CMON_PATH_MAX);
    ret->exec_output_builder = cmon_str_builder_create(_a, 1024);
    strcpy(ret->deps_dir, _dep_dir);
    cmon_dyn_arr_init(&ret->deps, _a, 4);
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
    cmon_dyn_arr_dealloc(&_pm->deps);
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
    if ((pos = _advance_to_next_digit(pos)) == '\0')
        return cmon_true;
    *_out_minor = atoi(pos);
    if ((pos = _advance_to_next_digit(pos)) == '\0')
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
cmon_bool cmon_pm_pull(cmon_pm * _pm)
{
    cmon_bool ret = cmon_false;
    char cwd[CMON_PATH_MAX];
    if (!cmon_fs_getcwd(cwd, sizeof(cwd)))
    {
        _err(_pm, end, "failed to get current working directory");
    }
    if (cmon_fs_chdir(_pm->deps_dir) == -1)
    {
        ret = cmon_true;
        _err(_pm, end, "failed to change working directory");
    }

    int status = cmon_exec(cmon_str_builder_tmp_str(_pm->str_builder, "git --version"),
                           _pm->exec_output_builder);
    if (status != 0)
    {
        ret = cmon_true;
        _err(_pm, end, "Failed to get git version. Do you have git installed?");
    }

    uint32_t major, minor, patch;
    if (_parse_git_version(
            cmon_str_builder_c_str(_pm->exec_output_builder), &major, &minor, &patch))
    {
        ret = cmon_true;
        _err(_pm,
             end,
             "Failed to parse git version from string: %s",
             cmon_str_builder_c_str(_pm->exec_output_builder));
    }

    for (size_t i = 0; i < cmon_dyn_arr_count(&_pm->deps); ++i)
    {
        cmon_str_builder_clear(_pm->str_builder);
        cmon_str_builder_append_fmt(
            _pm->str_builder,
            "git clone --depth=1 --branch %s %s %s",
            _pm->deps[i].version,
            _pm->deps[i].url,
            cmon_str_builder_tmp_str(_pm->exec_output_builder, "dep%02i", i));

        int status = cmon_exec(cmon_str_builder_c_str(_pm->str_builder), _pm->exec_output_builder);
        if (status != 0)
        {
            ret = cmon_true;
            _err(_pm,
                 end,
                 "Failed to clone git package from %s with version %s\ngit output:%s",
                 _pm->deps[i].url,
                 _pm->deps[i].version,
                 cmon_str_builder_c_str(_pm->exec_output_builder));
        }
    }

end:
    cmon_fs_chdir(cwd);
    return ret;
}

void cmon_pm_clean_dep_dir(cmon_pm * _pm)
{
}

cmon_idx cmon_pm_add_module(cmon_pm * _pm, const char * _path)
{
    
}

void cmon_pm_add_dep_git(cmon_pm * _pm, cmon_idx _mod, const char * _url, const char * _version)
{
}

// cmon_bool cmon_pm_remove(cmon_pm * _pm, const char * _url, const char * _version)
// {
// }

cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, cmon_idx _mod_idx, const char * _path)
{
}

cmon_idx cmon_pm_load_deps_file(cmon_pm * _pm, const char * _mod_path, const char * _path)
{
    cmon_tini_err terr;
    cmon_tini * tini;
    if (tini = cmon_tini_parse_file(_pm->alloc, _path, &terr))
    {
        _err(_pm,
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
                _err(_pm, end, "missing url in dependency obj");
            }
            cmon_idx version = cmon_tini_obj_find(tini, root_obj, "version");
            if (!cmon_is_valid_idx(version))
            {
                _err(_pm, end, "missing version in dependency obj");
            }

            cmon_idx url_val = cmon_tini_pair_value(tini, url);
            if (cmon_tini_kind(tini, url_val) != cmon_tinik_string)
            {
                _err(_pm, end, "url field has to be a string");
            }

            cmon_idx version_val = cmon_tini_pair_value(tini, version);
            if (cmon_tini_kind(tini, version_val) != cmon_tinik_string)
            {
                _err(_pm, end, "version field has to be a string");
            }
        }
        else
        {
            //@TODO: silent ignore for now.
        }
    }

end:
    cmon_tini_destroy(tini);
}

const char * cmon_pm_err_msg(cmon_pm * _pm)
{
    return _pm->err_msg;
}
