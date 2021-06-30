#ifndef CMON_CMON_SRC_DIR_H
#define CMON_CMON_SRC_DIR_H

#include <cmon/cmon_modules.h>
#include <cmon/cmon_src.h>

typedef struct cmon_src_dir cmon_src_dir;

//@TODO: As of now, this really does not need to be an object. Maybe simplify by turning it into a
//single dir parsing function?
CMON_API cmon_src_dir * cmon_src_dir_create(cmon_allocator * _alloc,
                                            const char * _path,
                                            cmon_modules * _mods,
                                            cmon_src * _src);
CMON_API void cmon_src_dir_destroy(cmon_src_dir * _dir);
CMON_API cmon_bool cmon_src_dir_parse(cmon_src_dir * _dir, const char * _base_path_prefix);
CMON_API const char * cmon_src_dir_err_msg(cmon_src_dir * _dir);

#endif // CMON_CMON_SRC_DIR_H
