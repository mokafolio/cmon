#include <cmon/cmon_fs.h>
#include <cmon/cmon_log.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>
#include <time.h>

//@TODO: Put these into a separate header for reusability and since there will eventually have to be
//a portable version for windows
#define COL_RESET "\x1B[0m"
#define COL_CYAN "\x1B[36m"
#define COL_RED "\x1B[31m"
#define COL_MAGENTA "\x1B[95m"
#define COL_BOLD "\x1B[1m"

typedef struct cmon_log
{
    cmon_allocator * alloc;
    cmon_str_builder * str_builder;
    char name[CMON_FILENAME_MAX];
    char path[CMON_PATH_MAX];
    FILE * file;
    size_t writes_since_last_flush;
    size_t flush_every_n;
    cmon_bool verbose;
} cmon_log;

static inline void _write_to_file(cmon_log * _l, const char * _prefix, const char * _txt)
{
    fputs(_prefix, _l->file);
    fputs(_txt, _l->file);
    if (ferror(_l->file) != 0)
    {
        cmon_panic("failed to write to log file at %s", _l->path);
    }

    if(_l->writes_since_last_flush == _l->flush_every_n)
    {
        if(fflush(_l->file) != 0)
        {
            cmon_panic("failed to flush log file at %s", _l->path);
        }
        _l->writes_since_last_flush = 0;
    }
}

static inline const char * _timestamp(char * _buf, size_t _buf_size)
{
    time_t now;
    struct tm ts;
    time(&now);
    ts = *localtime(&now);
    strftime(_buf, _buf_size, "%a %m/%d/%Y", &ts);
    return _buf;
}

#define _tstamp(_buf) _timestamp((_buf), sizeof((_buf)))

cmon_log * cmon_log_create(cmon_allocator * _alloc, const char * _name, const char * _path, cmon_bool _verbose)
{
    cmon_log * ret = CMON_CREATE(_alloc, cmon_log);
    ret->str_builder = cmon_str_builder_create(_alloc, 512);
    ret->alloc = _alloc;
    strcpy(ret->name, _name);
    cmon_join_paths(_path, _name, ret->path, sizeof(ret->path));
    ret->file = fopen(ret->path, "w");
    if (!ret->file)
    {
        cmon_panic("could not open log file at %s", _path);
    }
    ret->verbose = _verbose;
    ret->flush_every_n = 4; //@TODO: make this customizable?
    ret->writes_since_last_flush = 0;

    char tbuf[128];
    cmon_log_write(ret, "log '%s', started at %s\n", _name, _tstamp(tbuf));

    return ret;
}

void cmon_log_destroy(cmon_log * _log)
{
    if (!_log)
        return;

    fclose(_log->file);
    cmon_str_builder_destroy(_log->str_builder);
    CMON_DESTROY(_log->alloc, _log);
}

void cmon_log_write_v(cmon_log * _log, const char * _fmt, va_list _args)
{

}

void cmon_log_write(cmon_log * _log, const char * _fmt, ...)
{
}
