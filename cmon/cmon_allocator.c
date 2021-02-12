#include <cmon/cmon_allocator.h>

void cmon_allocator_dealloc(cmon_allocator * _alloc)
{
    if (_alloc->shutdown_fn)
        _alloc->shutdown_fn(_alloc->user_data);
}

cmon_mem_blk cmon_allocator_alloc(cmon_allocator * _alloc, size_t _byte_count)
{
    return _alloc->alloc_fn(_byte_count, _alloc->user_data);
}

cmon_mem_blk cmon_allocator_realloc(cmon_allocator * _alloc,
                                     cmon_mem_blk _blk,
                                     size_t _byte_count)
{
    return _alloc->realloc_fn(_blk, _byte_count, _alloc->user_data);
}

cmon_mem_blk cmon_allocator_alloc_od(cmon_allocator * _alloc, size_t _byte_count)
{
    cmon_mem_blk ret = cmon_allocator_alloc(_alloc, _byte_count);
    if(!ret.ptr)
    {
        fprintf(stderr, "Fatal: failed to allocate %zu bytes.\n", _byte_count);
        abort();
    }
    return ret;
}

cmon_mem_blk cmon_allocator_realloc_od(cmon_allocator * _alloc, cmon_mem_blk _old, size_t _byte_count)
{
    cmon_mem_blk ret = cmon_allocator_realloc(_alloc, _old, _byte_count);
    if(!ret.ptr)
    {
        fprintf(stderr, "Fatal: failed to reallocate %zu bytes.\n", _byte_count);
        abort();
    }
    return ret;
}

void cmon_allocator_free(cmon_allocator * _alloc, cmon_mem_blk _blk)
{
    _alloc->free_fn(_blk, _alloc->user_data);
}

static inline cmon_mem_blk mallocator_alloc(size_t _s, void * _user_data)
{
    return (cmon_mem_blk){ CMON_MALLOC(_s), _s };
}

static inline cmon_mem_blk mallocator_realloc(cmon_mem_blk _blk, size_t _s, void * _user_data)
{
    return (cmon_mem_blk){ CMON_REALLOC(_blk.ptr, _s), _s };
}

static inline void mallocator_free(cmon_mem_blk _blk, void * _user_data)
{
    CMON_FREE(_blk.ptr);
}

cmon_allocator cmon_mallocator_make()
{
    cmon_allocator ret;
    ret.alloc_fn = mallocator_alloc;
    ret.realloc_fn = mallocator_realloc;
    ret.free_fn = mallocator_free;
    ret.shutdown_fn = NULL;
    return ret;
}
