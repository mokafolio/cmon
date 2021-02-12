#ifndef CMON_CMON_LEXER_H
#define CMON_CMON_LEXER_H

#include <cmon/cmon_src.h>

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

// typedef struct
// {
//     // cmon_src_file * src_file;
//     size_t line;
//     size_t line_end;
//     size_t line_off;
//     cmon_str_view str_view;
//     //true if the token is preceeded by a new line
//     cmon_bool follows_nl;
// } cmon_token;

typedef struct cmon_lexer cmon_lexer;

CMON_API cmon_lexer * cmon_lexer_create(cmon_allocator * _alloc);
CMON_API void cmon_lexer_destroy(cmon_lexer * _l);

// tokenizer does not copy the input, the input string needs to outlive tokenizer.
CMON_API void cmon_lexer_set_input(cmon_lexer * _l,
                                   cmon_src * _src,
                                   cmon_idx _src_file_idx);
CMON_API cmon_bool cmon_lexer_tokenize(cmon_lexer * _l);

// functions to retrieve the index of a token
CMON_API cmon_idx cmon_lexer_prev(cmon_lexer * _l);
CMON_API cmon_idx cmon_lexer_current(cmon_lexer * _l);
CMON_API cmon_idx cmon_lexer_next(cmon_lexer * _l);
CMON_API cmon_idx cmon_lexer_advance(cmon_lexer * _l);

// functions to get information about a token
CMON_API cmon_token_kind cmon_lexer_token_kind(cmon_lexer * _l, cmon_idx _idx);
CMON_API cmon_str_view cmon_lexer_str_view(cmon_lexer * _l, cmon_idx _idx);
CMON_API cmon_idx cmon_lexer_line(cmon_lexer * _l, cmon_idx _idx);
CMON_API cmon_idx cmon_lexer_line_offset(cmon_lexer * _l, cmon_idx _idx);
CMON_API cmon_bool cmon_lexer_follows_nl(cmon_lexer * _l, cmon_idx _idx);

CMON_API cmon_bool cmon_lexer_is_at(cmon_lexer * _l, cmon_token_kind _kind, cmon_idx _idx);
CMON_API cmon_bool cmon_lexer_is_next(cmon_lexer * _l, cmon_token_kind _kind);
CMON_API cmon_bool cmon_lexer_is_current(cmon_lexer * _l, cmon_token_kind _kind);
CMON_API cmon_idx cmon_lexer_accept(cmon_lexer * _l, cmon_token_kind _kind);

#endif // CMON_CMON_LEXER_H
