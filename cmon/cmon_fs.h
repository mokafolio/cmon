#ifndef CMON_CMON_FILE_SYS_H
#define CMON_CMON_FILE_SYS_H

#include <cmon/cmon_allocator.h>
//@TODO: Windows version & possible other platforms
#include <dirent.h>
#include <sys/stat.h>

typedef enum CMON_API
{
    cmon_fs_dirent_dir,
    cmon_fs_dirent_file,
    cmon_fs_dirent_symlink,
    cmon_fs_dirent_other
} cmon_fs_dirent_type;

typedef struct CMON_API
{
    char path[CMON_PATH_MAX];
    char name[CMON_FILENAME_MAX];
    cmon_fs_dirent_type type;
    struct stat _native_stat;
} cmon_fs_dirent;

typedef struct CMON_API
{
    char path[CMON_PATH_MAX];
    int path_len;
    DIR * _native_dir;
    struct dirent * _native_dirent;
} cmon_fs_dir;

typedef struct CMON_API
{
    struct timespec _native_time;
} cmon_fs_timestamp;

//@TODO: sort this better
CMON_API int cmon_fs_open(const char * _path, cmon_fs_dir * _dir);
CMON_API int cmon_fs_close(cmon_fs_dir * _dir);
CMON_API cmon_bool cmon_fs_has_next(cmon_fs_dir * _dir);
CMON_API int cmon_fs_next(cmon_fs_dir * _dir, cmon_fs_dirent * _dirent);
CMON_API int cmon_fs_chdir(const char * _dir);
CMON_API const char * cmon_fs_getcwd(char * _buf, size_t _len);
CMON_API char * cmon_fs_load_txt_file(cmon_allocator * _alloc, const char * _path);
//@TODO better error handling/messaging for this one specifically but most likely all of these functions
CMON_API int cmon_fs_write_txt_file(const char * _path, const char * _txt);
CMON_API int cmon_fs_mkdir(const char * _path);
CMON_API int cmon_fs_remove(const char * _path);
CMON_API int cmon_fs_remove_all(const char * _path);

CMON_API cmon_bool cmon_fs_exists(const char * _path);
CMON_API cmon_bool cmon_fs_is_dir(const char * _path);
CMON_API int cmon_fs_last_chage_time(const char * _path, cmon_fs_timestamp * _result);
CMON_API int cmon_fs_timestamp_cmp(cmon_fs_timestamp * _a, cmon_fs_timestamp * _b);

#endif //CMON_CMON_FILE_SYS_H
