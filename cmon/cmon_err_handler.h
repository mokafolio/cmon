#ifndef CMON_CMON_ERR_HANDLER_H
#define CMON_CMON_ERR_HANDLER_H

#include <cmon/cmon_src.h>
#include <cmon/cmon_err_report.h>
#include <setjmp.h>

typedef struct cmon_err_handler cmon_err_handler;

CMON_API cmon_err_handler * cmon_err_handler_create(cmon_allocator * _alloc,
                                                    cmon_src * _src,
                                                    size_t _max_errors);
CMON_API void cmon_err_handler_destroy(cmon_err_handler * _e);
CMON_API void cmon_err_handler_err(
    cmon_err_handler * _e, cmon_bool _jump, cmon_idx _src_file_idx, cmon_idx _tok_idx, const char * _fmt, ...);
CMON_API void cmon_err_handler_add_err(cmon_err_handler * _e, cmon_bool _jump, cmon_err_report * _err);
CMON_API void cmon_err_handler_set_jump(cmon_err_handler * _e, jmp_buf * _jmp);
CMON_API void cmon_err_handler_set_src(cmon_err_handler * _e, cmon_src * _src);
CMON_API void cmon_err_handler_jump(cmon_err_handler * _e, cmon_bool _jmp_on_any_err);
CMON_API size_t cmon_err_handler_count(cmon_err_handler * _e);
CMON_API cmon_err_report * cmon_err_handler_err_report(cmon_err_handler * _e, size_t _idx);
CMON_API void cmon_err_handler_clear(cmon_err_handler * _e);

#endif // CMON_CMON_ERR_HANDLER_H
