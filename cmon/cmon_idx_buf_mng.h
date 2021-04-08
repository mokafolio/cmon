#ifndef CMON_CMON_IDX_BUF_MNG_H
#define CMON_CMON_IDX_BUF_MNG_H

#include <cmon/cmon_allocator.h>

typedef struct cmon_idx_buf_mng cmon_idx_buf_mng;

CMON_API cmon_idx_buf_mng * cmon_idx_buf_mng_create(cmon_allocator * _alloc);
CMON_API void cmon_idx_buf_mng_destroy(cmon_idx_buf_mng * _m);
CMON_API cmon_idx cmon_idx_buf_mng_get(cmon_idx_buf_mng * _m);
CMON_API void cmon_idx_buf_mng_return(cmon_idx_buf_mng * _m, cmon_idx _buf_idx);
CMON_API void cmon_idx_buf_append(cmon_idx_buf_mng * _m, cmon_idx _buf_idx, cmon_idx _val);
CMON_API cmon_idx * cmon_idx_buf_ptr(cmon_idx_buf_mng * _m, cmon_idx _buf_idx);
CMON_API size_t cmon_idx_buf_count(cmon_idx_buf_mng * _m, cmon_idx _buf_idx);
CMON_API cmon_idx cmon_idx_buf_at(cmon_idx_buf_mng * _m, cmon_idx _buf_idx, cmon_idx _at);
CMON_API void cmon_idx_buf_clear(cmon_idx_buf_mng * _m, cmon_idx _buf_idx);

#endif //CMON_CMON_IDX_BUF_MNG_H
