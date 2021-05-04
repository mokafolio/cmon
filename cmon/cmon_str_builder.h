#ifndef CMON_CMON_STR_BUILDER_H
#define CMON_CMON_STR_BUILDER_H

#include <cmon/cmon_allocator.h>

//dynamically builds one zero terminated string
typedef struct cmon_str_builder cmon_str_builder;

CMON_API cmon_str_builder * cmon_str_builder_create(cmon_allocator * _alloc, size_t _cap);
CMON_API void cmon_str_builder_destroy(cmon_str_builder * _s);
CMON_API void cmon_str_builder_clear(cmon_str_builder * _s);
CMON_API void cmon_str_builder_append_fmt_v(cmon_str_builder * _s, const char * _fmt, va_list _args);
CMON_API void cmon_str_builder_append_fmt(cmon_str_builder * _s, const char * _fmt, ...);
CMON_API void cmon_str_builder_append(cmon_str_builder * _s, const char * _str);
CMON_API size_t cmon_str_builder_count(cmon_str_builder * _s);
CMON_API const char * cmon_str_builder_c_str(cmon_str_builder * _s);
CMON_API const char * cmon_str_builder_tmp_str_v(cmon_str_builder * _s, const char * _fmt, va_list _args);
CMON_API const char * cmon_str_builder_tmp_str(cmon_str_builder * _s, const char * _fmt, ...);

//multiple zero terminated strings stored in continuous memory
typedef struct cmon_str_buf cmon_str_buf;

CMON_API cmon_str_buf * cmon_str_buf_create(cmon_allocator * _alloc, size_t _cap);
CMON_API void cmon_str_buf_destroy(cmon_str_buf * _s);
CMON_API size_t cmon_str_buf_append(cmon_str_buf * _s, const char * _str);
CMON_API const char * cmon_str_buf_get(cmon_str_buf * _s, size_t _offset);
CMON_API void cmon_str_buf_clear(cmon_str_buf * _s);

//helper to store shorter strings and minimize dynamic mem allocation
#define CMON_SHORT_STR_BUF_LEN 128
typedef struct cmon_short_str
{
    cmon_allocator * alloc;
    union {
        char buf[CMON_SHORT_STR_BUF_LEN];
        char * ptr;
    } data;
} cmon_short_str;

CMON_API cmon_short_str cmon_short_str_make(cmon_allocator * _alloc, const char * _str);
CMON_API void cmon_short_str_dealloc(cmon_short_str * _str);
CMON_API const char * cmon_short_str_c_str(cmon_short_str * _str);

#endif // CMON_CMON_STR_BUILDER_H
