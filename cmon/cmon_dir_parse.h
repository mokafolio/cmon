#ifndef CMON_CMON_DIR_PARSE_H
#define CMON_CMON_DIR_PARSE_H

#include <cmon/cmon_modules.h>
#include <cmon/cmon_src.h>

CMON_API cmon_bool cmon_dir_parse_src(cmon_allocator * _alloc,
                                      const char * _path,
                                      cmon_modules * _mods,
                                      cmon_src * _src,
                                      const char * _base_path_prefix,
                                      char * _err_msg_buf,
                                      size_t _buf_size);

CMON_API cmon_bool cmon_dir_parse_deps(cmon_allocator * _alloc,
                                       const char * _path,
                                       cmon_modules * _mods,
                                       cmon_src * _src,
                                       char * _err_msg_buf,
                                       size_t _buf_size);

#endif // CMON_CMON_DIR_PARSE_H
