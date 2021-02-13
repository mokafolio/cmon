#ifndef CMON_CMON_TOKENS_H
#define CMON_CMON_TOKENS_H

#include <cmon/cmon_src.h>
#include <cmon/cmon_err_report.h>

typedef enum
{
    cmon_tk_ident,
    cmon_tk_int,
    cmon_tk_float,
    cmon_tk_string,
    cmon_tk_curl_open,
    cmon_tk_curl_close,
    cmon_tk_paran_open,
    cmon_tk_paran_close,
    cmon_tk_equals,
    cmon_tk_not_equals,
    cmon_tk_assign,
    cmon_tk_plus_assign,
    cmon_tk_minus_assign,
    cmon_tk_mult_assign,
    cmon_tk_div_assign,
    cmon_tk_mod_assign,
    cmon_tk_bw_left_assign,
    cmon_tk_bw_right_assign,
    cmon_tk_bw_and_assign,
    cmon_tk_bw_xor_assign,
    cmon_tk_bw_or_assign,
    cmon_tk_plus,
    cmon_tk_minus,
    cmon_tk_inc,
    cmon_tk_dec,
    cmon_tk_mult,
    cmon_tk_div,
    cmon_tk_mod,
    cmon_tk_bw_left,
    cmon_tk_bw_right,
    cmon_tk_bw_and,
    cmon_tk_bw_xor,
    cmon_tk_bw_or,
    cmon_tk_bw_not,
    cmon_tk_fn,
    cmon_tk_dot,
    cmon_tk_comma,
    cmon_tk_colon,
    cmon_tk_exclam,
    cmon_tk_if,
    cmon_tk_else,
    cmon_tk_return,
    cmon_tk_true,
    cmon_tk_false,
    cmon_tk_square_open,
    cmon_tk_square_close,
    cmon_tk_less,
    cmon_tk_greater,
    cmon_tk_less_equal,
    cmon_tk_greater_equal,
    cmon_tk_and,
    cmon_tk_or,
    cmon_tk_for,
    cmon_tk_in,
    cmon_tk_break,
    cmon_tk_none,
    cmon_tk_struct,
    cmon_tk_pub,
    cmon_tk_mut,
    //@TODO: amp redundant with bw_and, remove this if amps only use case is bitwise and
    cmon_tk_amp,
    cmon_tk_as,
    cmon_tk_double_dot,
    cmon_tk_at,
    cmon_tk_noinit,
    cmon_tk_module,
    cmon_tk_import,
    cmon_tk_question,
    cmon_tk_dollar,
    cmon_tk_embed,
    cmon_tk_enum,
    cmon_tk_interface,
    cmon_tk_semicolon,
    cmon_tk_continue,
    cmon_tk_self,
    cmon_tk_type,
    cmon_tk_alias,
    cmon_tk_defer,
    cmon_tk_try,
    cmon_tk_comment,
    cmon_tk_eof
} cmon_token_kind;

typedef struct cmon_tokens cmon_tokens;


CMON_API cmon_tokens * cmon_tokenize(cmon_allocator * _alloc, cmon_src * _src, cmon_idx _src_file_idx, cmon_err_report * _out_err);
CMON_API void cmon_tokens_destroy(cmon_tokens * _t);

CMON_API size_t cmon_tokens_count(cmon_tokens * _t);

// functions to retrieve the index of a token
CMON_API cmon_idx cmon_tokens_prev(cmon_tokens * _t, cmon_bool _skip_comments);
CMON_API cmon_idx cmon_tokens_current(cmon_tokens * _t);
CMON_API cmon_idx cmon_tokens_next(cmon_tokens * _t, cmon_bool _skip_comments);
CMON_API cmon_idx cmon_tokens_advance(cmon_tokens * _t, cmon_bool _skip_comments);

// functions to get information about a token
CMON_API cmon_token_kind cmon_tokens_kind(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_str_view cmon_tokens_str_view(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tokens_line(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tokens_line_offset(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_bool cmon_tokens_follows_nl(cmon_tokens * _t, cmon_idx _idx);

CMON_API cmon_bool cmon_tokens_is_at(cmon_tokens * _t, cmon_token_kind _kind, cmon_idx _idx);
CMON_API cmon_bool cmon_tokens_is_next(cmon_tokens * _t, cmon_token_kind _kind);
CMON_API cmon_bool cmon_tokens_is_current(cmon_tokens * _t, cmon_token_kind _kind);
CMON_API cmon_idx cmon_tokens_accept(cmon_tokens * _t, cmon_token_kind _kind);

#endif // CMON_CMON_LEXER_H
