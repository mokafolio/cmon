#include <cmon/cmon_exec.h>

int cmon_exec(const char * _cmd, cmon_str_builder * _output)
{
    FILE * pipe = popen(_cmd, "r");
    if (!pipe)
        goto err;

    if (_output)
    {
        //@NOTE: CMON_PATH_MAX used as a good guess for now, we can specify another constant for
        //this if needed
        char line_buffer[CMON_PATH_MAX];
        while (fgets(line_buffer, sizeof(line_buffer), pipe) != NULL)
        {
            cmon_str_builder_append(_output, line_buffer);
        }
    }

err:
    return WEXITSTATUS(pclose(pipe));
}
