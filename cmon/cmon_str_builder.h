#ifndef CMON_CMON_STR_BUILDER_H
#define CMON_CMON_STR_BUILDER_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_str_builder cmon_str_builder;

CMON_API cmon_str_builder * cmon_str_builder_create(cmon_allocator * _alloc, size_t _cap);
CMON_API void cmon_str_builder_destroy(cmon_str_builder * _s);
CMON_API void cmon_str_builder_clear(cmon_str_builder * _s);
CMON_API void cmon_str_builder_append_fmt_v(cmon_str_builder * _s, const char * _fmt, va_list _args);
CMON_API void cmon_str_builder_append_fmt(cmon_str_builder * _s, const char * _fmt, ...);
CMON_API void cmon_str_builder_append(cmon_str_builder * _s, const char * _str);
CMON_API const char * cmon_str_builder_c_str(cmon_str_builder * _s);

#endif // CMON_CMON_STR_BUILDER_H
