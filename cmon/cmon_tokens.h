#ifndef CMON_CMON_TOKENS_H
#define CMON_CMON_TOKENS_H

#include <cmon/cmon_err_report.h>
#include <cmon/cmon_src.h>
#include <cmon/cmon_util.h>

typedef enum
{
    cmon_tokk_ident,
    cmon_tokk_int,
    cmon_tokk_float,
    cmon_tokk_string,
    cmon_tokk_curl_open,
    cmon_tokk_curl_close,
    cmon_tokk_paran_open,
    cmon_tokk_paran_close,
    cmon_tokk_equals,
    cmon_tokk_not_equals,
    cmon_tokk_assign,
    cmon_tokk_plus_assign,
    cmon_tokk_minus_assign,
    cmon_tokk_mult_assign,
    cmon_tokk_div_assign,
    cmon_tokk_mod_assign,
    cmon_tokk_bw_left_assign,
    cmon_tokk_bw_right_assign,
    cmon_tokk_bw_and_assign,
    cmon_tokk_bw_xor_assign,
    cmon_tokk_bw_or_assign,
    cmon_tokk_plus,
    cmon_tokk_minus,
    cmon_tokk_inc,
    cmon_tokk_dec,
    cmon_tokk_mult,
    cmon_tokk_div,
    cmon_tokk_mod,
    cmon_tokk_bw_left,
    cmon_tokk_bw_right,
    cmon_tokk_bw_and,
    cmon_tokk_bw_xor,
    cmon_tokk_bw_or,
    cmon_tokk_bw_not,
    cmon_tokk_arrow, // ->
    cmon_tokk_fn,
    cmon_tokk_dot,
    cmon_tokk_comma,
    cmon_tokk_colon,
    cmon_tokk_exclam,
    cmon_tokk_if,
    cmon_tokk_else,
    cmon_tokk_return,
    cmon_tokk_true,
    cmon_tokk_false,
    cmon_tokk_square_open,
    cmon_tokk_square_close,
    cmon_tokk_less,
    cmon_tokk_greater,
    cmon_tokk_less_equal,
    cmon_tokk_greater_equal,
    cmon_tokk_and,
    cmon_tokk_or,
    cmon_tokk_for,
    cmon_tokk_in,
    cmon_tokk_break,
    cmon_tokk_none,
    cmon_tokk_struct,
    cmon_tokk_pub,
    cmon_tokk_mut,
    //@TODO: amp redundant with bw_and, remove this if amps only use case is bitwise and
    // cmon_tokk_amp,
    cmon_tokk_as,
    cmon_tokk_double_dot,
    cmon_tokk_at,
    cmon_tokk_noinit,
    cmon_tokk_module,
    cmon_tokk_import,
    cmon_tokk_question,
    cmon_tokk_dollar,
    cmon_tokk_embed,
    cmon_tokk_enum,
    cmon_tokk_interface,
    cmon_tokk_semicolon,
    cmon_tokk_continue,
    cmon_tokk_self,
    cmon_tokk_type,
    cmon_tokk_alias,
    cmon_tokk_defer,
    cmon_tokk_try,
    cmon_tokk_comment,
    cmon_tokk_scoped,
    cmon_tokk_scope_exit,
    cmon_tokk_eof
} cmon_tokk;

typedef struct cmon_tokens cmon_tokens;

CMON_API cmon_tokens * cmon_tokenize(cmon_allocator * _alloc,
                                     cmon_src * _src,
                                     cmon_idx _src_file_idx,
                                     cmon_err_report * _out_err);
CMON_API void cmon_tokens_destroy(cmon_tokens * _t);

CMON_API size_t cmon_tokens_count(cmon_tokens * _t);

// functions to retrieve the index of a token
CMON_API cmon_idx cmon_tokens_prev(cmon_tokens * _t, cmon_bool _skip_comments);
CMON_API cmon_idx cmon_tokens_current(cmon_tokens * _t);
CMON_API cmon_idx cmon_tokens_next(cmon_tokens * _t, cmon_bool _skip_comments);
CMON_API cmon_idx cmon_tokens_advance(cmon_tokens * _t, cmon_bool _skip_comments);

// functions to get information about a token
CMON_API cmon_tokk cmon_tokens_kind(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_str_view cmon_tokens_str_view(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tokens_line(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tokens_line_offset(cmon_tokens * _t, cmon_idx _idx);
CMON_API cmon_bool cmon_tokens_follows_nl(cmon_tokens * _t, cmon_idx _idx);

// implementation functions for variadic token check/accept macros below
CMON_API cmon_bool cmon_tokens_is_impl_v(cmon_tokens * _t, cmon_idx _idx, va_list _args);
CMON_API cmon_bool cmon_tokens_is_impl(cmon_tokens * _t, cmon_idx _idx, ...);
CMON_API cmon_idx cmon_tokens_accept_impl_v(cmon_tokens * _t, va_list _args);
CMON_API cmon_idx cmon_tokens_accept_impl(cmon_tokens * _t, ...);

//retrieve a view of a whole line
CMON_API cmon_str_view cmon_tokens_line_str_view(cmon_tokens * _t, size_t _line);

// token utility functions
CMON_API const char * cmon_tokk_to_str(cmon_tokk _kind);

// variadic macros to check for one or multiple token kinds
#define cmon_tokens_is(_t, _idx, ...)                                                              \
    cmon_tokens_is_impl((_t), _idx, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define cmon_tokens_is_current(_t, ...)                                                            \
    cmon_tokens_is_impl((_t), cmon_tokens_current((_t)), _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define cmon_tokens_is_next(_t, ...)                                                               \
    cmon_tokens_is_impl(                                                                           \
        (_t), cmon_tokens_current((_t)) + 1, _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))
#define cmon_tokens_accept(_t, ...)                                                                \
    cmon_tokens_accept_impl((_t), _CMON_VARARG_APPEND_LAST(-1, __VA_ARGS__))

// helper so we don't need to manually write out all binary expression tokens evey time we want to
// check for that
#define CMON_ASSIGN_TOKS                                                                           \
    cmon_tokk_assign, cmon_tokk_plus_assign, cmon_tokk_minus_assign, cmon_tokk_mult_assign,        \
        cmon_tokk_div_assign, cmon_tokk_mod_assign, cmon_tokk_bw_left_assign,                      \
        cmon_tokk_bw_right_assign, cmon_tokk_bw_and_assign, cmon_tokk_bw_xor_assign,               \
        cmon_tokk_bw_or_assign

#define CMON_BIN_TOKS                                                                              \
    CMON_ASSIGN_TOKS, cmon_tokk_plus, cmon_tokk_minus, cmon_tokk_inc, cmon_tokk_dec,               \
        cmon_tokk_mult, cmon_tokk_div, cmon_tokk_mod, cmon_tokk_bw_left, cmon_tokk_bw_right,       \
        cmon_tokk_bw_and, cmon_tokk_bw_xor, cmon_tokk_bw_or

#endif // CMON_CMON_TOKENS_H
