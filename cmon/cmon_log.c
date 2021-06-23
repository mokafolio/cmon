#include <cmon/cmon_fs.h>
#include <cmon/cmon_log.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tokens.h>
#include <cmon/cmon_util.h>
#include <time.h>

#define COL_RESET "\x1B[0m"
#define COL_CYAN "\x1B[36m"
#define COL_RED "\x1B[31m"
#define COL_MAGENTA "\x1B[95m"
#define COL_BOLD "\x1B[1m"

typedef struct cmon_log
{
    cmon_allocator * alloc;
    cmon_str_builder * str_builder;
    cmon_str_builder * printf_str_builder;
    char name[CMON_FILENAME_MAX];
    char path[CMON_PATH_MAX];
    FILE * file;
    size_t writes_since_last_flush;
    size_t flush_every_n;
    cmon_log_level print_lvl;
} cmon_log;

static inline void _write_to_file(cmon_log * _l, const char * _prefix, const char * _txt)
{
    fputs(_prefix, _l->file);
    fputs(_txt, _l->file);
    if (ferror(_l->file) != 0)
    {
        cmon_panic("failed to write to log file at %s", _l->path);
    }

    if (_l->writes_since_last_flush == _l->flush_every_n)
    {
        if (fflush(_l->file) != 0)
        {
            cmon_panic("failed to flush log file at %s", _l->path);
        }
        _l->writes_since_last_flush = 0;
    }
}

static inline const char * _timestamp(char * _buf, size_t _buf_size)
{
    time_t now;
    struct tm ts;
    time(&now);
    ts = *localtime(&now);
    strftime(_buf, _buf_size, "%a %m/%d/%Y", &ts);
    return _buf;
}

#define _tstamp(_buf) _timestamp((_buf), sizeof((_buf)))

cmon_log * cmon_log_create(cmon_allocator * _alloc,
                           const char * _name,
                           const char * _path,
                           cmon_log_level _prinf_lvl)
{
    cmon_log * ret = CMON_CREATE(_alloc, cmon_log);
    ret->str_builder = cmon_str_builder_create(_alloc, 512);
    ret->printf_str_builder = cmon_str_builder_create(_alloc, 256);

    ret->alloc = _alloc;
    strcpy(ret->name, _name);
    cmon_join_paths(_path, _name, ret->path, sizeof(ret->path));

    ret->file = fopen(ret->path, "w");
    if (!ret->file)
    {
        cmon_panic("could not open log file at %s", _path);
    }
    ret->print_lvl = _prinf_lvl;
    ret->flush_every_n = 4; //@TODO: make this customizable?
    ret->writes_since_last_flush = 0;

    char tbuf[128];
    cmon_log_write(ret, cmon_log_level_debug, "log '%s', started at %s\n", _name, _tstamp(tbuf));

    return ret;
}

void cmon_log_destroy(cmon_log * _log)
{
    if (!_log)
        return;

    fclose(_log->file);
    cmon_str_builder_destroy(_log->printf_str_builder);
    cmon_str_builder_destroy(_log->str_builder);
    CMON_DESTROY(_log->alloc, _log);
}

void cmon_log_write_v(cmon_log * _log, cmon_log_level _lvl, const char * _fmt, va_list _args)
{
    cmon_str_builder_clear(_log->str_builder);
    cmon_str_builder_append_fmt_v(_log->str_builder, _fmt, _args);
    _write_to_file(_log, "", cmon_str_builder_c_str(_log->str_builder));
    if (_lvl <= _log->print_lvl)
    {
        printf("%s", cmon_str_builder_c_str(_log->str_builder));
    }
}

void cmon_log_write(cmon_log * _log, cmon_log_level _lvl, const char * _fmt, ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_log_write_v(_log, _lvl, _fmt, args);
    va_end(args);
}

static inline cmon_bool _style_is_set(uint32_t _bitmask, cmon_log_style _style)
{
    return (_bitmask & _style) == _style;
}

static inline void _append_formatting(cmon_str_builder * _b, const char * _str, size_t _start_count)
{
    // prepend semicolon separator if the builder contains more then the escape sequence (we use the
    // arg _start_count so that the str builder can contain things before the escape sequence)
    if (cmon_str_builder_count(_b) > _start_count)
    {
        cmon_str_builder_append_fmt(_b, ";%s", _str);
    }
    else
    {
        cmon_str_builder_append(_b, _str);
    }
}

static inline void _write_color(cmon_str_builder * _b,
                                cmon_log_color _color,
                                cmon_bool _is_bg,
                                size_t _start_count)
{
    if (_color == cmon_log_color_default)
        return;

    if (_is_bg)
    {
        _append_formatting(_b, "4", _start_count);
    }
    else
    {
        _append_formatting(_b, "3", _start_count);
    }

    switch (_color)
    {
    case cmon_log_color_black:
        cmon_str_builder_append(_b, "0");
        break;
    case cmon_log_color_red:
        cmon_str_builder_append(_b, "1");
        break;
    case cmon_log_color_green:
        cmon_str_builder_append(_b, "2");
        break;
    case cmon_log_color_yellow:
        cmon_str_builder_append(_b, "3");
        break;
    case cmon_log_color_blue:
        cmon_str_builder_append(_b, "4");
        break;
    case cmon_log_color_magenta:
        cmon_str_builder_append(_b, "5");
        break;
    case cmon_log_color_cyan:
        cmon_str_builder_append(_b, "6");
        break;
    case cmon_log_color_white:
        cmon_str_builder_append(_b, "7");
        break;
    case cmon_log_color_default:
    default:
        break;
    }
}

static inline void _append_col_and_style(cmon_str_builder * _b,
                                         cmon_log_color _color,
                                         cmon_log_color _bg_color,
                                         cmon_log_style _style)
{
    cmon_str_builder_append(_b, "\x1b[");
    size_t start_count = cmon_str_builder_count(_b);
    _write_color(_b, _color, cmon_false, start_count);
    _write_color(_b, _bg_color, cmon_true, start_count);
    if (_style_is_set((uint32_t)_style, cmon_log_style_bold))
    {
        _append_formatting(_b, "1", start_count);
    }
    if (_style_is_set((uint32_t)_style, cmon_log_style_light))
    {
        _append_formatting(_b, "2", start_count);
    }
    if (_style_is_set((uint32_t)_style, cmon_log_style_underline))
    {
        _append_formatting(_b, "4", start_count);
    }
    cmon_str_builder_append(_b, "m");
}

static inline void _append_reset(cmon_str_builder * _b)
{
    cmon_str_builder_append(_b, "\x1b[0m");
}

static inline void _printf_styled(cmon_log * _log,
                                  cmon_log_color _color,
                                  cmon_log_color _bg_color,
                                  cmon_log_style _style,
                                  const char * _msg)
{
    cmon_str_builder_clear(_log->printf_str_builder);
    _append_col_and_style(_log->printf_str_builder, _color, _bg_color, _style);
    printf("%s%s\x1b[0m", cmon_str_builder_c_str(_log->printf_str_builder), _msg);
}

void cmon_log_write_styled_v(cmon_log * _log,
                             cmon_log_level _lvl,
                             cmon_log_color _color,
                             cmon_log_color _bg_color,
                             cmon_log_style _style,
                             const char * _fmt,
                             va_list _args)
{
    cmon_str_builder_clear(_log->str_builder);
    cmon_str_builder_append_fmt_v(_log->str_builder, _fmt, _args);
    _write_to_file(_log, "", cmon_str_builder_c_str(_log->str_builder));
    if (_lvl <= _log->print_lvl)
    {
        _printf_styled(_log, _color, _bg_color, _style, cmon_str_builder_c_str(_log->str_builder));
    }
}

void cmon_log_write_styled(cmon_log * _log,
                           cmon_log_level _lvl,
                           cmon_log_color _color,
                           cmon_log_color _bg_color,
                           cmon_log_style _style,
                           const char * _fmt,
                           ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_log_write_styled_v(_log, _lvl, _color, _bg_color, _style, _fmt, args);
    va_end(args);
}

static inline void enable_err_style(cmon_str_builder * _b,
                                    cmon_bool _condition,
                                    cmon_bool * _err_style_active)
{
    if (_condition && !*_err_style_active)
    {
        *_err_style_active = cmon_true;
        _append_col_and_style(_b, cmon_log_color_default, cmon_log_color_red, cmon_log_style_none);
    }
}

static inline void _write_err(cmon_str_builder * _b,
                              cmon_err_report * _err,
                              cmon_src * _src,
                              cmon_bool _add_styling)
{
    cmon_tokens * tokens = cmon_src_tokens(_src, _err->src_file_idx);

    cmon_str_builder_clear(_b);

    // 01. Add the error message.
    if (_add_styling)
    {
        _append_col_and_style(_b, cmon_log_color_red, cmon_log_color_default, cmon_log_style_bold);
    }

    cmon_str_builder_append(_b, "error: ");

    if (_add_styling)
    {
        _append_reset(_b);
        _append_col_and_style(
            _b, cmon_log_color_default, cmon_log_color_default, cmon_log_style_light);
    }

    if (cmon_is_valid_idx(cmon_err_report_token_first(_err)))
    {
        cmon_str_builder_append_fmt(_b,
                                    "%s:%lu:%lu:",
                                    cmon_err_report_filename(_err, _src),
                                    cmon_err_report_line(_err, _src),
                                    cmon_err_report_line_offset(_err, _src));
    }

    if (_add_styling)
    {
        _append_reset(_b);
    }
    cmon_str_builder_append_fmt(_b, " %s\n", _err->msg);

    // 02. Early out if the error is not associated with a token
    if (!cmon_is_valid_idx(cmon_err_report_token_first(_err)))
    {
        return;
    }

    // 03. Otherwise print the code lines associated with the error.
    cmon_idx first_tok = cmon_err_report_token_first(_err);
    cmon_idx last_tok = cmon_err_report_token_last(_err);
    cmon_str_view err_sv = (cmon_str_view){ cmon_tokens_str_view(tokens, first_tok).begin,
                                            cmon_tokens_str_view(tokens, last_tok).end };
    cmon_idx first_line = cmon_tokens_line(tokens, cmon_err_report_token_first(_err));
    cmon_idx last_line = cmon_tokens_line(tokens, cmon_err_report_token_last(_err));

    for (cmon_idx i = first_line; i <= last_line; ++i)
    {
        cmon_str_view line_sv = cmon_tokens_line_str_view(tokens, i);

        if (_add_styling)
        {
            _append_reset(_b);
            _append_col_and_style(
                _b, cmon_log_color_default, cmon_log_color_default, cmon_log_style_light);
        }

        size_t line_num_offset = cmon_str_builder_count(_b);
        cmon_str_builder_append_fmt(_b, "%02lu: ", i);
        line_num_offset = cmon_str_builder_count(_b) - line_num_offset;

        if (_add_styling)
        {
            _append_reset(_b);
        }

        const char * last_end = line_sv.begin;
        cmon_bool err_style_active = cmon_false;
        for (size_t j = 0; j < cmon_tokens_line_token_count(tokens, i); ++j)
        {
            cmon_idx tok = cmon_tokens_line_token(tokens, i, j);
            cmon_str_view tok_sv = cmon_tokens_str_view(tokens, tok);

            const char * tbegin = CMON_MAX(line_sv.begin, tok_sv.begin);
            const char * tend = CMON_MIN(line_sv.end, tok_sv.end);

            // enable err style for the whitespace if we the whitespace is within the error string
            // view
            enable_err_style(_b,
                             _add_styling && last_end >= err_sv.begin && last_end < err_sv.end,
                             &err_style_active);

            // fill in the whitespace between tokens
            if (last_end != tbegin)
            {
                cmon_str_builder_append_fmt(_b, "%.*s", tbegin - last_end, last_end);
            }

            // if the whitespace was outside the error range, potentially enable the error style now
            enable_err_style(
                _b, _add_styling && tok >= first_tok && tok <= last_tok, &err_style_active);

            cmon_str_builder_append_fmt(_b, "%.*s", tend - tbegin, tbegin);
            last_end = tend;

            if (err_style_active && _add_styling && tok >= last_tok)
            {
                err_style_active = cmon_false;
                _append_reset(_b);
            }
        }

        if (err_style_active && _add_styling)
        {
            _append_reset(_b);
        }

        cmon_str_builder_append(_b, "\n");

        for (size_t i = 0; i < line_num_offset; ++i)
        {
            cmon_str_builder_append(_b, " ");
        }

        if (_add_styling)
        {
            _append_col_and_style(
                _b, cmon_log_color_yellow, cmon_log_color_default, cmon_log_style_none);
        }

        for (const char * begin = line_sv.begin; begin != line_sv.end; ++begin)
        {
            if (begin >= err_sv.begin && begin < err_sv.end)
            {
                if (begin == cmon_tokens_str_view(tokens, cmon_err_report_token(_err)).begin)
                {
                    cmon_str_builder_append(_b, "^");
                }
                else
                {
                    cmon_str_builder_append(_b, "~");
                }
            }
            else
            {
                cmon_str_builder_append(_b, " ");
            }
        }
        if (_add_styling)
        {
            _append_reset(_b);
        }
        cmon_str_builder_append(_b, "\n");
    }
}

void cmon_log_write_err_report(cmon_log * _log, cmon_err_report * _err, cmon_src * _src)
{
    _write_err(_log->str_builder, _err, _src, cmon_false);
    _write_to_file(_log, "", cmon_str_builder_c_str(_log->str_builder));

    if (cmon_log_level_error <= _log->print_lvl)
    {
        _write_err(_log->printf_str_builder, _err, _src, cmon_true);
        printf("%s", cmon_str_builder_c_str(_log->printf_str_builder));
    }
}