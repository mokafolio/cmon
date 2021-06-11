#ifndef CMON_CMON_CODEGEN_H
#define CMON_CMON_CODEGEN_H

#include <cmon/cmon_ir.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_types.h>

// abstract code generation interface
typedef cmon_idx (*cmon_codegen_begin_session_fn)(void *, cmon_idx, cmon_ir *);
typedef void (*cmon_codegen_end_session_fn)(void *, cmon_idx);
typedef cmon_bool (*cmon_codegen_prep_fn)(void *, cmon_modules *, cmon_types *, const char *);
typedef cmon_bool (*cmon_codegen_fn)(void *, cmon_idx);
typedef void (*cmon_codegen_shutdown_fn)(void *);
typedef const char * (*cmon_codegen_session_err_msg_fn)(void *, cmon_idx);
typedef const char * (*cmon_codegen_err_msg_fn)(void *);

typedef struct
{
    void * obj;
    cmon_codegen_prep_fn prep_fn;
    cmon_codegen_begin_session_fn begin_session_fn;
    cmon_codegen_end_session_fn end_session_fn;
    cmon_codegen_fn fn;
    cmon_codegen_shutdown_fn shutdown_fn;
    cmon_codegen_err_msg_fn err_msg_fn;
    cmon_codegen_session_err_msg_fn sess_err_msg_fn;
} cmon_codegen;

CMON_API cmon_codegen cmon_codegen_make_empty();
CMON_API void cmon_codegen_dealloc(cmon_codegen * _cg);
CMON_API cmon_bool cmon_codegen_prepare(cmon_codegen * _cg, cmon_modules * _mods, cmon_types * _types, const char * _build_dir);
CMON_API cmon_idx cmon_codegen_begin_session(cmon_codegen * _cg, cmon_idx _mod_idx, cmon_ir * _ir);
CMON_API void cmon_codegen_end_session(cmon_codegen * _cg, cmon_idx _session_idx);
CMON_API cmon_bool cmon_codegen_gen(cmon_codegen * _cg, cmon_idx _session_idx);
CMON_API const char * cmon_codegen_err_msg(cmon_codegen * _cg);
CMON_API const char * cmon_codegen_session_err_msg(cmon_codegen * _cg, cmon_idx _session_idx);

// typedef cmon_bool (*cmon_codegen_prep_fn)(const char *, cmon_short_str *);
// typedef cmon_bool (*cmon_codegen_fn)(void *, cmon_idx);

// typedef struct
// {
//     cmon_codegen_prep_fn prep_fn;
//     cmon_codegen_fn fn;
// } cmon_codegen;

// CMON_API cmon_bool cmon_codegen_prepare(cmon_codegen * _cg,
//                                         const char * _build_dir,
//                                         cmon_short_str * _err_msg);
// CMON_API cmon_bool cmon_codegen_gen(cmon_codegen * _cg,
//                                     cmon_allocator * _alloc,
//                                     cmon_types * _types,
//                                     cmon_modules * _mods,
//                                     cmon_idx _mod_idx,
//                                     cmon_ir * _ir,
//                                     cmon_short_str * _err_msg);

// CMON_API cmon_codegen cmon_codegen_make_empty();
// CMON_API cmon_bool cmon_codegen_prepare(cmon_codegen * _cg, const char * _build_dir);
// CMON_API cmon_bool cmon_codegen_gen(cmon_codegen * _cg,
//                                     cmon_modules * _mods,
//                                     cmon_idx _mod_idx,
//                                     cmon_types * _types,
//                                     cmon_ir * _ir,
//                                     const char * _build_dir);
// CMON_API const char * cmon_codegen_err_msg(cmon_codegen * _cg);
// CMON_API void cmon_codegen_dealloc(cmon_codegen * _cg);

// CMON_API cmon_codegen cmon_codegen_make_empty();
// CMON_API cmon_bool cmon_codegen_gen(cmon_codegen * _cg, cmon_idx _mod_idx, cmon_ir * _ir);
// CMON_API const char * cmon_codegen_err_msg(cmon_codegen * _cg);
// CMON_API void cmon_codegen_dealloc(cmon_codegen * _cg);

#endif // CMON_CMON_CODEGEN_H
