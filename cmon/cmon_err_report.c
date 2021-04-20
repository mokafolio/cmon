#include <cmon/cmon_err_report.h>

cmon_err_report cmon_err_report_make_empty()
{
    cmon_err_report ret;
    memset(&ret, 0, sizeof(ret));
    return ret;
}

cmon_err_report cmon_err_report_make(const char * _file,
                                     size_t _line,
                                     size_t _line_off,
                                     const char * _msg)
{
    cmon_err_report ret;
    assert(strlen(_file) < CMON_FILENAME_MAX - 1);
    assert(strlen(_msg) < CMON_ERR_MSG_MAX - 1);
    ret.line = _line;
    ret.line_offset = _line_off;
    strcpy(ret.filename, _file);
    strcpy(ret.msg, _msg);
    return ret;
}

cmon_err_report cmon_err_report_copy(cmon_err_report * _er)
{
    return *_er;
}

cmon_bool cmon_err_report_is_empty(cmon_err_report * _er)
{
    return _er->line == 0;
}

void cmon_err_report_print(cmon_err_report * _er)
{
    printf("%s:%lu:%lu: %s\n", _er->filename, _er->line, _er->line_offset, _er->msg);
}
