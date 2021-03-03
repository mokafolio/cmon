#ifndef CMON_CMON_MODULES_H
#define CMON_CMON_MODULES_H

#include <cmon/cmon_src.h>

typedef struct cmon_modules cmon_modules;

CMON_API cmon_modules * cmon_modules_create(cmon_allocator * _a, cmon_src * _src);
CMON_API void cmon_modules_destroy(cmon_modules * _m);
CMON_API cmon_idx cmon_modules_add(cmon_modules * _m, const char * _path, const char * _name);
CMON_API void cmon_modules_add_src_file(cmon_modules * _m,
                                        cmon_idx _mod_idx,
                                        cmon_idx _src_file);

CMON_API const char * cmon_modules_path(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API const char * cmon_modules_name(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API const char * cmon_modules_prefix(cmon_modules * _m, cmon_idx _mod_idx);
CMON_API cmon_idx cmon_modules_src_file(cmon_modules * _m,
                                        cmon_idx _mod_idx,
                                        cmon_idx _src_file_idx);
CMON_API size_t cmon_modules_src_file_count(cmon_modules * _m, cmon_idx _mod_idx);

#endif // CMON_CMON_MODULES_H
