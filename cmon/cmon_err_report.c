#include <cmon/cmon_err_report.h>
#include <cmon/cmon_src.h>
#include <cmon/cmon_tokens.h>

cmon_err_report cmon_err_report_make_empty()
{
    cmon_err_report ret;
    ret.src_file_idx = CMON_INVALID_IDX;
    return ret;
}

cmon_err_report cmon_err_report_make(cmon_idx _src_file_idx,
                                              cmon_idx _tok_first,
                                              cmon_idx _tok_of_interest,
                                              cmon_idx _tok_last, const char * _msg)
{
    cmon_err_report ret;
    assert(strlen(_msg) < CMON_ERR_MSG_MAX - 1);
    ret.src_file_idx = _src_file_idx;
    ret.tok_first = _tok_first;
    ret.tok_of_interest = _tok_of_interest;
    ret.tok_last = _tok_last;
    strcpy(ret.msg, _msg);
    return ret;
}

cmon_bool cmon_err_report_is_empty(cmon_err_report * _er)
{
    return !cmon_is_valid_idx(_er->src_file_idx);
}

static inline cmon_tokens * _err_toks(cmon_err_report * _er, cmon_src * _src)
{
    assert(cmon_is_valid_idx(_er->src_file_idx));
    return cmon_src_tokens(_src, _er->src_file_idx);
}

// static inline cmon_idx _first_tok(cmon_err_report * _er, cmon_src * _src)
// {
//     return cmon_ast_token_first(cmon_src_ast(_src, _er->src_file_idx), _er->ast);
// }

const char * cmon_err_report_filename(cmon_err_report * _er, cmon_src * _src)
{
    return cmon_src_filename(_src, _er->src_file_idx);
}

size_t cmon_err_report_line(cmon_err_report * _er, cmon_src * _src)
{
    return cmon_tokens_line(_err_toks(_er, _src), _er->tok_of_interest);
}

size_t cmon_err_report_line_offset(cmon_err_report * _er, cmon_src * _src)
{
    return cmon_tokens_line_offset(_err_toks(_er, _src), _er->tok_of_interest);
}

cmon_idx cmon_err_report_token_first(cmon_err_report * _er)
{
    return _er->tok_first;
}

cmon_idx cmon_err_report_token_last(cmon_err_report * _er)
{
    return _er->tok_last;
}

cmon_idx cmon_err_report_token(cmon_err_report * _er)
{
    return _er->tok_of_interest;
}

size_t cmon_err_report_line_count(cmon_err_report * _er, cmon_src * _src)
{
    return cmon_tokens_line(_err_toks(_er, _src), _er->tok_last) - cmon_tokens_line(_err_toks(_er, _src), _er->tok_first) + 1;
}

size_t cmon_err_report_absolute_line(cmon_err_report * _er, cmon_src * _src, size_t _line)
{
    return cmon_tokens_line(_err_toks(_er, _src), _er->tok_first) + _line;
}

cmon_str_view cmon_err_report_line_str_view(cmon_err_report * _er, cmon_src * _src, size_t _line)
{
    return cmon_src_line(_src, _er->src_file_idx, cmon_err_report_absolute_line(_er, _src, _line)); 
}

// cmon_str_view cmon_err_report_line_str_view(cmon_err_report * _er, cmon_src * _src)
// {
//     return cmon_src_line(_src, _er->src_file_idx, cmon_err_report_line(_er, _src));
// }

void cmon_err_report_print(cmon_err_report * _er, cmon_src * _src)
{
    if (cmon_is_valid_idx(_er->tok_first))
    {
        printf("%s:%lu:%lu: %s\n",
               cmon_err_report_filename(_er, _src),
               cmon_err_report_line(_er, _src),
               cmon_err_report_line_offset(_er, _src),
               _er->msg);
    }
    else
    {
        printf("%s: %s\n", cmon_err_report_filename(_er, _src), _er->msg);
    }
}
