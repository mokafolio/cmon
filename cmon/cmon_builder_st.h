#ifndef CMON_CMON_BUILDER_ST_H
#define CMON_CMON_BUILDER_ST_H

#include <cmon/cmon_src.h>
#include <cmon/cmon_modules.h>

typedef struct cmon_builder_st cmon_builder_st;

CMON_API cmon_builder_st * cmon_builder_st_create(cmon_allocator * _alloc, size_t _max_errors, cmon_src * _src, cmon_modules * _mods);
CMON_API void cmon_builder_st_destroy(cmon_builder_st * _b);
CMON_API cmon_bool cmon_builder_st_build(cmon_builder_st * _b);

#endif //CMON_CMON_BUILDER_ST_H
