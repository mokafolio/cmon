#ifndef CMON_CMON_CODEGEN_C_H
#define CMON_CMON_CODEGEN_C_H

#include <cmon/cmon_codegen.h>
#include <cmon/cmon_modules.h>
#include <cmon/cmon_types.h>

CMON_API cmon_codegen cmon_codegen_c_make(cmon_allocator * _alloc, cmon_modules * _mods, cmon_types * _types, const char * _build_dir);

#endif //CMON_CMON_CODEGEN_C_H
