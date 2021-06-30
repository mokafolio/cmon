#ifndef CMON_CMON_PM_H
#define CMON_CMON_PM_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_pm cmon_pm;

CMON_API cmon_pm * cmon_pm_create(cmon_allocator * _a);
CMON_API void cmon_pm_destroy(cmon_pm * _pm);
CMON_API void cmon_pm_clean_dep_dir(cmon_pm * _pm);
CMON_API void cmon_pm_add_git(cmon_pm * _pm, const char * _url, const char * _min_version, const char * _max_version);

#endif //CMON_CMON_PM_H
