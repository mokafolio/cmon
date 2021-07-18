#include <cmon/cmon_fs.h>
#include <errno.h>
#include <unistd.h>

static int _advance(cmon_fs_dir * _dir)
{
    assert(_dir && _dir->_native_dir);
    _dir->_native_dirent = readdir(_dir->_native_dir);
    if (!_dir->_native_dirent)
    {
        if (errno != 0)
            return -1;
    }

    return 0;
}

int cmon_fs_open(const char * _path, cmon_fs_dir * _dir)
{
    assert(_dir);
    _dir->_native_dirent = NULL;
    _dir->path_len = 0;

    _dir->_native_dir = opendir(_path);
    if (!_dir->_native_dir)
        goto err;

    int len = strlen(_path);
    assert(len < CMON_PATH_MAX);

    memcpy(_dir->path, _path, len);
    _dir->path_len = len;

    if (_advance(_dir) == -1)
        goto err;

    return 0;
err:
    cmon_fs_close(_dir);
    return -1;
}

cmon_bool cmon_fs_has_next(cmon_fs_dir * _dir)
{
    return _dir->_native_dirent != NULL;
}

int cmon_fs_close(cmon_fs_dir * _dir)
{
    int ret = 0;
    if (!_dir)
        return 0;

    memset(_dir->path, 0, sizeof(_dir->path));
    _dir->_native_dirent = NULL;
    if (closedir(_dir->_native_dir) == -1)
        ret = -1;

    _dir->_native_dir = NULL;
    return ret;
}

int cmon_fs_next(cmon_fs_dir * _dir, cmon_fs_dirent * _dirent)
{
    assert(_dirent);
    assert(_dir->_native_dirent);

    int name_len = strlen(_dir->_native_dirent->d_name);
    assert(name_len);
    assert(_dir->path_len + name_len < CMON_PATH_MAX);

    int off = _dir->path_len;
    memset(_dirent->path, 0, sizeof(_dirent->path));
    memset(_dirent->name, 0, sizeof(_dirent->name));
    memcpy(_dirent->path, _dir->path, off);
    if (_dir->path[_dir->path_len - 1] != '/')
    {
        static const char * _slash = "/";
        memcpy(_dirent->path + off, _slash, 1);
        ++off;
    }
    memcpy(_dirent->path + off, _dir->_native_dirent->d_name, name_len);
    memcpy(_dirent->name, _dir->_native_dirent->d_name, name_len);

    if (stat(_dirent->path, &_dirent->_native_stat) == -1)
        return -1;

    if ((_dirent->_native_stat.st_mode & S_IFMT) == S_IFDIR)
        _dirent->type = cmon_fs_dirent_dir;
    else if ((_dirent->_native_stat.st_mode & S_IFMT) == S_IFREG)
        _dirent->type = cmon_fs_dirent_file;
    else if ((_dirent->_native_stat.st_mode & S_IFMT) == S_IFLNK)
        _dirent->type = cmon_fs_dirent_symlink;
    else
        _dirent->type = cmon_fs_dirent_other;

    if (_advance(_dir) == -1)
        return -1;

    return 0;
}

int cmon_fs_chdir(const char * _dir)
{
    return chdir(_dir);
}

const char * cmon_fs_getcwd(char * _buf, size_t _len)
{
    return getcwd(_buf, _len);
}

cmon_bool cmon_fs_exists(const char * _path)
{
    struct stat _stat;
    return stat(_path, &_stat) != -1;
}

cmon_bool cmon_fs_is_dir(const char * _path)
{
    struct stat _stat;
    if (stat(_path, &_stat) == 0 && ((_stat.st_mode & S_IFMT) == S_IFDIR))
        return cmon_true;
    return cmon_false;
}

char * cmon_fs_load_txt_file(cmon_allocator * _alloc, const char * _path)
{
    int64_t len, read_len;
    cmon_mem_blk blk = {NULL};
    FILE * fp = NULL;

    fp = fopen(_path, "r");
    if (!fp)
    {
        goto err;
    }

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        goto err;
    }

    len = ftell(fp);
    if (len == -1)
    {
        goto err;
    }

    if (fseek(fp, 0, SEEK_SET) == -1)
    {
        goto err;
    }

    blk = cmon_allocator_alloc(_alloc, len + 1);
    memset(blk.ptr, 0, len + 1);
    read_len = fread(blk.ptr, sizeof(char), len, fp);

    if (read_len != len)
    {
        goto err;
    }

    fclose(fp);
    return blk.ptr;

err:
    cmon_allocator_free(_alloc, blk);
    if(fp)
    {
        fclose(fp);
    }
    return NULL;
}

int cmon_fs_mkdir(const char * _path)
{
    return mkdir(_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int cmon_fs_remove(const char * _path)
{
    if (cmon_fs_is_dir(_path))
    {
        return rmdir(_path);
    }

    return unlink(_path);
}

int cmon_fs_remove_all(const char * _path)
{
    if (cmon_fs_is_dir(_path))
    {
        cmon_fs_dir d;
        cmon_fs_dirent dent;

        if (cmon_fs_open(_path, &d) == -1)
            return -1;

        while (cmon_fs_has_next(&d))
        {
            if (cmon_fs_next(&d, &dent) == -1)
            {
                cmon_fs_close(&d);
                return -1;
            }

            if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0)
                continue;

            cmon_fs_remove_all(dent.path);
        }

        cmon_fs_close(&d);
    }

    return cmon_fs_remove(_path);
}

int cmon_fs_write_txt_file(const char * _path, const char * _txt)
{
    size_t len;
    FILE * fp = fopen(_path, "w");
    if (!fp)
        goto err;

    len = strlen(_txt);
    if (fwrite(_txt, sizeof(char), len, fp) != len)
    {
        goto err;
    }

    fclose(fp);
    return 0;

err:
    if(fp)
    {
        fclose(fp);
    }
    return -1;
}

int cmon_fs_last_chage_time(const char * _path, cmon_fs_timestamp * _out)
{
    struct stat _stat;
    if (stat(_path, &_stat) == 0)
    {
        _out->_native_time = _stat.st_mtim;
        return 0;
    }
    return -1;
}

int cmon_fs_timestamp_cmp(cmon_fs_timestamp * _a, cmon_fs_timestamp * _b)
{
    if (_a->_native_time.tv_sec < _b->_native_time.tv_sec)
    {
        return -1;
    }
    if (_a->_native_time.tv_sec > _b->_native_time.tv_sec)
    {
        return 1;
    }
    return _a->_native_time.tv_nsec - _b->_native_time.tv_nsec;
}
