#ifndef CMON_CMON_ERR_BUILDER_H
#define CMON_CMON_ERR_BUILDER_H

#include <cmon/cmon_allocator.h>
#include <cmon/cmon_err_report.h>

typedef struct cmon_err_builder cmon_err_builder;

CMON_API cmon_err_builder * cmon_err_builder_create(cmon_allocator * _alloc);
CMON_API void cmon_err_builder_destroy(cmon_err_builder * _e);
CMON_API cmon_err_report cmon_err_builder_make_err()

#endif //CMON_CMON_ERR_BUILDER_H
