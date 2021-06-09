#ifndef CMON_CMON_BUILDER_ST_H
#define CMON_CMON_BUILDER_ST_H

#include <cmon/cmon_codegen.h>
#include <cmon/cmon_err_report.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_src.h>

typedef struct cmon_builder_st cmon_builder_st;

CMON_API cmon_builder_st * cmon_builder_st_create(cmon_allocator * _alloc,
                                                  size_t _max_errors,
                                                  cmon_src * _src,
                                                  cmon_modules * _mods);
CMON_API void cmon_builder_st_destroy(cmon_builder_st * _b);
CMON_API cmon_bool cmon_builder_st_build(cmon_builder_st * _b, cmon_codegen * _codegen, const char * _build_dir);
CMON_API cmon_bool cmon_builder_st_errors(cmon_builder_st * _b,
                                          cmon_err_report ** _out_errs,
                                          size_t * _out_count);
CMON_API cmon_types * cmon_builder_st_types(cmon_builder_st * _b);

#endif // CMON_CMON_BUILDER_ST_H
