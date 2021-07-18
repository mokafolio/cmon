#include <cmon/cmon_argparse.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_log.h>
#include <cmon/cmon_pm.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>

// panic macro that takes a goto label to jump to
#define _panic(_goto, _fmt, ...)                                                                   \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "Panic: " _fmt "\n", ##__VA_ARGS__);                                       \
        goto _goto;                                                                                \
    } while (0)

static inline cmon_bool _is_absolute_path(const char * _path)
{
    if (strlen(_path) && _path[0] == '/')
    {
        return cmon_true;
    }
    return cmon_false;
}

int main(int _argc, const char * _args[])
{
    cmon_allocator a = cmon_mallocator_make();
    cmon_argparse * ap = cmon_argparse_create(&a, "cmonpm");
    cmon_pm * pm = NULL;
    cmon_pm_lock_file * lf = NULL;

    CMON_UNUSED(cmon_argparse_add_arg(
        ap, CMON_INVALID_IDX, "-h", "--help", "show this help and exit", cmon_false, cmon_false));
    CMON_UNUSED(cmon_argparse_add_arg(
        ap, CMON_INVALID_IDX, "-v", "--verbose", "print detailed output", cmon_false, cmon_false));

    cmon_idx install_cmd =
        cmon_argparse_add_cmd(ap, "install", "install all dependencies to deps_pm");
    cmon_idx install_dir_arg = cmon_argparse_add_arg(ap,
                                                     install_cmd,
                                                     "-i",
                                                     "--install_dir",
                                                     "the installation directory dependencies",
                                                     cmon_false,
                                                     cmon_false);

    cmon_argparse_add_possible_val(ap, install_dir_arg, "deps_pm", cmon_true);
    cmon_argparse_add_possible_val(ap, install_dir_arg, "?", cmon_false);

    cmon_idx deps_file_arg = cmon_argparse_add_arg(
        ap,
        install_cmd,
        "-d",
        "--deps_file_dir",
        "path to a folder containing cmon_pm_deps.tini or cmon_pm_deps_all.tini",
        cmon_false,
        cmon_false);
    cmon_argparse_add_possible_val(ap, deps_file_arg, "cwd", cmon_true);
    cmon_argparse_add_possible_val(ap, deps_file_arg, "?", cmon_false);

    cmon_idx clean_cmd = cmon_argparse_add_cmd(ap, "clean", "cleans the installation directory");
    cmon_argparse_cmd_add_arg(ap, clean_cmd, install_dir_arg);

    cmon_idx add_cmd = cmon_argparse_add_cmd(ap, "add", "add a dependency");
    cmon_argparse_cmd_add_arg(ap, add_cmd, deps_file_arg);
    CMON_UNUSED(cmon_argparse_add_arg(ap,
                                      add_cmd,
                                      "",
                                      "--url",
                                      "url to the git respository of the dependency",
                                      cmon_true,
                                      cmon_true));
    cmon_idx version_arg = cmon_argparse_add_arg(ap,
                                                 add_cmd,
                                                 "",
                                                 "--version",
                                                 "the version of the dependency to pull",
                                                 cmon_true,
                                                 cmon_true);
    cmon_argparse_add_possible_val(ap, version_arg, "latest", cmon_false);
    cmon_argparse_add_possible_val(ap, version_arg, "?", cmon_false);

    cmon_argparse_parse(ap, _args, _argc);

    if (cmon_argparse_is_arg_set(ap, "-h") || !cmon_is_valid_idx(cmon_argparse_cmd(ap)))
    {
        cmon_argparse_print_help(ap);
        goto end;
    }

    pm = cmon_pm_create(&a, cmon_argparse_value(ap, "-i"));

    // fill in a couple of paths needed by commands
    char cwd[CMON_PATH_MAX];
    char install_dir_path[CMON_PATH_MAX];
    char abs_dep_dir_path[CMON_PATH_MAX];
    char abs_dep_file_path[CMON_PATH_MAX];
    char abs_lock_file_path[CMON_PATH_MAX];

    // get working directory
    if (!cmon_fs_getcwd(cwd, sizeof(cwd)))
    {
        _panic(end, "could not get current working directory.");
    }

    // fill in absolute install directory path
    const char * install_dir = cmon_argparse_value(ap, "-i");
    if (_is_absolute_path(install_dir))
    {
        strcpy(install_dir_path, install_dir);
    }
    else
    {
        cmon_join_paths(cwd, install_dir, install_dir_path, sizeof(install_dir_path));
    }

    // fill in absolute deps file dir path
    const char * deps_file_dir = cmon_argparse_value(ap, "-d");
    if (strcmp(deps_file_dir, "cwd") != 0)
    {
        if (_is_absolute_path(deps_file_dir))
        {
            strcpy(abs_dep_dir_path, deps_file_dir);
        }
        else
        {
            cmon_join_paths(cwd, deps_file_dir, abs_dep_dir_path, sizeof(abs_dep_dir_path));
        }
    }
    else
    {
        strcpy(abs_dep_dir_path, cwd);
    }

    // lock file path
    cmon_join_paths(
        abs_dep_dir_path, "cmon_pm_deps_all.tini", abs_lock_file_path, sizeof(abs_lock_file_path));

    // deps file path
    cmon_join_paths(
        abs_dep_dir_path, "cmon_pm_deps.tini", abs_dep_file_path, sizeof(abs_dep_file_path));

    if (cmon_argparse_cmd(ap) == clean_cmd)
    {
        if (cmon_fs_remove_all(install_dir_path))
        {
            _panic(end, "failed to remove dep dir at %s", install_dir_path);
        }
    }
    else if (cmon_argparse_cmd(ap) == install_cmd)
    {
        char err_msg[CMON_ERR_MSG_MAX];

        if (cmon_fs_exists(abs_lock_file_path))
        {
            lf = cmon_pm_lock_file_load(&a, abs_lock_file_path, err_msg, sizeof(err_msg));
            if (!lf)
            {
                _panic(end, "%s", err_msg);
            }
            if (cmon_pm_lock_file_pull(lf, cmon_argparse_value(ap, "-i")))
            {
                _panic(end, "%s", cmon_pm_lock_file_err_msg(lf));
            }
        }
        else
        {
            if (cmon_fs_exists(abs_dep_dir_path))
            {
                if (cmon_pm_load_deps_file(pm, CMON_INVALID_IDX, abs_dep_file_path))
                {
                    _panic(end, "%s", cmon_pm_err_msg(pm));
                }
                lf = cmon_pm_resolve(pm);
                if (!lf)
                {
                    _panic(end, "%s", cmon_pm_err_msg(pm));
                }

                if (cmon_pm_lock_file_save(lf, abs_lock_file_path))
                {
                    _panic(end, "%s", cmon_pm_lock_file_err_msg(lf));
                }
            }
            else
            {
                _panic(end,
                       "could not find cmon_pm_deps.tini or cmon_pm_deps_all.tini in '%s'",
                       deps_file_dir);
            }
        }
    }
    else if (cmon_argparse_cmd(ap) == add_cmd)
    {
        // if there is a cmon_pm_deps.tini, load it, add the new dependency.
        if (cmon_fs_exists(abs_dep_file_path))
        {
            if (cmon_pm_load_deps_file(pm, CMON_INVALID_IDX, abs_dep_file_path))
            {
                _panic(end, "%s", cmon_pm_err_msg(pm));
            }
        }

        // add the new module (if it does not exist yet)
        cmon_idx mod = cmon_pm_find_or_add_module_c_str(
            pm, cmon_argparse_value(ap, "--url"), cmon_argparse_value(ap, "--version"));

        //added as a dependency
        if(cmon_pm_add_dep(pm, CMON_INVALID_IDX, mod))
        {
            //@TODO: log dependency already existed
        }

        // save it
        if (cmon_pm_save_deps_file(pm, CMON_INVALID_IDX, abs_dep_file_path))
        {
            _panic(end, "%s", cmon_pm_err_msg(pm));
        }
    }

end:
    cmon_pm_lock_file_destroy(lf);
    cmon_pm_destroy(pm);
    cmon_argparse_destroy(ap);
    cmon_allocator_dealloc(&a);

    return EXIT_SUCCESS;
}
