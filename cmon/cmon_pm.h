#ifndef CMON_CMON_PM_H
#define CMON_CMON_PM_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_pm cmon_pm;

CMON_API cmon_pm * cmon_pm_create(cmon_allocator * _a, const char * _dep_dir);
CMON_API void cmon_pm_destroy(cmon_pm * _pm);
CMON_API cmon_bool cmon_pm_pull(cmon_pm * _pm);
CMON_API void cmon_pm_clean_dep_dir(cmon_pm * _pm);
CMON_API void cmon_pm_add_git(cmon_pm * _pm, const char * _url, const char * _version);
CMON_API cmon_bool cmon_pm_remove(cmon_pm * _pm, const char * _url, const char * _version);
CMON_API cmon_bool cmon_pm_save_deps_file(cmon_pm * _pm, const char * _path);
CMON_API cmon_bool cmon_pm_load_deps_file(cmon_pm * _pm, const char * _path);

#endif //CMON_CMON_PM_H
