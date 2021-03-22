#ifndef CMON_CMON_DYN_ARR_H
#define CMON_CMON_DYN_ARR_H

#include <cmon/cmon_allocator.h>

#define cmon_dyn_arr(_type) _type *
#define cmon_dyn_arr_append(_arr, _val)                                                            \
    do                                                                                             \
    {                                                                                              \
        _cmon_dyn_arr_meta * _md = _cmon_dyn_arr_md((_arr));                                       \
        if (_md->count + 1 > _md->cap)                                                             \
        {                                                                                          \
            size_t nc = _md->cap * 2;                                                              \
            cmon_dyn_arr_reserve((_arr), nc);                                                      \
            _md = _cmon_dyn_arr_md((_arr));                                                        \
        }                                                                                          \
        (*(_arr))[_md->count++] = (_val);                                                          \
    } while (0)
#define cmon_dyn_arr_count(_arr) _cmon_dyn_arr_md(_arr)->count
#define cmon_dyn_arr_capacity(_arr) _cmon_dyn_arr_md(_arr)->cap
#define cmon_dyn_arr_clear(_arr) (_cmon_dyn_arr_md(_arr)->count = 0)
#define cmon_dyn_arr_pop(_arr) ((*_arr)[--_cmon_dyn_arr_md(_arr)->count])
#define cmon_dyn_arr_last(_arr) ((*_arr)[_cmon_dyn_arr_md(_arr)->count - 1])
#define cmon_dyn_arr_reserve(_arr, _count)                                                         \
    do                                                                                             \
    {                                                                                              \
        _cmon_dyn_arr_meta * _md = _cmon_dyn_arr_md((_arr));                                       \
        if (_md->cap < _count)                                                                     \
        {                                                                                          \
            _cmon_dyn_arr_meta _old_md = *_md;                                                     \
            void * _mem = cmon_allocator_realloc(                                                  \
                              _md->alloc,                                                          \
                              (cmon_mem_blk){                                                      \
                                  _md, sizeof(*(_arr)) * _md->cap + sizeof(_cmon_dyn_arr_meta) },  \
                              sizeof(*(_arr)) * _count + sizeof(_cmon_dyn_arr_meta))               \
                              .ptr;                                                                \
            _cmon_dyn_arr_meta * _nmd = _mem;                                                      \
            *_nmd = _old_md;                                                                       \
            _nmd->cap = _count;                                                                    \
            *(_arr) = _mem + sizeof(_cmon_dyn_arr_meta);                                           \
        }                                                                                          \
    } while (0)
#define cmon_dyn_arr_resize(_arr, _count)                                                          \
    do                                                                                             \
    {                                                                                              \
        cmon_dyn_arr_reserve((_arr), (_count));                                                    \
        _cmon_dyn_arr_md((_arr))->count = (_count);                                                \
    } while (0)
#define _cmon_dyn_arr_md(_arr)                                                                     \
    ((_cmon_dyn_arr_meta *)((void *)(*(_arr)) - sizeof(_cmon_dyn_arr_meta)))
#define cmon_dyn_arr_init(_arr, _alloc, _cap)                                                      \
    do                                                                                             \
    {                                                                                              \
        void * _mem =                                                                              \
            cmon_allocator_alloc(_alloc, sizeof(_cmon_dyn_arr_meta) + sizeof(**(_arr)) * _cap)     \
                .ptr;                                                                              \
        _cmon_dyn_arr_meta * _md = _mem;                                                           \
        _md->alloc = _alloc;                                                                       \
        _md->count = 0;                                                                            \
        _md->cap = _cap;                                                                           \
        *(_arr) = _mem + sizeof(_cmon_dyn_arr_meta);                                               \
    } while (0)
#define cmon_dyn_arr_dealloc(_arr)                                                                 \
    do                                                                                             \
    {                                                                                              \
        _cmon_dyn_arr_meta * md = _cmon_dyn_arr_md(_arr);                                          \
        cmon_allocator_free(                                                                       \
            md->alloc,                                                                             \
            (cmon_mem_blk){ md, sizeof(_cmon_dyn_arr_meta) + sizeof(*(*(_arr))) * md->cap });      \
    } while (0)
#define cmon_dyn_arr_remove(_arr, _idx)                                                            \
    do                                                                                             \
    {                                                                                              \
        size_t _i;                                                                                 \
        _cmon_dyn_arr_meta * _md = _cmon_dyn_arr_md(_arr);                                         \
        assert(_idx >= 0 && _idx < _md->count);                                                    \
        if (_md->count > _idx)                                                                     \
        {                                                                                          \
            for (_i = _idx; _i < _md->count - 1; ++_i)                                             \
            {                                                                                      \
                (*(_arr))[_i] = (*(_arr))[_i + 1];                                                 \
            }                                                                                      \
            _md->count--;                                                                          \
        }                                                                                          \
    } while (0)
#define cmon_dyn_arr_insert(_arr, _idx, _val)                                                      \
    do                                                                                             \
    {                                                                                              \
        size_t _i;                                                                                 \
        _cmon_dyn_arr_meta * _md = _cmon_dyn_arr_md(_arr);                                         \
        assert(_idx >= 0 && _idx <= _md->count);                                                   \
        cmon_dyn_arr_reserve((_arr), _md->count + 1);                                              \
        for (_i = _md->count++; _i > (_idx); --_i)                                                 \
        {                                                                                          \
            (*(_arr))[_i] = (*(_arr))[_i - 1];                                                     \
        }                                                                                          \
        (*(_arr))[(_idx)] = (_val);                                                                \
    } while (0)

typedef struct
{
    cmon_allocator * alloc;
    size_t count;
    size_t cap;
} _cmon_dyn_arr_meta;

// CMON_API size_t _cmon_dyn_arr_find_impl(
//     void * _arr, size_t _count, void * _val, size_t _elem_size);

// CMON_API cmon_bool _cmon_dyn_arr_append_if_unique_impl(
//     void * _arr, size_t _count, void * _val, size_t _elem_size);

#endif // CMON_CMON_DYN_ARR_H
