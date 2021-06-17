#ifndef CMON_CMON_LOG_H
#define CMON_CMON_LOG_H

#include <cmon/cmon_allocator.h>
#include <stdarg.h>

typedef struct cmon_log cmon_log;

CMON_API cmon_log * cmon_log_create(cmon_allocator * _alloc, const char * _name, const char * _path, cmon_bool _verbose);
CMON_API void cmon_log_destroy(cmon_log * _log);
CMON_API void cmon_log_write_v(cmon_log * _log, const char * _fmt, va_list _args);
CMON_API void cmon_log_write(cmon_log * _log, const char * _fmt, ...);

#endif //CMON_CMON_LOG_H
