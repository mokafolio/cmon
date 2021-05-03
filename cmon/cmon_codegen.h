#ifndef CMON_CMON_CODEGEN_H
#define CMON_CMON_CODEGEN_H

//abstract code generation interface
typedef cmon_bool (*cmon_codegen_fn)(void*, cmon_ir*);
typedef void (*cmon_codegen_shutdown_fn)(void *);
typedef const char * (*cmon_codegen_err_msg_fn)(void *);

typedef struct
{
    void * obj;
    cmon_codegen_fn fn;
    cmon_codegen_shutdown_fn shutdown_fn;
    cmon_codegen_err_msg_fn err_msg_fn;
} cmon_codegen;

CMON_API cmon_codegen cmon_codegen_make_empty();
CMON_API cmon_bool cmon_codegen_gen(cmon_codegen * _cg, cmon_ir * _ir);
CMON_API const char * cmon_codegen_err_msg(cmon_codegen * _cg);
CMON_API void cmon_codegen_dealloc(cmon_codegen * _cg);

#endif //CMON_CMON_CODEGEN_H
