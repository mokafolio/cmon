#include <cmon/cmon_dir_parse.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tini.h>
#include <cmon/cmon_util.h>
#include <stdio.h>

#define _err(_s, _goto, _fmt, ...)                                                                 \
    do                                                                                             \
    {                                                                                              \
        snprintf((_s)->err_buf, (_s)->err_buf_size, (_fmt), ##__VA_ARGS__);                        \
        goto _goto;                                                                                \
    } while (0)

typedef struct
{
    const char * path;
    cmon_str_builder * str_builder;
    cmon_modules * mods;
    cmon_src * src;
    char * err_buf;
    size_t err_buf_size;
} _src_session;

typedef struct
{
    char * err_buf;
    size_t err_buf_size;
    // search prefixes for all top level modules in deps folder.
    // Used to add search paths to all modules.
    cmon_dyn_arr(cmon_short_str) search_prefixes;
} _deps_session;

static inline cmon_bool _should_ignore_file(const char * _name)
{
    //@TODO: What else should be skipped?
    return strcmp(_name, ".") == 0 || strcmp(_name, "..") == 0;
}

//@TODO: this function is a lil messy, room for improvement
static cmon_bool _recurse_src_dir(_src_session * _dir,
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
        assert(_parent_mod_path);
        cmon_str_builder_clear(_dir->str_builder);
        // if (_parent_mod_path)
        // {
        cmon_str_builder_append_fmt(_dir->str_builder, "%s.%s", _parent_mod_path, _dirname);
        // }
        // else
        // {
        // if (_mod_path_prefix)
        // {
        //     cmon_str_builder_append_fmt(_dir->str_builder, "%s.%s", _mod_path_prefix,
        //     _dirname);
        // }
        // else
        // {
        // cmon_str_builder_append(_dir->str_builder, _dirname);
        // }
        // }

        printf("ADDING MODULE %s\n", cmon_str_builder_c_str(_dir->str_builder));
        mod_idx = cmon_modules_add(_dir->mods, cmon_str_builder_c_str(_dir->str_builder), _dirname);
    }

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
        {
            continue;
        }

        if (ent.type == cmon_fs_dirent_dir)
        {
            _recurse_src_dir(_dir,
                             ent.path,
                             ent.name,
                             cmon_is_valid_idx(mod_idx) ? cmon_modules_path(_dir->mods, mod_idx)
                                                        : _parent_mod_path,
                             cmon_false);
        }
        else if (ent.type == cmon_fs_dirent_file)
        {
            if (_parent_mod_path)
            {
                char ext_buf[32];
                cmon_file_ext(ent.name, ext_buf, sizeof(ext_buf));

                if (strcmp(ext_buf, ".cmon") == 0)
                {
                    assert(cmon_is_valid_idx(mod_idx));
                    cmon_modules_add_src_file(
                        _dir->mods, mod_idx, cmon_src_add(_dir->src, ent.path, ent.name));
                }
                else
                {
                    //@TODO:
                    // cmon_log_write(_builder->log, cmon_true, "skipping file %s", ent.name);
                }
            }
            else
            {
                //@TODO: Anything? Just ignore?
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

cmon_bool cmon_dir_parse_src(cmon_allocator * _alloc,
                             const char * _path,
                             cmon_modules * _mods,
                             cmon_src * _src,
                             const char * _base_path_prefix,
                             char * _err_msg_buf,
                             size_t _buf_size)
{

    _src_session sess;
    sess.path = _path;
    sess.mods = _mods;
    sess.src = _src;
    sess.err_buf = _err_msg_buf;
    sess.err_buf_size = _buf_size;
    sess.str_builder = cmon_str_builder_create(_alloc, CMON_PATH_MAX);

    char filename[CMON_FILENAME_MAX];
    cmon_bool ret = _recurse_src_dir(&sess,
                                     sess.path,
                                     cmon_filename(sess.path, filename, sizeof(filename)),
                                     _base_path_prefix,
                                     cmon_true);

    cmon_str_builder_destroy(sess.str_builder);
    return ret;
}

cmon_bool cmon_dir_parse_deps(cmon_allocator * _alloc,
                              const char * _path,
                              cmon_modules * _mods,
                              cmon_src * _src,
                              char * _err_msg_buf,
                              size_t _buf_size)
{
    _deps_session sess;
    sess.err_buf = _err_msg_buf;
    sess.err_buf_size = _buf_size;
    cmon_dyn_arr_init(&sess.search_prefixes, _alloc, 8);

    cmon_bool ret = cmon_false;
    cmon_fs_dir deps_dir;
    cmon_fs_dirent ent;
    char dep_src_path[CMON_PATH_MAX];
    if (cmon_fs_open(_path, &deps_dir) == -1)
    {
        ret = cmon_true;
        _err(&sess, deps_end, "failed to open deps directory");
    }

    while (cmon_fs_has_next(&deps_dir))
    {
        if (cmon_fs_next(&deps_dir, &ent) == -1)
        {
            ret = cmon_true;
            _err(&sess, deps_end, "failed to advance deps directory iterator");
        }

        if (_should_ignore_file(ent.name))
        {
            continue;
        }

        if (ent.type == cmon_fs_dirent_dir)
        {
            cmon_join_paths(ent.path, "src", dep_src_path, sizeof(dep_src_path));
            if (cmon_fs_exists(dep_src_path))
            {
                if (cmon_dir_parse_src(_alloc,
                                       dep_src_path,
                                       _mods,
                                       _src,
                                       ent.name,
                                       sess.err_buf,
                                       sess.err_buf_size))
                {
                    ret = cmon_true;
                    goto deps_end;
                }
                cmon_dyn_arr_append(&sess.search_prefixes, cmon_short_str_make(_alloc, ent.name));
            }
            //@TODO: else log dependency folder without src?
        }
        //@TODO: else log skipped/unexpeced file?
    }

    // set each dependency directory as a module search path on all modules
    printf("ADDING DEP PATHS %lu %lu\n",
           cmon_modules_count(_mods),
           cmon_dyn_arr_count(&sess.search_prefixes));
    for (size_t i = 0; i < cmon_modules_count(_mods); ++i)
    {
        for (size_t j = 0; j < cmon_dyn_arr_count(&sess.search_prefixes); ++j)
        {
            printf("ADDING DEP PATH %s\n", cmon_short_str_c_str(&sess.search_prefixes[j]));
            cmon_modules_add_search_prefix_c_str(
                _mods, (cmon_idx)i, cmon_short_str_c_str(&sess.search_prefixes[j]));
        }
    }

    // apply dependency mapping if applicable
    char deps_mapping_path[CMON_PATH_MAX];
    cmon_join_paths(_path, "deps_mapping.tini", deps_mapping_path, sizeof(deps_mapping_path));
    // check if there is a deps_mapping.tini file to directly map certain paths within a module to
    // one specific module
    if (cmon_fs_exists(deps_mapping_path))
    {
        cmon_tini_err terr;
        cmon_tini * tini = cmon_tini_parse_file(_alloc, deps_mapping_path, &terr);
        if (!tini)
        {
            ret = cmon_true;
            _err(&sess, deps_end, "failed to parse deps_mapping.tini");
        }

        cmon_idx root = cmon_tini_root_obj(tini);

        printf("TINI C COUNT %lu\n", cmon_tini_child_count(tini, root));
        // set all the path overwrites on all modules having the path prefix
        for (size_t i = 0; i < cmon_tini_child_count(tini, root); ++i)
        {
            cmon_idx child = cmon_tini_child(tini, root, i);
            cmon_str_view prefix = cmon_tini_pair_key(tini, child);
            cmon_idx val = cmon_tini_pair_value(tini, child);

            if (cmon_tini_kind(tini, val) == cmon_tinik_obj)
            {
                // @NOTE: for now we just iterate over all modules as its simple but not very
                // efficient.
                // Possibly add a way to get all modules prefixed with X to cmon modules?
                for (size_t j = 0; j < cmon_modules_count(_mods); ++j)
                {
                    if (strncmp(prefix.begin,
                                cmon_modules_path(_mods, (cmon_idx)j),
                                prefix.end - prefix.begin) == 0)
                    {
                        for (size_t k = 0; k < cmon_tini_child_count(tini, val); ++k)
                        {
                            cmon_idx ochild = cmon_tini_child(tini, val, k);
                            printf("DA FOCKING KEY %.*s\n",
                                   cmon_tini_pair_key(tini, ochild).end -
                                       cmon_tini_pair_key(tini, ochild).begin,
                                   cmon_tini_pair_key(tini, ochild).begin);

                            cmon_modules_add_path_overwrite(
                                _mods,
                                (cmon_idx)j,
                                cmon_tini_pair_key(tini, ochild),
                                cmon_tini_string(tini, cmon_tini_pair_value(tini, ochild)));
                        }
                    }
                }
            }
            else
            {
                //@NOTE: Silently ignore this?
            }
        }

        cmon_tini_destroy(tini);
    }

deps_end:
    cmon_fs_close(&deps_dir);
    for (size_t i = 0; i < cmon_dyn_arr_count(&sess.search_prefixes); ++i)
    {
        cmon_short_str_dealloc(&sess.search_prefixes[i]);
    }
    cmon_dyn_arr_dealloc(&sess.search_prefixes);
    return ret;
}
