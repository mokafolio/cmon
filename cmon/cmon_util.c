#include <cmon/cmon_util.h>

cmon_str_view cmon_str_view_make(const char * c_str)
{
    cmon_str_view ret;
    ret.begin = c_str;
    ret.end = c_str + strlen(c_str);
    return ret;
}

cmon_str_view cmon_str_view_make_empty()
{
    static const char * _e = "";
    return cmon_str_view_make(_e);
}

size_t cmon_str_view_len(cmon_str_view _v)
{
    return _v.end - _v.begin;
}

char * cmon_str_view_copy(cmon_allocator * _alloc, cmon_str_view _view)
{
    int len = _view.end - _view.begin;
    // assert(len);
    char * ret = cmon_allocator_alloc(_alloc, len + 1).ptr;
    memcpy(ret, _view.begin, len);
    ret[len] = '\0';
    printf("DA NEW LEN %lu %s\n", len, ret);
    return ret;
}

char * cmon_c_str_copy(cmon_allocator * _alloc, const char * _str)
{
    cmon_str_view view;
    int len = strlen(_str);
    
    // if(!len)
    //     return _str;

    view.begin = _str;
    view.end = _str + len;
    return cmon_str_view_copy(_alloc, view);
}

void cmon_c_str_free(cmon_allocator * _alloc, char * _str)
{
    cmon_allocator_free(_alloc, (cmon_mem_blk){_str, strlen(_str) + 1});
}

const char * cmon_str_view_tmp_str(cmon_str_view _view, char * _tmp)
{
    int len = _view.end - _view.begin;
    // assert(len < CMON_TMP_STR_BUFFER_SIZE - 1);
    memcpy(_tmp, _view.begin, len);
    _tmp[len] = '\0';
    return _tmp;
}

int cmon_str_view_cmp(cmon_str_view _a, cmon_str_view _b)
{
    return strncmp(_a.begin, _b.begin, CMON_MAX(_a.end - _a.begin, _b.end - _b.begin));
}

int cmon_str_view_c_str_cmp(cmon_str_view _a, const char * _str)
{
    return cmon_str_view_cmp(_a, cmon_str_view_make(_str));
}

char * cmon_c_str_append(char * _dst, const char * _src)
{
    return cmon_c_str_append_string_view(_dst, cmon_str_view_make(_src));
}

char * cmon_c_str_append_string_view(char * _dst, cmon_str_view _src)
{
    size_t len = _src.end - _src.begin;
    memcpy(_dst, _src.begin, len);
    _dst[len] = '\0';
    return _dst + len;
}

char * cmon_str_create_v(cmon_allocator * _alloc, const char * _fmt, va_list _argp)
{
    int len;
    char * ret;
    va_list cpy;
    va_copy(cpy, _argp);
    len = vsnprintf(NULL, 0, _fmt, _argp);
    ret = cmon_allocator_alloc(_alloc, len + 1).ptr;
    len = vsnprintf(ret, len + 1, _fmt, cpy);
    //@TODO: Proper error/panic
    assert(len != -1);
    return ret;
}

char * cmon_str_create(cmon_allocator * _alloc, const char * _fmt, ...)
{
    char * ret;
    va_list args;
    va_start(args, _fmt);
    ret = cmon_str_create_v(_alloc, _fmt, args);
    va_end(args);
    return ret;
}

const char * cmon_file_ext(const char * _filename, char * _buf, size_t _buf_size)
{
    size_t i;
    size_t len = strlen(_filename);
    i = len - 1;
    for (; i > 0; --i)
    {
        if (_filename[i] == '.')
        {
            strncpy(_buf, _filename + i, _buf_size);
            return _buf;
        }
    }
    return NULL;
}

const char * cmon_join_paths(const char * _a, const char * _b, char * _buf, size_t _buf_size)
{
    size_t alen = strlen(_a);
    size_t blen = strlen(_b);
    size_t off = alen;

    //+2 for potential "/" and null terminator
    if (alen + blen + 2 > _buf_size)
        return NULL;

    memcpy(_buf, _a, alen);
    if (_a[alen - 1] != '/')
    {
        memcpy(_buf + off, "/", sizeof(char));
        ++off;
    }

    memcpy(_buf + off, _b, blen);
    off += blen;
    _buf[off] = '\0';
    return _buf;
}

int cmon_exec(const char * _cmd)
{
    FILE * pipe = popen(_cmd, "r");
    if (!pipe)
        goto err;

    pclose(pipe);
    return 0;

err:
    pclose(pipe);
    return -1;
}
