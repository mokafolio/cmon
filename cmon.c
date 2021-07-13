#include <cmon/cmon_argparse.h>
#include <cmon/cmon_builder_st.h>
#include <cmon/cmon_codegen_c.h>
#include <cmon/cmon_dir_parse.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>

// panic macro that takes a goto label to jump to
#define _panic(_goto, _fmt, ...)                                                                   \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "Panic: " _fmt "\n", ##__VA_ARGS__);                                       \
        goto _goto;                                                                                \
    } while (0)

int main(int _argc, const char * _args[])
{
    cmon_allocator alloc = cmon_mallocator_make();
    cmon_argparse * ap = cmon_argparse_create(&alloc, "cmon");
    cmon_src * src = cmon_src_create(&alloc);
    cmon_modules * mods = cmon_modules_create(&alloc, src);
    cmon_log * log = NULL;
    cmon_builder_st * builder = NULL;
    // cmon_src_dir * sd = NULL;
    // // cmon_src_dir * dep_dir = NULL;
    // cmon_dyn_arr(cmon_src_dir *) dep_dirs;
    cmon_str_builder * tmp_strb = cmon_str_builder_create(&alloc, CMON_PATH_MAX);
    char cwd[CMON_PATH_MAX];
    char project_path[CMON_PATH_MAX];
    char src_path[CMON_PATH_MAX];
    char deps_path[CMON_PATH_MAX];
    char deps_pm_path[CMON_PATH_MAX];
    char build_path[CMON_PATH_MAX];

    // cmon_dyn_arr_init(&dep_dirs, &alloc, 4);

    CMON_UNUSED(cmon_argparse_add_arg(ap, "-h", "--help", cmon_false, "show this help and exit"));
    cmon_idx arg =
        cmon_argparse_add_arg(ap, "-b", "--buildtype", cmon_true, "changes the buildtype");
    cmon_argparse_add_possible_val(ap, arg, "release", cmon_false);
    cmon_argparse_add_possible_val(ap, arg, "debug", cmon_true);
    arg = cmon_argparse_add_arg(ap, "-d", "--dir", cmon_true, "path to the project directory");
    cmon_argparse_add_possible_val(ap, arg, "cwd", cmon_true);
    cmon_argparse_add_possible_val(ap, arg, "?", cmon_false);
    arg = cmon_argparse_add_arg(ap, "-e", "--errcount", cmon_true, "max error count");
    cmon_argparse_add_possible_val(ap, arg, "8", cmon_true);
    cmon_argparse_add_possible_val(ap, arg, "?", cmon_false);
    CMON_UNUSED(cmon_argparse_add_arg(
        ap, "-v", "--verbose", cmon_false, "print detailed compilation output"));
    CMON_UNUSED(cmon_argparse_add_arg(ap, "build", "", cmon_false, "build all modules"));
    CMON_UNUSED(cmon_argparse_add_arg(ap, "clean", "", cmon_false, "clean build directory"));

    cmon_argparse_parse(ap, _args, _argc);

    if (cmon_argparse_is_set(ap, "-h") ||
        (!cmon_argparse_is_set(ap, "clean") && !cmon_argparse_is_set(ap, "build")))
    {
        cmon_argparse_print_help(ap);
        goto end;
    }

    const char * project_dir = cmon_argparse_value(ap, "-d");
    if (strcmp(project_dir, "cwd") == 0)
    {
        if (!cmon_fs_getcwd(cwd, sizeof(cwd)))
        {
            _panic(end, "could not get current working directory.");
        }
        strcpy(project_path, cwd);
    }
    else
    {
        if (!cmon_fs_exists(project_dir))
        {
            _panic(end, "the project path does not exist");
        }
        else if (!cmon_fs_is_dir(project_dir))
        {
            _panic(end, "the project path does not point to a directory");
        }
        strcpy(project_path, project_dir);
    }

    cmon_join_paths(project_path, "src", src_path, sizeof(src_path));
    cmon_join_paths(project_path, "deps", deps_path, sizeof(deps_path));
    cmon_join_paths(project_path, "deps_pm", deps_pm_path, sizeof(deps_pm_path));
    cmon_join_paths(project_path, "build", build_path, sizeof(build_path));

    // clean and exit
    if (cmon_argparse_is_set(ap, "clean"))
    {
        if (cmon_fs_exists(build_path))
        {
            cmon_fs_dir dir;
            if (cmon_fs_open(build_path, &dir) != -1)
            {
                cmon_fs_dirent ent;
                while (cmon_fs_has_next(&dir))
                {
                    if (cmon_fs_next(&dir, &ent) == -1)
                    {
                        _panic(close_build_dir,
                               "failed to advance directory iterator while cleaning");
                    }
                    if (strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0)
                    {
                        continue;
                    }
                    if (cmon_fs_remove_all(ent.path) == -1)
                    {
                        _panic(close_build_dir,
                               "failed to remove %s while cleaning build directory",
                               ent.name);
                    }
                }
            close_build_dir:
                cmon_fs_close(&dir);
            }
            else
            {
                _panic(end, "failed to open build directory for cleaning");
            }
        }
        goto end;
    }
    else if (cmon_argparse_is_set(ap, "build"))
    {
        if (!cmon_fs_exists(src_path))
            _panic(end, "missing src directory at %s", project_path);

        char parse_dir_err[CMON_ERR_MSG_MAX];
        if (cmon_dir_parse_src(
                &alloc, src_path, mods, src, "src", parse_dir_err, sizeof(parse_dir_err)))
        {
            _panic(end, "failed to parse src directory: %s", parse_dir_err);
        }

        // set the src directory as a module search path on all src based modules
        for (size_t i = 0; i < cmon_modules_count(mods); ++i)
        {
            cmon_modules_add_search_prefix_c_str(mods, (cmon_idx)i, "src");
        }

        // optionally add/parse the dependency and pm_deps directory
        if (cmon_fs_exists(deps_path))
        {
            if (cmon_dir_parse_deps(
                    &alloc, deps_path, mods, src, parse_dir_err, sizeof(parse_dir_err)))
            {
                _panic(end, "failed to parse deps directory: %s", parse_dir_err);
            }
        }

        if (cmon_fs_exists(deps_pm_path))
        {
            if (cmon_dir_parse_deps(
                    &alloc, deps_pm_path, mods, src, parse_dir_err, sizeof(parse_dir_err)))
            {
                _panic(end, "failed to parse deps_pm directory: %s", parse_dir_err);
            }
        }

        if (!cmon_fs_exists(build_path))
        {
            if (cmon_fs_mkdir(build_path) == -1)
            {
                _panic(end, "failed to create build directory at %s", build_path);
            }
        }

        builder = cmon_builder_st_create(&alloc, atoi(cmon_argparse_value(ap, "-e")), src, mods);
        log = cmon_log_create(&alloc,
                              "cmon_build.log",
                              build_path,
                              cmon_argparse_is_set(ap, "-v") ? cmon_log_level_info
                                                             : cmon_log_level_error);
        if (cmon_argparse_is_set(ap, "-v"))
        {
            cmon_log_write_styled(log,
                                  cmon_log_level_info,
                                  cmon_log_color_default,
                                  cmon_log_color_red,
                                  cmon_log_style_underline | cmon_log_style_bold,
                                  "cmon");
            cmon_log_write(log, cmon_log_level_info, " compiler ");
            cmon_log_write_styled(log,
                                  cmon_log_level_info,
                                  cmon_log_color_default,
                                  cmon_log_color_default,
                                  cmon_log_style_light,
                                  "(v%i.%i.%i)",
                                  CMON_VERSION_MAJOR,
                                  CMON_VERSION_MINOR,
                                  CMON_VERSION_PATCH);
            cmon_log_write(log, cmon_log_level_info, "\n");
        }

        cmon_codegen cgen = cmon_codegen_c_make(&alloc);

        if (cmon_builder_st_build(builder, &cgen, build_path, log))
        {
            cmon_err_report * errs;
            size_t count;
            cmon_builder_st_errors(builder, &errs, &count);
            for (size_t i = 0; i < count; ++i)
            {
                cmon_log_write_err_report(log, &errs[i], src);
            }
        }

        cmon_codegen_dealloc(&cgen);
    }

end:
    cmon_builder_st_destroy(builder);

    cmon_str_builder_destroy(tmp_strb);
    cmon_log_destroy(log);
    cmon_modules_destroy(mods);
    cmon_src_destroy(src);
    cmon_argparse_destroy(ap);
    cmon_allocator_dealloc(&alloc);
    return EXIT_SUCCESS;
}
