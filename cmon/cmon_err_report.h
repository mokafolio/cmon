#ifndef CMON_CMON_ERR_REPORT_H
#define CMON_CMON_ERR_REPORT_H

#include <cmon/cmon_base.h>

typedef struct cmon_src cmon_src;

typedef struct
{
    cmon_idx src_file_idx;
    cmon_idx tok_first, tok_of_interest, tok_last;
    char msg[CMON_ERR_MSG_MAX];
} cmon_err_report;

CMON_API cmon_err_report cmon_err_report_make_empty();
CMON_API cmon_err_report cmon_err_report_make(cmon_idx _src_file_idx,
                                              cmon_idx _tok_first,
                                              cmon_idx _tok_of_interest,
                                              cmon_idx _tok_last,
                                              const char * _msg);
CMON_API cmon_bool cmon_err_report_is_empty(cmon_err_report * _er);
CMON_API const char * cmon_err_report_filename(cmon_err_report * _er, cmon_src * _src);


CMON_API size_t cmon_err_report_line(cmon_err_report * _er, cmon_src * _src);
CMON_API size_t cmon_err_report_line_offset(cmon_err_report * _er, cmon_src * _src);

CMON_API cmon_idx cmon_err_report_token_first(cmon_err_report * _er);
CMON_API cmon_idx cmon_err_report_token_last(cmon_err_report * _er);
CMON_API cmon_idx cmon_err_report_token(cmon_err_report * _er);

CMON_API size_t cmon_err_report_line_count(cmon_err_report * _er, cmon_src * _src);
CMON_API size_t cmon_err_report_absolute_line(cmon_err_report * _er, cmon_src * _src, size_t _line);
CMON_API cmon_str_view cmon_err_report_line_str_view(cmon_err_report * _er, cmon_src * _src, size_t _line);

// CMON_API cmon_str_view cmon_err_report_line_str_view(cmon_err_report * _er, cmon_src * _src);
CMON_API void cmon_err_report_print(cmon_err_report * _er, cmon_src * _src);

#endif // CMON_CMON_ERR_REPORT_H
