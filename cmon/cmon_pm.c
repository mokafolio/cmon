#include <cmon/cmon_exec.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_pm.h>
#include <cmon/cmon_str_builder.h>

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
} cmon_pm;

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

static inline cmon_bool _parse_git_version(const char * _input,
                                           uint32_t * _out_major,
                                           uint32_t * _out_minor,
                                           uint32_t * _out_patch)
{

    return cmon_false;
}

//@NOTE: A lot of useful info over here:
// https://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset
cmon_bool cmon_pm_pull(cmon_pm * _pm)
{
    char cwd[CMON_PATH_MAX];
    cmon_fs_getcwd(cwd, sizeof(cwd));
    if (cmon_fs_chdir(_pm->deps_dir) == -1)
    {
        return cmon_true;
    }

    int status = cmon_exec(cmon_str_builder_tmp_str(_pm->str_builder, "git --version"),
                           _pm->exec_output_builder);
    if (status == EXIT_FAILURE)
    {
    }

    uint32_t major, minor, patch;
    if (_parse_git_version(
            cmon_str_builder_c_str(_pm->exec_output_builder), &major, &minor, &patch))
    {
    }

    for (size_t i = 0; i < cmon_dyn_arr_count(&_pm->deps[i]); ++i)
    {
        cmon_str_builder_clear(_pm->str_builder);
        cmon_str_builder_append_fmt(
            _pm->str_builder,
            "git clone --depth=1 --branch %s %s %s",
            _pm->deps[i].version,
            _pm->deps[i].url,
            cmon_str_builder_tmp_str(_pm->exec_output_builder, "dep%02i", i));

        int status = cmon_exec(cmon_str_builder_c_str(_pm->str_builder), _pm->exec_output_builder);
    }

    cmon_fs_chdir(cwd);
    return cmon_false;
}

void cmon_pm_clean_dep_dir(cmon_pm * _pm)
{
}

void cmon_pm_add_git(cmon_pm * _pm, const char * _url, const char * _version)
{
}

cmon_bool cmon_pm_remove(cmon_pm * _pm, const char * _url, const char * _version)
{
}

cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, const char * _path)
{
}

cmon_bool cmon_pm_load_deps_file(cmon_pm * _pm, const char * _path)
{
}
