#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_idx_buf_mng.h>

typedef struct cmon_idx_buf_mng
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_dyn_arr(cmon_idx)) bufs;
    cmon_dyn_arr(cmon_idx) free_bufs;
} cmon_idx_buf_mng;

cmon_idx_buf_mng * cmon_idx_buf_mng_create(cmon_allocator * _alloc)
{
    cmon_idx_buf_mng * ret = CMON_CREATE(_alloc, cmon_idx_buf_mng);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->bufs, _alloc, 8);
    cmon_dyn_arr_init(&ret->free_bufs, _alloc, 8);
    return ret;
}

void cmon_idx_buf_mng_destroy(cmon_idx_buf_mng * _m)
{
    size_t i;
    cmon_dyn_arr_dealloc(&_m->free_bufs);
    for (i = 0; i < cmon_dyn_arr_count(&_m->bufs); ++i)
    {
        cmon_dyn_arr_dealloc(&_m->bufs[i]);
    }

    cmon_dyn_arr_dealloc(&_m->bufs);
    CMON_DESTROY(_m->alloc, _m);
}

cmon_idx cmon_idx_buf_mng_get(cmon_idx_buf_mng * _m)
{
    cmon_idx ret;
    if (!cmon_dyn_arr_count(&_m->free_bufs))
    {
        cmon_dyn_arr(cmon_idx) buf;
        cmon_dyn_arr_init(&buf, _m->alloc, 16);
        cmon_dyn_arr_append(&_m->bufs, buf);
        return cmon_dyn_arr_count(&_m->bufs) - 1;
    }
    ret = cmon_dyn_arr_pop(&_m->free_bufs);
    cmon_dyn_arr_clear(&_m->bufs[ret]);
    return ret;
}

void cmon_idx_buf_mng_return(cmon_idx_buf_mng * _m, cmon_idx _buf_idx)
{
    cmon_dyn_arr_append(&_m->free_bufs, _buf_idx);
}

void cmon_idx_buf_append(cmon_idx_buf_mng * _m, cmon_idx _buf_idx, cmon_idx _val)
{
    cmon_dyn_arr_append(&_m->bufs[_buf_idx], _val);
}

void cmon_idx_buf_set(cmon_idx_buf_mng * _m, cmon_idx _buf_idx, cmon_idx _at, cmon_idx _val)
{
    _m->bufs[_buf_idx][_at] = _val;
}

cmon_idx * cmon_idx_buf_ptr(cmon_idx_buf_mng * _m, cmon_idx _buf_idx)
{
    return _m->bufs[_buf_idx];
}

size_t cmon_idx_buf_count(cmon_idx_buf_mng * _m, cmon_idx _buf_idx)
{
    return cmon_dyn_arr_count(&_m->bufs[_buf_idx]);
}

cmon_idx cmon_idx_buf_at(cmon_idx_buf_mng * _m, cmon_idx _buf_idx, cmon_idx _at)
{
    assert(_at < cmon_idx_buf_count(_m, _buf_idx));
    return _m->bufs[_buf_idx][_at];
}

void cmon_idx_buf_clear(cmon_idx_buf_mng * _m, cmon_idx _buf_idx)
{
    cmon_dyn_arr_clear(&_m->bufs[_buf_idx]);
}
