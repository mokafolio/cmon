#include <cmon/cmon_argparse.h>
#include <cmon/cmon_builder_st.h>
#include <cmon/cmon_codegen_c.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_src_dir.h>
#include <cmon/cmon_util.h>

#define _panic(_fmt, ...)                                                                          \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "Panic: " _fmt "\n", ##__VA_ARGS__);                                       \
        goto end;                                                                                  \
    } while (0)

int main(int _argc, const char * _args[])
{
    cmon_allocator alloc = cmon_mallocator_make();
    cmon_argparse * ap = cmon_argparse_create(&alloc, "cmon");
    cmon_src * src = cmon_src_create(&alloc);
    cmon_modules * mods = cmon_modules_create(&alloc, src);
    cmon_log * log = NULL;
    cmon_builder_st * builder = NULL;
    cmon_src_dir * sd = NULL;
    char cwd[CMON_PATH_MAX];
    char project_path[CMON_PATH_MAX];
    char src_path[CMON_PATH_MAX];
    char build_path[CMON_PATH_MAX];

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
    CMON_UNUSED(cmon_argparse_add_arg(ap, "-c", "--clean", cmon_false, "clean build directory"));

    cmon_argparse_parse(ap, _args, _argc);

    if (cmon_argparse_is_set(ap, "-h"))
    {
        cmon_argparse_print_help(ap);
        goto end;
    }

    const char * project_dir = cmon_argparse_value(ap, "-d");
    if (strcmp(project_dir, "cwd") == 0)
    {
        if (!cmon_fs_getcwd(cwd, sizeof(cwd)))
        {
            _panic("could not get current working directory.");
        }
        strcpy(project_path, cwd);
    }
    else
    {
        if (!cmon_fs_exists(project_dir))
        {
            _panic("the project path does not exist");
        }
        else if (!cmon_fs_is_dir(project_dir))
        {
            _panic("the project path does not point to a directory");
        }
        strcpy(project_path, project_dir);
    }

    cmon_join_paths(project_path, "src", src_path, sizeof(src_path));
    cmon_join_paths(project_path, "build", build_path, sizeof(build_path));

    if (!cmon_fs_exists(src_path))
        _panic("missing src directory at %s", project_path);

    sd = cmon_src_dir_create(&alloc, src_path, mods, src);
    if (cmon_src_dir_parse(sd))
    {
        _panic("failed to parse src directory: %s", cmon_src_dir_err_msg(sd));
    }

    if (!cmon_fs_exists(build_path))
    {
        if (cmon_fs_mkdir(build_path) == -1)
        {
            _panic("failed to create build directory at %s", build_path);
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

end:
    cmon_codegen_dealloc(&cgen);
    cmon_builder_st_destroy(builder);
    cmon_src_dir_destroy(sd);
    cmon_log_destroy(log);
    cmon_modules_destroy(mods);
    cmon_src_destroy(src);
    cmon_argparse_destroy(ap);
    cmon_allocator_dealloc(&alloc);
    return EXIT_SUCCESS;
}
