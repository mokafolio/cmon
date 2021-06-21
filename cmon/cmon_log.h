#ifndef CMON_CMON_LOG_H
#define CMON_CMON_LOG_H

#include <cmon/cmon_allocator.h>
#include <cmon/cmon_err_report.h>
#include <stdarg.h>

typedef struct cmon_log cmon_log;

typedef enum
{
    cmon_log_color_default,
    cmon_log_color_black,
    cmon_log_color_red,
    cmon_log_color_green,
    cmon_log_color_yellow,
    cmon_log_color_blue,
    cmon_log_color_magenta,
    cmon_log_color_cyan,
    cmon_log_color_white
} cmon_log_color;

typedef enum
{
    cmon_log_style_none = 1 << 0,
    cmon_log_style_bold = 1 << 1,
    cmon_log_style_light = 1 << 2,
    cmon_log_style_underline = 1 << 3
} cmon_log_style;

CMON_API cmon_log * cmon_log_create(cmon_allocator * _alloc,
                                    const char * _name,
                                    const char * _path,
                                    cmon_bool _verbose);
CMON_API void cmon_log_destroy(cmon_log * _log);
CMON_API void cmon_log_write_v(cmon_log * _log, const char * _fmt, va_list _args);
CMON_API void cmon_log_write(cmon_log * _log, const char * _fmt, ...);
CMON_API void cmon_log_write_styled_v(cmon_log * _log, cmon_log_color _color, cmon_log_color _bg_color, cmon_log_style _style, const char * _fmt, va_list _args);
CMON_API void cmon_log_write_styled(cmon_log * _log, cmon_log_color _color, cmon_log_color _bg_color, cmon_log_style _style, const char * _fmt, ...);
CMON_API void cmon_log_write_err_report(cmon_log * _log, cmon_err_report * _err, cmon_src * _src);

#endif // CMON_CMON_LOG_H
