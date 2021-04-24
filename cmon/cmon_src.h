#ifndef CMON_CMON_SRC_H
#define CMON_CMON_SRC_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_ast cmon_ast;
typedef struct cmon_tokens cmon_tokens;
typedef struct cmon_src cmon_src;

CMON_API cmon_src * cmon_src_create(cmon_allocator * _alloc);
CMON_API void cmon_src_destroy(cmon_src * _src);

CMON_API cmon_idx cmon_src_add(cmon_src * _src, const char * _path, const char * _filename);
CMON_API cmon_bool cmon_src_load_code(cmon_src * _src, cmon_idx _file_idx);
CMON_API void cmon_src_set_code(cmon_src * _src, cmon_idx _file_idx, const char * _code);
CMON_API void cmon_src_set_ast(cmon_src * _src, cmon_idx _file_idx, cmon_ast * _ast);
CMON_API cmon_ast * cmon_src_ast(cmon_src * _src, cmon_idx _file_idx);
CMON_API void cmon_src_set_tokens(cmon_src * _src, cmon_idx _file_idx, cmon_tokens * _tokens);
CMON_API void cmon_src_set_mod_src_idx(cmon_src * _src, cmon_idx _file_idx, cmon_idx _idx);
CMON_API cmon_tokens * cmon_src_tokens(cmon_src * _src, cmon_idx _file_idx);
CMON_API const char * cmon_src_path(cmon_src * _src, cmon_idx _file_idx);
CMON_API const char * cmon_src_filename(cmon_src * _src, cmon_idx _file_idx);
CMON_API const char * cmon_src_code(cmon_src * _src, cmon_idx _file_idx);
CMON_API cmon_idx cmon_src_mod_src_idx(cmon_src * _src, cmon_idx _file_idx);
CMON_API size_t cmon_src_count(cmon_src * _src);

#endif //CMON_CMON_SRC_H
