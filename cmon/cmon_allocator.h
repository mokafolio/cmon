#ifndef CMON_CMON_ALLOCATOR_H
#define CMON_CMON_ALLOCATOR_H

#define CMON_CREATE(_alloc, _type) cmon_allocator_alloc(_alloc, sizeof(_type)).ptr
#define CMON_DESTROY(_alloc, _ptr) cmon_allocator_free(_alloc, (cmon_mem_blk){_ptr, sizeof(*_ptr)})

#include <cmon/cmon_base.h>

typedef struct cmon_allocator cmon_allocator;

typedef struct
{
    void * ptr;
    size_t byte_count;
} cmon_mem_blk;

typedef cmon_mem_blk (*cmon_alloc_fn)(size_t, void *);
typedef cmon_mem_blk (*cmon_realloc_fn)(cmon_mem_blk, size_t, void *);
typedef void (*cmon_free_fn)(cmon_mem_blk, void *);
typedef void (*cmon_alloc_shutdown_fn)(void *);

typedef struct cmon_allocator
{
    void * user_data;
    cmon_alloc_fn alloc_fn;
    cmon_realloc_fn realloc_fn;
    cmon_free_fn free_fn;
    cmon_alloc_shutdown_fn shutdown_fn;
} cmon_allocator;

// releases all memory associated with an allocator (calls its cmon_alloc_shutdown_fn)
CMON_API void cmon_allocator_dealloc(cmon_allocator * _alloc);
CMON_API cmon_mem_blk cmon_allocator_alloc(cmon_allocator * _alloc, size_t _byte_count);
CMON_API cmon_mem_blk cmon_allocator_realloc(cmon_allocator * _alloc, cmon_mem_blk _old, size_t _byte_count);
CMON_API cmon_mem_blk cmon_allocator_alloc_od(cmon_allocator * _alloc, size_t _byte_count);
CMON_API cmon_mem_blk cmon_allocator_realloc_od(cmon_allocator * _alloc, cmon_mem_blk _old, size_t _byte_count);
CMON_API void cmon_allocator_free(cmon_allocator * _alloc, cmon_mem_blk _blk);

// creates an allocator that simply uses malloc and free
CMON_API cmon_allocator cmon_mallocator_make();

#endif // CMON_CMON_ALLOCATOR2_H
