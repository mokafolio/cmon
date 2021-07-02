#ifndef CMON_CMON_MODULES_H
#define CMON_CMON_MODULES_H

#include <cmon/cmon_src.h>

typedef struct cmon_modules cmon_modules;
typedef struct cmon_resolver cmon_resolver;

CMON_API cmon_modules * cmon_modules_create(cmon_allocator * _a, cmon_src * _src);
CMON_API void cmon_modules_destroy(cmon_modules * _m);
CMON_API cmon_idx cmon_modules_add(cmon_modules * _m, const char * _path, const char * _name);
CMON_API void cmon_modules_add_src_file(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _src_file);
CMON_API void cmon_modules_add_dep(cmon_modules * _m,
                                   cmon_idx _mod_idx,
                                   cmon_idx _mod_dep_idx,
                                   cmon_idx _src_file_idx,
                                   cmon_idx _import_tok_idx);
CMON_API void cmon_modules_set_global_scope(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _scope);
CMON_API void cmon_modules_set_resolver(cmon_modules * _m, cmon_idx _mod_idx, cmon_resolver * _r);
CMON_API cmon_resolver * cmon_modules_resolver(cmon_modules * _m, cmon_idx _mod_idx);

CMON_API cmon_idx cmon_modules_find(cmon_modules * _m, cmon_str_view _path);
CMON_API cmon_idx cmon_modules_find_import(cmon_modules * _m, cmon_idx _looking_mod_idx, const char * _path);
CMON_API void cmon_modules_add_search_prefix(cmon_modules * _m, cmon_idx _mod_idx, const char * _path);
CMON_API void cmon_modules_add_path_overwrite(cmon_modules * _m, cmon_idx _mod_idx, const char * _path, const char * _overwrite);
CMON_API size_t cmon_modules_count(cmon_modules * _m);
CMON_API const char * cmon_modules_path(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API const char * cmon_modules_name(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API const char * cmon_modules_prefix(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API size_t cmon_modules_path_token_count(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API cmon_str_view cmon_modules_path_token(cmon_modules * _m, cmon_idx _mod_idx, size_t _tok_idx);
CMON_API cmon_idx cmon_modules_global_scope(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API cmon_idx cmon_modules_src_file(cmon_modules * _m,
                                        cmon_idx _mod_idx,
                                        cmon_idx _src_file_idx);
CMON_API size_t cmon_modules_src_file_count(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API cmon_idx cmon_modules_find_dep_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_mod_idx);
CMON_API cmon_idx cmon_modules_dep_mod_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_idx);
CMON_API cmon_idx cmon_modules_dep_tok_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_idx);
CMON_API cmon_idx cmon_modules_dep_src_file_idx(cmon_modules * _m, cmon_idx _mod_idx, cmon_idx _dep_idx);
CMON_API size_t cmon_modules_dep_count(cmon_modules * _m, cmon_idx _mod_idx);

#endif // CMON_CMON_MODULES_H
