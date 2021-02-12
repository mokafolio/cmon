#ifndef CMON_CMON_PARSER_H
#define CMON_CMON_PARSER_H

#include <cmon/cmon_ast.h>
#include <cmon/cmon_lexer.h>

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

typedef struct cmon_parser cmon_parser;

CMON_API cmon_parser * cmon_parser_create(cmon_parser * _p,
                               cmon_allocator * _alloc,
                               cmon_src_file * _file,
                               cmon_tokenizer * _tok);
CMON_API void cmon_parser_dealloc(cmon_parser * _p);
CMON_API cmon_err_report cmon_parser_copy_err(cmon_parser * _p);
CMON_API cmon_ast_file * cmon_parser_parse(cmon_parser * _p);

#endif // CMON_CMON_PARSER_H
