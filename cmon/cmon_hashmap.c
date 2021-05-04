#include <cmon/cmon_hashmap.h>
#include <stdalign.h>

static cmon_hashmap_node * _create_node(cmon_hashmap_base * _m,
                                        size_t _hash,
                                        const void * _key_ref,
                                        size_t _ksize,
                                        void * _value,
                                        size_t _vsize)
{
    cmon_hashmap_node * node;
    //@TODO: Cache all these allocation things in the map_base?
    size_t hash = _hash;
    size_t align_hlpr = alignof(void *) - 1;
    size_t key_padding = (-sizeof(cmon_hashmap_node) & align_hlpr);
    size_t koff = sizeof(cmon_hashmap_node) + key_padding;
    size_t voff = koff + _ksize;
    voff += (-voff & align_hlpr);
    if (_m->node_byte_count == 0)
    {
        _m->node_byte_count = voff + _vsize;
    }
    node = cmon_allocator_alloc_od(_m->alloc, _m->node_byte_count).ptr;
    node->hash = hash;
    node->key = ((char *)node) + koff;
    node->value = ((char *)node) + voff;
    node->next = NULL;
    memcpy(node->key, _key_ref, _ksize);
    memcpy(node->value, _value, _vsize);
    return node;
}

static inline size_t _bucket_idx(cmon_hashmap_base * _m, size_t _hash)
{
    // bucket count needs to be power of 2, based on Java implementation
    return _hash & (_m->bucket_count - 1);
}

static inline void _add_node(cmon_hashmap_base * _m, cmon_hashmap_node * _node)
{
    size_t idx = _bucket_idx(_m, _node->hash);
    _node->next = _m->buckets[idx];
    _m->buckets[idx] = _node;
}

static void _resize(cmon_hashmap_base * _m, size_t _count)
{
    cmon_hashmap_node *nodes, *node, *next;
    cmon_hashmap_node ** buckets;
    size_t i;

    // Chain all nodes together
    nodes = NULL;
    i = _m->bucket_count;
    while (i--)
    {
        node = (_m->buckets)[i];
        while (node)
        {
            next = node->next;
            node->next = nodes;
            nodes = node;
            node = next;
        }
    }

    // Reset buckets
    buckets = cmon_allocator_realloc_od(
                  _m->alloc,
                  (cmon_mem_blk){ _m->buckets, sizeof(*_m->buckets) * _m->bucket_count },
                  sizeof(*_m->buckets) * _count)
                  .ptr;

    _m->buckets = buckets;
    _m->bucket_count = _count;

    memset(_m->buckets, 0, sizeof(*_m->buckets) * _m->bucket_count);
    /* Re-add nodes to buckets */
    node = nodes;
    while (node)
    {
        next = node->next;
        _add_node(_m, node);
        node = next;
    }
}

static cmon_hashmap_node ** _getref(cmon_hashmap_base * _m,
                                    size_t _hash,
                                    const void * _key_ref,
                                    size_t _key_size)
{
    cmon_hashmap_node ** next;
    if (_m->bucket_count > 0)
    {
        printf("aswoop %lu %lu\n", _m->bucket_count, _bucket_idx(_m, _hash));
        size_t hash = _hash;
        next = &_m->buckets[_bucket_idx(_m, hash)];
        while (*next)
        {
            printf("woop\n");
            if ((*next)->hash == hash && _m->cmp_fn(_key_ref, (*next)->key, _key_size, _m->cmp_user_data))
            {
                printf("FOOOUN!\n");
                return next;
            }
            next = &(*next)->next;
        }
    }
    return NULL;
}

void _cmon_hashmap_deinit(cmon_hashmap_base * _m)
{
    cmon_hashmap_node *next, *node;
    size_t i;

    i = _m->bucket_count;
    while (i--)
    {
        node = _m->buckets[i];
        while (node)
        {
            next = node->next;
            cmon_allocator_free(_m->alloc, (cmon_mem_blk){ node, _m->node_byte_count });
            node = next;
        }
    }
    cmon_allocator_free(_m->alloc,
                        (cmon_mem_blk){ _m->buckets, sizeof(*_m->buckets) * _m->bucket_count });
}

void * _cmon_hashmap_get(cmon_hashmap_base * _m,
                         size_t _hash,
                         const void * _key_ref,
                         size_t _key_size)
{
    cmon_hashmap_node ** n = _getref(_m, _hash, _key_ref, _key_size);
    return n ? (*n)->value : NULL;
}

cmon_bool _cmon_hashmap_set(cmon_hashmap_base * _m,
                            size_t _hash,
                            const void * _key_ref,
                            size_t _ksize,
                            void * _value,
                            size_t _vsize)
{
    size_t n;
    cmon_hashmap_node **next, *node;
    /* Find & replace existing node */
    next = _getref(_m, _hash, _key_ref, _ksize);
    if (next)
    {
        printf("REPLACEING\n");
        memcpy((*next)->value, _value, _vsize);
        return cmon_true;
    }

    /* Add new node */
    node = _create_node(_m, _hash, _key_ref, _ksize, _value, _vsize);
    if (_m->node_count >= _m->bucket_count)
    {
        n = (_m->bucket_count > 0) ? (_m->bucket_count << 1) : 1;
        _resize(_m, n);
    }
    _add_node(_m, node);
    ++_m->node_count;
    return cmon_false;
}

cmon_bool _cmon_hashmap_remove(cmon_hashmap_base * _m,
                               size_t _hash,
                               const void * _key_ref,
                               size_t _key_size)
{
    cmon_hashmap_node * node;
    cmon_hashmap_node ** next = _getref(_m, _hash, _key_ref, _key_size);
    if (next)
    {
        node = *next;
        *next = (*next)->next;
        cmon_allocator_free(_m->alloc, (cmon_mem_blk){ node, _m->node_byte_count });
        --_m->node_count;
        return cmon_true;
    }
    return cmon_false;
}

cmon_hashmap_iter_t _cmon_hashmap_iter()
{
    return (cmon_hashmap_iter_t){ -1, NULL };
}

void * _cmon_hashmap_next(cmon_hashmap_base * _m, cmon_hashmap_iter_t * _iter)
{
    if (_iter->node)
    {
        _iter->node = _iter->node->next;
        if (_iter->node == NULL)
            goto next_bucket;
    }
    else
    {
    next_bucket:
        do
        {
            if (++_iter->bucket_idx >= _m->bucket_count)
            {
                return NULL;
            }
            _iter->node = _m->buckets[_iter->bucket_idx];
        } while (_iter->node == NULL);
    }
    return _iter->node->key;
}

uint64_t _cmon_ptr_hash(const void * _ptr)
{
    // in theory this should be a bad idea due to alignment and distribution. In practice it seems
    // to be pretty fast, and std::hash on GCC is identical:
    // https://stackoverflow.com/questions/20953390/what-is-the-fastest-hash-function-for-pointers
    printf("PTR HASH %lu\n", (uint64_t)_ptr);
    return (uint64_t)_ptr;
}

//@TODO: add 32 bit version for 32 bit platforms?
// MurmurHash2, by Austin Appleby

// Microsoft Visual Studio

#if defined(_MSC_VER)

#define BIG_CONSTANT(x) (x)

// Other compilers

#else // defined(_MSC_VER)

#define BIG_CONSTANT(x) (x##LLU)

#endif // !defined(_MSC_VER)

uint64_t _murmur2_64(const void * _key, int _len, unsigned int _seed)
{
    const uint64_t m = BIG_CONSTANT(0xc6a4a7935bd1e995);
    const int r = 47;

    uint64_t h = _seed ^ (_len * m);

    const uint64_t * data = (const uint64_t *)_key;
    const uint64_t * end = data + (_len / 8);

    while (data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char * data2 = (const unsigned char *)data;

    switch (_len & 7)
    {
    case 7:
        h ^= ((uint64_t)data2[6]) << 48;
    case 6:
        h ^= ((uint64_t)data2[5]) << 40;
    case 5:
        h ^= ((uint64_t)data2[4]) << 32;
    case 4:
        h ^= ((uint64_t)data2[3]) << 24;
    case 3:
        h ^= ((uint64_t)data2[2]) << 16;
    case 2:
        h ^= ((uint64_t)data2[1]) << 8;
    case 1:
        h ^= ((uint64_t)data2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

uint64_t _cmon_str_hash(const char * _str)
{
    return _cmon_str_range_hash(_str, _str + strlen(_str));
}

uint64_t _cmon_str_range_hash(const char * _begin, const char * _end)
{
    //@TODO: randomize seed on program start? Not really needed I guess?
    return _murmur2_64(_begin, _end - _begin, 0);
}

uint64_t _cmon_integer_hash(uint64_t _i)
{
    return _i;
}

cmon_bool _cmon_str_cmp(const void * _stra,
                        const void * _strb,
                        size_t _byte_count,
                        void * _user_data)
{
    printf("cmp %s %s\n", *(const char **)_stra, *(const char **)_strb);
    return strcmp(*(const char **)_stra, *(const char **)_strb) == 0;
}

cmon_bool _cmon_default_cmp(const void * _a, const void * _b, size_t _byte_count, void * _user_data)
{
    return memcmp(_a, _b, _byte_count) == 0;
}
