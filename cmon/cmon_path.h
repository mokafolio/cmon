#ifndef CMON_CMON_PATH_H
#define CMON_CMON_PATH_H

#include <cmon/cmon_base.h>

CMON_API const char * cmon_path_file_ext(const char * _filename, char * _buf, size_t _buf_size);
CMON_API const char * cmon_path_filename(const char * _path, char * _buf, size_t _buf_size);
CMON_API const char * cmon_path_join(const char * _a, const char * _b, char * _buf, size_t _buf_size);
CMON_API cmon_bool cmon_path_is_abs(const char * _path);

#endif //CMON_CMON_PATH_H