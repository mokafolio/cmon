#include <cmon/cmon_pm.h>
#include <cmon/cmon_pfs.h>

typedef struct
{
    char url[CMON_PATH_MAX];
    char version[CMON_FILENAME_MAX];
} dep;

typedef struct cmon_pm
{
    cmon_allocator * alloc;
    char dep_dir[CMON_PATH_MAX];
    cmon_dyn_arr(dep) deps;
} cmon_pm;

cmon_pm * cmon_pm_create(cmon_allocator * _a, const char * _dep_dir)
{
    cmon_pm * ret = CMON_CREATE(_a, cmon_pm);
    ret->alloc = _a;
    strcpy(ret->dep_dir, _dep_dir);
    cmon_dyn_arr_init(&ret->deps, _a, 4);
    return ret;
}

void cmon_pm_destroy(cmon_pm * _pm)
{
    if(!_pm)
    {
        return;
    }
    cmon_dyn_arr_dealloc(&_pm->deps);
}

cmon_bool cmon_pm_pull(cmon_pm * _pm)
{

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
