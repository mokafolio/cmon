#include <cmon/cmon_argparse.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_log.h>
#include <cmon/cmon_pm.h>
#include <cmon/cmon_str_builder.h>

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

    CMON_UNUSED(cmon_argparse_add_arg(
        ap, CMON_INVALID_IDX, "-h", "--help", cmon_false, "show this help and exit"));
    CMON_UNUSED(cmon_argparse_add_arg(
        ap, CMON_INVALID_IDX, "-v", "--verbose", cmon_false, "print detailed output"));

    cmon_idx install_cmd =
        cmon_argparse_add_cmd(ap, "install", "install all dependencies to deps_pm");
    cmon_idx install_dir_arg = cmon_argparse_add_arg(ap,
                                                     install_cmd,
                                                     "-i",
                                                     "--install_dir",
                                                     cmon_false,
                                                     "the installation directory dependencies");
    cmon_argparse_add_possible_val(ap, install_dir_arg, "deps_pm", cmon_true);
    cmon_argparse_add_possible_val(ap, install_dir_arg, "?", cmon_false);

    cmon_idx deps_file_arg = cmon_argparse_add_arg(
        ap, install_cmd, "-d", "--deps_file", cmon_false, "path to dependency tini file");
    cmon_argparse_add_possible_val(ap, deps_file_arg, "cmon_pm_deps.tini", cmon_true);
    cmon_argparse_add_possible_val(ap, deps_file_arg, "?", cmon_false);

    cmon_idx clean_cmd = cmon_argparse_add_cmd(ap, "clean", "cleans the installation directory");
    cmon_argparse_cmd_add_arg(ap, clean_cmd, install_dir_arg);

    cmon_argparse_parse(ap, _args, _argc);

    if (cmon_argparse_is_arg_set(ap, "-h") || !cmon_is_valid_idx(cmon_argparse_cmd(ap)))
    {
        cmon_argparse_print_help(ap);
        goto end;
    }

    pm = cmon_pm_create(&a, cmon_argparse_value(ap, "-i"));

    char dep_dir_path[CMON_PATH_MAX];
    const char * install_dir = cmon_argparse_value(ap, "-i");
    if (_is_absolute_path(install_dir))
    {
        strcpy(dep_dir_path, install_dir);
    }
    else
    {
        if (!cmon_fs_getcwd(dep_dir_path, sizeof(dep_dir_path)))
        {
            _panic(end, "could not get current working directory.");
        }
        cmon_join_paths(dep_dir_path, install_dir, dep_dir_path, sizeof(dep_dir_path));
    }

    if (cmon_argparse_cmd(ap) == clean_cmd)
    {
        if (cmon_fs_remove_all(dep_dir_path))
        {
            _panic(end, "failed to remove dep dir at %s", dep_dir_path);
        }
    }
    else if (cmon_argparse_cmd(ap) == install_cmd)
    {
    }

end:
    cmon_pm_destroy(pm);
    cmon_argparse_destroy(ap);
    cmon_allocator_dealloc(&a);
}
