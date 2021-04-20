#ifndef CMON_CMON_ERR_REPORT_H
#define CMON_CMON_ERR_REPORT_H

#include <cmon/cmon_base.h>

typedef struct
{
    //@TODO: instead of line/line_off simply store token and src file idx?
    size_t line;
    size_t line_offset;
    char filename[CMON_FILENAME_MAX];
    char msg[CMON_ERR_MSG_MAX];
} cmon_err_report;

CMON_API cmon_err_report cmon_err_report_make_empty();
CMON_API cmon_err_report cmon_err_report_make(const char * _file, size_t _line, size_t _line_off, const char * _msg);
CMON_API cmon_err_report cmon_err_report_copy(cmon_err_report * _er);
CMON_API cmon_bool cmon_err_report_is_empty(cmon_err_report * _er);
CMON_API void cmon_err_report_print(cmon_err_report * _er);

#endif // CMON_CMON_ERR_REPORT_H
