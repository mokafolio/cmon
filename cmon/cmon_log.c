#include <cmon/cmon_fs.h>
#include <cmon/cmon_log.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>
#include <time.h>

//@TODO: Put these into a separate header for reusability and since there will eventually have to be
// a portable version for windows
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
    fputs("\n", _l->file);
    if (ferror(_l->file) != 0)
    {
        cmon_panic("failed to write to log file at %s", _l->path);
    }

    if (_l->writes_since_last_flush == _l->flush_every_n)
    {
        if (fflush(_l->file) != 0)
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

cmon_log * cmon_log_create(cmon_allocator * _alloc,
                           const char * _name,
                           const char * _path,
                           cmon_bool _verbose)
{
    cmon_log * ret = CMON_CREATE(_alloc, cmon_log);
    ret->str_builder = cmon_str_builder_create(_alloc, 512);
    ret->alloc = _alloc;
    strcpy(ret->name, _name);
    cmon_join_paths(_path, _name, ret->path, sizeof(ret->path));
    printf("path %s\n", ret->path);
    ret->file = fopen(ret->path, "w");
    if (!ret->file)
    {
        cmon_panic("could not open log file at %s", _path);
    }
    ret->verbose = _verbose;
    ret->flush_every_n = 4; //@TODO: make this customizable?
    ret->writes_since_last_flush = 0;

    char tbuf[128];
    cmon_log_write(ret, "log '%s', started at %s", _name, _tstamp(tbuf));

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
    cmon_str_builder_clear(_log->str_builder);
    cmon_str_builder_append_fmt_v(_log->str_builder, _fmt, _args);
    _write_to_file(_log, "", cmon_str_builder_c_str(_log->str_builder));
    if (_log->verbose)
    {
        printf("%s\n", cmon_str_builder_c_str(_log->str_builder));
    }
}

void cmon_log_write(cmon_log * _log, const char * _fmt, ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_log_write_v(_log, _fmt, args);
    va_end(args);
}

void cmon_log_write_err_report(cmon_log * _log, cmon_err_report * _err)
{
    cmon_str_builder_clear(_log->str_builder);
    cmon_str_builder_append_fmt(_log->str_builder,
                                "error: %s:%lu:%lu: %s",
                                _err->filename,
                                _err->line,
                                _err->line_offset,
                                _err->msg);
    _write_to_file(_log, "", cmon_str_builder_c_str(_log->str_builder));
    if (_log->verbose)
    {
        printf("%s%serror:%s %s:%lu:%lu: %s\n%*.s^\n",
               COL_RED,
               COL_BOLD,
               COL_RESET,
               _err->filename,
               _err->line,
               _err->line_offset,
               _err->msg,
               _err->line_offset);
    }
}
