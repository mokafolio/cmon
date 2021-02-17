#ifndef CMON_CMON_PARSER_H
#define CMON_CMON_PARSER_H

#include <cmon/cmon_ast.h>
#include <cmon/cmon_tokens.h>

typedef struct cmon_parser cmon_parser;

CMON_API cmon_parser * cmon_parser_create(cmon_allocator * _alloc);
CMON_API void cmon_parser_destroy(cmon_parser * _p);
CMON_API cmon_err_report cmon_parser_err(cmon_parser * _p);
CMON_API cmon_ast * cmon_parser_parse(cmon_parser * _p, cmon_src * _src, cmon_idx _src_file_idx, cmon_tokens * _tokens);

#endif // CMON_CMON_PARSER_H
