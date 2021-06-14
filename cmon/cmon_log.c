#include <cmon/cmon_fs.h>
#include <cmon/cmon_log.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>

typedef struct cmon_log
{
    cmon_allocator * alloc;
    cmon_str_builder str_builder;
    char path[CMON_PATH_MAX];
} cmon_log;

static void _write_to_file(const char * _path,
                           const char * _prefix,
                           const char * _txt,
                           cmon_err_handler * _eh)
{
    FILE * file = fopen(_path, "a");
    if (file)
    {
        fputs(_prefix, file);
        fputs(_txt, file);
        fclose(file);
    }
    else
    {
        cmon_panic("Could not open cmon log file");
    }
}

cmon_log * cmon_log_create(cmon_allocator * _alloc, const char * _path, cmon_bool _verbose)
{
    cmon_log * ret = CMON_CREATE(_alloc, cmon_log);

    return ret;
}

void cmon_log_destroy(cmon_log * _log)
{
    if (!_log)
        return;
}

void cmon_log_write_v(cmon_log * _log, const char * _fmt, va_list _args)
{
}

void cmon_log_write(cmon_log * _log, const char * _fmt, ...)
{
}
