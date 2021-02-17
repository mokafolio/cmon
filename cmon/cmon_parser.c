#include <cmon/cmon_err_report.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_str_builder.h>
#include <setjmp.h>

// for now the parser stops on the first error. we simpy save the error and jump out of parsing.
#define _err(_p, _tok, _msg, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        cmon_str_builder_clear(&_p->str_builder);                                                  \
        cmon_str_builder_append_fmt(&_p->str_builder, _msg, ##__VA_ARGS__);                        \
        _p->err = cmon_err_report_make(_p->alloc,                                                  \
                                       cmon_src_filename(_l->src, _l->src_file_idx),               \
                                       _tok->line,                                                 \
                                       _tok->line_off,                                             \
                                       cmon_str_builder_c_str(&_p->str_builder));                  \
        longjmp(_p->err_jmp, 1);                                                                   \
    } while (0)

// typedef struct cmon_parser
// {
//     //@TODO: move everything implementation specific into a pimpl
//     cmon_allocator * alloc;
//     cmon_src_file * file;
//     cmon_tokenizer * tokenizer;
//     cmon_ast_file * ast_file;
//     // the top level statements of the parsed file
//     cmon_dyn_arr(cmon_ast_stmt *) root_stmts;
//     // set by the top module statement (if any)
//     cmon_ast_stmt * mod_stmt;
//     cmon_dyn_arr(cmon_ast_stmt *) imports;
//     // while only top level functions are supported at this point, once we want to have anonymous
//     // functions, this will come in handy. Right now we could just use a ptr to the current
//     // function.
//     cmon_dyn_arr(cmon_ast_expr_fn_decl *) fn_stack;
//     cmon_dyn_arr(cmon_ast_stmt*) for_stack;
//     // these arrays mainly exist for easy cleanup
//     cmon_dyn_arr(cmon_parsed_type *) parsed_types;
//     cmon_dyn_arr(cmon_ast_expr *) exprs;
//     cmon_dyn_arr(cmon_ast_stmt *) stmts;
//     // helper to build tmp strings
//     cmon_str_builder tmp_builder;
//     cmon_err_report err;
//     jmp_buf err_jmp;
// } cmon_parser;

typedef struct cmon_parser
{
    cmon_allocator * alloc;
    cmon_astb * ast_builder;
    cmon_str_builder * str_builder;
    cmon_tokens * tokens;
    cmon_err_report err;
    jmp_buf err_jmp;
} cmon_parser;

static inline cmon_idx _token_check(cmon_parser * _p,
                                    cmon_token_kind _kind,
                                    cmon_bool _allow_line_break)
{
    // cmon_token * tok = cmon_tokens_accept(_p->tokens, _kind, _allow_line_break);
    // if (!tok)
    // {
    //     cmon_token * cur = cmon_token_current(_p->tokenizer);
    //     if (!cmon_token_kind_is(cur->kind, cmon_tk_eof))
    //     {
    //         _err(_p,
    //              cur,
    //              "%s expected, got '%.*s'",
    //              cmon_tok_kind_to_str(_type_mask),
    //              cur->str_view.end - cur->str_view.begin,
    //              cur->str_view.begin);
    //     }
    //     else
    //     {
    //         _err(_p,
    //              cur,
    //              "%s expected, got 'EOF'",
    //              cmon_tok_kind_to_str(_type_mask));
    //     }
    // }
    // return tok;
}

static cmon_idx _parse_expr(cmon_parser * _p)
{
}

static cmon_idx _parse_block(cmon_parser * _p)
{
    // if (_token_check(_p, cmon_tk_curl_open, cmon_true))
    // {
    //     while (!cmon_token_is_current(
    //         _p->tokenizer, CMON_MSK(cmon_tk_curl_close, cmon_tk_eof), cmon_true))
    //     {
    //         cmon_ast_stmt * s = _parse_single_stmt(_p);
    //         if (s)
    //         {
    //             cmon_dyn_arr_append(ret->data.block.body_stmts, s);
    //             if (s->kind == cmon_ast_stmt_kind_defer)
    //                 cmon_dyn_arr_append(ret->data.block.defer_stmts, s);
    //         }
    //     }
    //     _token_check(_p, cmon_tk_curl_close, cmon_true);
    // }
}

static cmon_idx _parse_stmt(cmon_parser * _p)
{
}

static cmon_idx _parse_top_lvl_stmt(cmon_parser * _p)
{
}

cmon_parser * cmon_parser_create(cmon_allocator * _alloc)
{
    cmon_parser * ret = CMON_CREATE(_alloc, cmon_parser);
    ret->alloc = _alloc;
    ret->ast_builder = cmon_astb_create(_alloc);
    ret->str_builder = cmon_str_builder_create(_alloc, 256);
    ret->err = cmon_err_report_make_empty();
    return ret;
}

void cmon_parser_destroy(cmon_parser * _p)
{
    if (!_p)
        return;

    cmon_str_builder_destroy(_p->str_builder);
    cmon_astb_destroy(_p->ast_builder);
}

cmon_err_report cmon_parser_err(cmon_parser * _p)
{
    return _p->err;
}

cmon_ast * cmon_parser_parse(cmon_parser * _p,
                             cmon_src * _src,
                             cmon_idx _src_file_idx,
                             cmon_tokens * _tokens)
{
}
