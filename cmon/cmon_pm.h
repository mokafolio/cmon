#ifndef CMON_CMON_PM_H
#define CMON_CMON_PM_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_pm cmon_pm;

CMON_API cmon_pm * cmon_pm_create(cmon_allocator * _a, const char * _dep_dir);
CMON_API void cmon_pm_destroy(cmon_pm * _pm);

CMON_API void cmon_pm_clean_dep_dir(cmon_pm * _pm);

// Add a module to the pm. A module has a list of direct dependencies.
CMON_API cmon_idx cmon_pm_add_module(cmon_pm * _pm, const char * _path);
// Add a dependency to a module.
CMON_API void cmon_pm_add_dep_git(cmon_pm * _pm,
                                  cmon_idx _mod,
                                  const char * _url,
                                  const char * _version);

// this function will recursively clone all the dependencies.
// 01. recursively pull all dependencies while adding them to the PM.
// 02. create a dependency tree and resolve it to potentially detect circular dependencies.
// 03. write deps_mapping.tini to map each modules dependencies to the correct place (to deal with
// modules having the same name)
CMON_API cmon_bool cmon_pm_pull(cmon_pm * _pm);

// CMON_API cmon_bool cmon_pm_remove(cmon_pm * _pm, const char * _url, const char * _version);
CMON_API cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, cmon_idx _mod, const char * _path);
CMON_API cmon_idx cmon_pm_load_deps_file(cmon_pm * _pm, const char * _mod_path, const char * _path);
CMON_API const char * cmon_pm_err_msg(cmon_pm * _pm);

#endif // CMON_CMON_PM_H
