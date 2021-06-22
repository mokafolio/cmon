#include <cmon/cmon_fs.h>
#include <cmon/cmon_src_dir.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>

typedef struct cmon_src_dir
{
    cmon_allocator * alloc;
    char path[CMON_PATH_MAX];
    cmon_str_builder * str_builder;
    cmon_modules * mods;
    cmon_src * src;
    char err_msg[CMON_ERR_MSG_MAX];
} cmon_src_dir;

cmon_src_dir * cmon_src_dir_create(cmon_allocator * _alloc,
                                   const char * _path,
                                   cmon_modules * _mods,
                                   cmon_src * _src)
{
    cmon_src_dir * ret = CMON_CREATE(_alloc, cmon_src_dir);
    ret->alloc = _alloc;
    // memset(ret->path, 0, sizeof(ret->path));
    strcpy(ret->path, _path);
    ret->str_builder = cmon_str_builder_create(_alloc, CMON_PATH_MAX);
    ret->mods = _mods;
    ret->src = _src;
    memset(ret->err_msg, 0, sizeof(ret->err_msg));
    return ret;
}

void cmon_src_dir_destroy(cmon_src_dir * _dir)
{
    if(!_dir)
        return;
    cmon_str_builder_destroy(_dir->str_builder);
    CMON_DESTROY(_dir->alloc, _dir);
}

#define _err(_dir, _goto, _msg)                                                                    \
    do                                                                                             \
    {                                                                                              \
        strcpy((_dir)->err_msg, _msg);                                                             \
        goto _goto;                                                                                \
    } while (0)

static inline cmon_bool _should_ignore_file(const char * _name)
{
    //@TODO: What else should be skipped?
    return strcmp(_name, ".") == 0 || strcmp(_name, "..") == 0;
}

static cmon_bool _recurse_src_dir(cmon_src_dir * _dir,
                                  const char * _path,
                                  const char * _dirname,
                                  const char * _parent_mod_path,
                                  cmon_bool _is_root_dir)
{
    cmon_fs_dir d;
    cmon_fs_dirent ent;
    cmon_idx mod_idx = CMON_INVALID_IDX;

    if (!_is_root_dir)
    {
        cmon_str_builder_clear(_dir->str_builder);
        if (_parent_mod_path)
        {
            cmon_str_builder_append_fmt(_dir->str_builder, "%s.%s", _parent_mod_path, _dirname);
        }
        else
        {
            cmon_str_builder_append(_dir->str_builder, _dirname);
        }

        mod_idx =
            cmon_modules_add(_dir->mods, "_dirname", cmon_str_builder_c_str(_dir->str_builder));
    }

    printf("pathsas %s\n", _parent_mod_path);
    if (cmon_fs_open(_path, &d) == -1)
    {
        //@TODO: errno description and path?
        _err(_dir, err_end2, "failed to open src directory");
    }

    while (cmon_fs_has_next(&d))
    {
        if (cmon_fs_next(&d, &ent) == -1)
        {
            //@TODO: errno description and path?
            _err(_dir, err_end, "failed to access file");
        }

        if (_should_ignore_file(ent.name))
            continue;

        if (ent.type == cmon_fs_dirent_dir)
        {
            _recurse_src_dir(_dir,
                             ent.path,
                             ent.name,
                             cmon_is_valid_idx(mod_idx) ? cmon_modules_path(_dir->mods, mod_idx)
                                                        : NULL,
                             cmon_false);
        }
        else if (ent.type == cmon_fs_dirent_file)
        {
            char ext_buf[32];
            cmon_file_ext(ent.name, ext_buf, sizeof(ext_buf));

            if (strcmp(ext_buf, ".cmon") == 0 && !_is_root_dir)
            {
                assert(cmon_is_valid_idx(mod_idx));
                cmon_modules_add_src_file(_dir->mods, mod_idx, cmon_src_add(_dir->src, ent.path, ent.name));
            }
            else
            {
                // cmon_log_write(_builder->log, cmon_true, "skipping file %s", ent.name);
            }
        }
    }

    cmon_fs_close(&d);
    return cmon_false;

err_end:
    cmon_fs_close(&d);
err_end2:
    return cmon_true;
}

cmon_bool cmon_src_dir_parse(cmon_src_dir * _dir)
{
    return _recurse_src_dir(_dir, _dir->path, "", NULL, cmon_true);
}

const char * cmon_src_dir_err_msg(cmon_src_dir * _dir)
{
    return _dir->err_msg;
}
