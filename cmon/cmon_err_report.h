#ifndef CMON_CMON_ERR_REPORT_H
#define CMON_CMON_ERR_REPORT_H

#include <cmon/cmon_base.h>

typedef struct cmon_src cmon_src;

typedef struct
{
    cmon_idx src_file_idx;
    cmon_idx token_idx;
    char msg[CMON_ERR_MSG_MAX];
} cmon_err_report;

CMON_API cmon_err_report cmon_err_report_make_empty();
CMON_API cmon_err_report cmon_err_report_make(cmon_idx _src_file_idx, cmon_idx _tok_idx, const char * _msg);
CMON_API cmon_bool cmon_err_report_is_empty(cmon_err_report * _er);
CMON_API const char * cmon_err_report_filename(cmon_err_report * _er, cmon_src * _src);
CMON_API size_t cmon_err_report_line(cmon_err_report * _er, cmon_src * _src);
CMON_API size_t cmon_err_report_line_offset(cmon_err_report * _er, cmon_src * _src);
CMON_API cmon_str_view cmon_err_report_line_str_view(cmon_err_report * _er, cmon_src * _src);
CMON_API void cmon_err_report_print(cmon_err_report * _er, cmon_src * _src);

#endif // CMON_CMON_ERR_REPORT_H
