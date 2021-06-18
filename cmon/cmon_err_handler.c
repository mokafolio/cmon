#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_handler.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_tokens.h>
#include <stdarg.h>

typedef struct cmon_err_handler
{
    cmon_allocator * alloc;
    cmon_src * src;
    jmp_buf * jmp;
    size_t max_errors;
    cmon_str_builder * str_builder;
    cmon_dyn_arr(cmon_err_report) errs;
} cmon_err_handler;

cmon_err_handler * cmon_err_handler_create(cmon_allocator * _alloc,
                                           cmon_src * _src,
                                           size_t _max_errors)
{
    cmon_err_handler * ret = CMON_CREATE(_alloc, cmon_err_handler);
    ret->alloc = _alloc;
    ret->jmp = NULL;
    ret->src = _src;
    ret->max_errors = _max_errors;
    ret->str_builder = cmon_str_builder_create(_alloc, 256);
    cmon_dyn_arr_init(&ret->errs, _alloc, _max_errors);
    return ret;
}

void cmon_err_handler_destroy(cmon_err_handler * _e)
{
    if (!_e)
        return;

    cmon_dyn_arr_dealloc(&_e->errs);
    cmon_str_builder_destroy(_e->str_builder);
    CMON_DESTROY(_e->alloc, _e);
}

void cmon_err_handler_err(cmon_err_handler * _e,
                          cmon_bool _jump,
                          cmon_idx _src_file_idx,
                          cmon_idx _tok_idx,
                          const char * _fmt,
                          ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_str_builder_clear(_e->str_builder);

    // cmon_tokens * toks = cmon_src_tokens(_e->src, _src_file_idx);
    // toks = cmon_src_tokens(_e->src, _src_file_idx);
    cmon_str_builder_append_fmt_v(_e->str_builder, _fmt, args);

    // size_t line = 0;
    // size_t line_off = 0;
    // if(cmon_is_valid_idx(_tok_idx))
    // {
    //     line = cmon_tokens_line(toks, _tok_idx);
    //     line_off = cmon_tokens_line_offset(toks, _tok_idx);
    // }

    cmon_err_report err = cmon_err_report_make(_src_file_idx,
                                               _tok_idx,
                                               cmon_str_builder_c_str(_e->str_builder));
    va_end(args);
    cmon_err_handler_add_err(_e, _jump, &err);
}

void cmon_err_handler_add_err(cmon_err_handler * _e, cmon_bool _jump, cmon_err_report * _err)
{
    cmon_dyn_arr_append(&_e->errs, *_err);
    if (_jump)
    {
        cmon_err_handler_jump(_e, cmon_false);
    }
}

void cmon_err_handler_set_src(cmon_err_handler * _e, cmon_src * _src)
{
    _e->src = _src;
}

void cmon_err_handler_set_jump(cmon_err_handler * _e, jmp_buf * _jmp)
{
    _e->jmp = _jmp;
}

void cmon_err_handler_jump(cmon_err_handler * _e, cmon_bool _jmp_on_any_err)
{
    if (_e->jmp && ((cmon_dyn_arr_count(&_e->errs) && _jmp_on_any_err) ||
                    cmon_dyn_arr_count(&_e->errs) >= _e->max_errors))
    {
        longjmp(*_e->jmp, 1);
    }
}

size_t cmon_err_handler_count(cmon_err_handler * _e)
{
    return cmon_dyn_arr_count(&_e->errs);
}

cmon_err_report * cmon_err_handler_err_report(cmon_err_handler * _e, size_t _idx)
{
    assert(_idx < cmon_dyn_arr_count(&_e->errs));
    return &_e->errs[_idx];
}

void cmon_err_handler_clear(cmon_err_handler * _e)
{
    cmon_dyn_arr_clear(&_e->errs);
}
