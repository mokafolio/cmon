#ifndef CMON_CMON_TINI_H
#define CMON_CMON_TINI_H

#include <cmon/cmon_allocator.h>

typedef enum
{
    cmon_tinik_pair,
    cmon_tinik_string,
    cmon_tinik_obj,
    cmon_tinik_array
} cmon_tinik;

typedef struct
{
    size_t line;
    size_t line_offset;
    char filename[CMON_FILENAME_MAX];
    char msg[CMON_ERR_MSG_MAX];
} cmon_tini_err;

typedef struct cmon_tini cmon_tini;

CMON_API cmon_tini * cmon_tini_parse(cmon_allocator * _alloc, const char * _name, const char * _txt, cmon_tini_err * _out_err);
CMON_API cmon_tini * cmon_tini_parse_file(cmon_allocator * _alloc, const char * _path, cmon_tini_err * _out_err);
CMON_API void cmon_tini_destroy(cmon_tini * _t);

CMON_API cmon_tinik cmon_tini_kind(cmon_tini * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tini_root_obj(cmon_tini * _t);
CMON_API cmon_idx cmon_tini_obj_find(cmon_tini * _t, cmon_idx _idx, const char * _key);
CMON_API size_t cmon_tini_child_count(cmon_tini * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tini_child(cmon_tini * _t, cmon_idx _idx, size_t _child_idx);
CMON_API cmon_str_view cmon_tini_pair_key(cmon_tini * _t, cmon_idx _idx);
CMON_API cmon_idx cmon_tini_pair_value(cmon_tini * _t, cmon_idx _idx);
CMON_API cmon_str_view cmon_tini_string(cmon_tini * _t, cmon_idx _idx);

#endif //CMON_CMON_TINI_H
