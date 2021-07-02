#ifndef CMON_CMON_UTIL_H
#define CMON_CMON_UTIL_H

#include <cmon/cmon_allocator.h>
#include <stdarg.h> //for varargs

CMON_API cmon_str_view cmon_str_view_make(const char * c_str);
CMON_API cmon_str_view cmon_str_view_make_empty();
CMON_API size_t cmon_str_view_len(cmon_str_view _v);
CMON_API char * cmon_str_view_copy(cmon_allocator * _alloc, cmon_str_view _view);
CMON_API char * cmon_c_str_copy(cmon_allocator * _alloc, const char * _str);
CMON_API const char * cmon_str_view_tmp_str(cmon_str_view _view, char * _tmp);
CMON_API int cmon_str_view_cmp(cmon_str_view _a, cmon_str_view _b);
CMON_API int cmon_str_view_c_str_cmp(cmon_str_view _a, const char * _str);
CMON_API char * cmon_c_str_append(char * _dst, const char * _src);
CMON_API char * cmon_c_str_append_string_view(char * _dst, cmon_str_view _src);
CMON_API void cmon_c_str_free(cmon_allocator * _alloc, char * _str);
CMON_API char * cmon_str_create_v(cmon_allocator * _alloc, const char * _fmt, va_list _argp);
CMON_API char * cmon_str_create(cmon_allocator * _alloc, const char * _fmt, ...);
CMON_API const char * cmon_file_ext(const char * _filename, char * _buf, size_t _buf_size);
CMON_API const char * cmon_filename(const char * _path, char * _buf, size_t _buf_size);
CMON_API const char * cmon_join_paths(const char * _a, const char * _b, char * _buf, size_t _buf_size);
CMON_API void cmon_panic(const char * _fmt, ...);

// helper macros to postfix __VA_ARGS__ with a -1 delimiter.
#define _CMON_VARARGS_COMBINE(...) __VA_ARGS__
#define _CMON_VARARG_APPEND_LAST(last, ...) _CMON_VARARGS_COMBINE(__VA_ARGS__, last)
// #define _CMON_VARARG_APPEND_LAST(last, ...) _CMON_VARARG_APPEND_LAST_IMPL(last, ##__VA_ARGS__)

#endif //CMON_CMON_UTIL_H
