#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_dyn_arr.h>
#include <stdarg.h>

typedef struct cmon_str_builder
{
    cmon_dyn_arr(char) buf;
} cmon_str_builder;

cmon_str_builder * cmon_str_builder_create(cmon_allocator * _alloc, size_t _cap)
{
    cmon_str_builder * ret = CMON_CREATE(_alloc, cmon_str_builder);
    cmon_dyn_arr_init(&ret->buf, _alloc, _cap);
    cmon_str_builder_clear(ret);
    return ret;
}

void cmon_str_builder_destroy(cmon_str_builder * _s)
{
    if(!_s)
        return;

    cmon_allocator * a = _cmon_dyn_arr_md(&_s->buf)->alloc;
    cmon_dyn_arr_dealloc(&_s->buf);
    CMON_DESTROY(a, _s);
}

void cmon_str_builder_clear(cmon_str_builder * _s)
{
    cmon_dyn_arr_clear(&_s->buf);
    cmon_dyn_arr_append(&_s->buf, '\0');
}

void cmon_str_builder_append_fmt_v(cmon_str_builder * _s, const char * _fmt, va_list _args)
{
    va_list argscpy;
    int len;
    size_t off;
    va_copy(argscpy, _args);
    len = vsnprintf(NULL, 0, _fmt, _args);
    off = cmon_dyn_arr_count(&_s->buf) - 1;
    cmon_dyn_arr_resize(&_s->buf, cmon_dyn_arr_count(&_s->buf) + len);
    len = vsnprintf(_s->buf + off, len + 1, _fmt, argscpy);
    //@TODO: replace with panic
    assert(len != -1);
}

void cmon_str_builder_append_fmt(cmon_str_builder * _s, const char * _fmt, ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_str_builder_append_fmt_v(_s, _fmt, args);
    va_end(args);
}

void cmon_str_builder_append(cmon_str_builder * _s, const char * _str)
{
    size_t len, off;
    //should always contain at least zero terminator
    assert(cmon_dyn_arr_count(&_s->buf) > 0);
    len = strlen(_str);
    off = cmon_dyn_arr_count(&_s->buf) - 1;
    cmon_dyn_arr_resize(&_s->buf, cmon_dyn_arr_count(&_s->buf) + len);
    memcpy(_s->buf + off, _str, len + 1);
}

const char * cmon_str_builder_c_str(cmon_str_builder * _s)
{
    return &_s->buf[0];
}
