#ifndef CMON_CMON_HASHMAP_H
#define CMON_CMON_HASHMAP_H

#include <cmon/cmon_allocator.h>

struct cmon_hashmap_node;
typedef struct cmon_hashmap_node cmon_hashmap_node;

struct cmon_hashmap_node
{
    size_t hash;
    void * key;
    void * value;
    cmon_hashmap_node * next;
};

typedef cmon_bool (*cmon_hashmap_cmp_fn)(const void *, const void *, size_t, void *);

typedef struct
{
    cmon_allocator * alloc;
    cmon_hashmap_node ** buckets;
    size_t bucket_count, node_count;
    cmon_hashmap_cmp_fn cmp_fn;
    void * cmp_user_data;
    size_t node_byte_count; // how many bytes does a node take? Needed for cmon_allocator_free
} cmon_hashmap_base;

typedef struct
{
    size_t bucket_idx;
    cmon_hashmap_node * node;
} cmon_hashmap_iter_t;

#define cmon_hashmap(K, V)                                                                         \
    struct                                                                                         \
    {                                                                                              \
        cmon_hashmap_base base;                                                                    \
        V * ref;                                                                                   \
        V tmp;                                                                                     \
        K tmp_key;                                                                                 \
        uint64_t (*hash_fn)(K);                                                                    \
    }
#define cmon_hashmap_init(_m, _alloc, _hash_fn, _cmp_fn, _cmp_user_data)                           \
    (memset((_m), 0, sizeof(*(_m))),                                                               \
     (_m)->base.alloc = (_alloc),                                                                  \
     (_m)->base.cmp_fn = (_cmp_fn),                                                                \
     (_m)->base.cmp_user_data = (_cmp_user_data),                                                  \
     (_m)->hash_fn = (_hash_fn))
#define cmon_hashmap_dealloc(_m) _cmon_hashmap_deinit(&(_m)->base)
#define cmon_hashmap_get(_m, _key)                                                                 \
    ((_m)->tmp_key = (_key),                                                                       \
     (_m)->ref = _cmon_hashmap_get(                                                                \
         &(_m)->base, (_m)->hash_fn((_m)->tmp_key), &(_m)->tmp_key, sizeof((_m)->tmp_key)))
#define cmon_hashmap_set(_m, _key, _value)                                                         \
    ((_m)->tmp_key = (_key),                                                                       \
     (_m)->tmp = (_value),                                                                         \
     _cmon_hashmap_set(&(_m)->base,                                                                \
                       (_m)->hash_fn((_m)->tmp_key),                                               \
                       &(_m)->tmp_key,                                                             \
                       sizeof((_m)->tmp_key),                                                      \
                       &(_m)->tmp,                                                                 \
                       sizeof((_m)->tmp)))
#define cmon_hashmap_remove(_m, _key)                                                              \
    ((_m)->tmp_key = (_key),                                                                       \
     _cmon_hashmap_remove(                                                                         \
         &(_m)->base, (_m)->hash_fn((_m)->tmp_key), &(_m)->tmp_key, sizeof((_m)->tmp_key)))
#define cmon_hashmap_count(_m) ((_m)->base.node_count)
#define cmon_hashmap_bucket_count(_m) ((_m)->base.bucket_count)
#define cmon_hashmap_iter(_m) _cmon_hashmap_iter()
#define cmon_hashmap_iter_value(_m, _iter) *((_m)->ref = (_iter)->node->value)
#define cmon_hashmap_next(_m, _iter) _cmon_hashmap_next(&(_m)->base, _iter)

CMON_API void _cmon_hashmap_deinit(cmon_hashmap_base * _m);
CMON_API void * _cmon_hashmap_get(cmon_hashmap_base * _m,
                                  size_t _hash,
                                  const void * _key_ref,
                                  size_t _ksize);
CMON_API cmon_bool _cmon_hashmap_set(cmon_hashmap_base * _m,
                                     size_t _hash,
                                     const void * _key_ref,
                                     size_t _ksize,
                                     void * _value,
                                     size_t _vsize);
CMON_API cmon_bool _cmon_hashmap_remove(cmon_hashmap_base * _m,
                                        size_t _hash,
                                        const void * _key_ref,
                                        size_t _ksize);
CMON_API cmon_hashmap_iter_t _cmon_hashmap_iter();
CMON_API void * _cmon_hashmap_next(cmon_hashmap_base * _m, cmon_hashmap_iter_t * _iter);

CMON_API uint64_t _cmon_str_range_hash(const char * _begin, const char * _end);
CMON_API uint64_t _cmon_str_hash(const char * _str);
CMON_API uint64_t _cmon_ptr_hash(const void * _ptr);
CMON_API uint64_t _cmon_integer_hash(uint64_t _i);
CMON_API cmon_bool _cmon_str_cmp(const void * _stra,
                                 const void * _strb,
                                 size_t _byte_count,
                                 void *);
CMON_API cmon_bool _cmon_default_cmp(const void * _a, const void * _b, size_t _byte_count, void *);

// helpers to initialize hash maps with certain key hash functions
#define cmon_hashmap_str_key_init(_m, _alloc)                                                      \
    cmon_hashmap_init((_m), (_alloc), _cmon_str_hash, _cmon_str_cmp, NULL)
#define cmon_hashmap_ptr_key_init(_m, _alloc)                                                      \
    cmon_hashmap_init((_m), (_alloc), _cmon_ptr_hash, _cmon_default_cmp, NULL)
#define cmon_hashmap_int_key_init(_m, _alloc)                                                      \
    cmon_hashmap_init((_m), (_alloc), _cmon_integer_hash, _cmon_default_cmp, NULL)

#endif // CMON_CMON_HASHMAP_H
