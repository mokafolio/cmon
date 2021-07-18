#include <cmon/cmon_path.h>

const char * cmon_path_file_ext(const char * _filename, char * _buf, size_t _buf_size)
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

const char * cmon_path_filename(const char * _path, char * _buf, size_t _buf_size)
{
    size_t len = strlen(_path);
    char * c = &_path[len - 1];
    char * last = c;
    while (*c != '/')
    {
        --c;
    }
    assert(last - c < _buf_size);
    memcpy(_buf, c + 1, last - c + 1);
    return _buf;
}

const char * cmon_path_join(const char * _a, const char * _b, char * _buf, size_t _buf_size)
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

cmon_bool cmon_path_is_abs(const char * _path)
{
    if (strlen(_path) && _path[0] == '/')
    {
        return cmon_true;
    }
    return cmon_false;
}