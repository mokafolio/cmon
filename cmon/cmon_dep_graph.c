#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>

typedef enum
{
    cmon_dep_graph_mark_none,
    cmon_dep_graph_mark_tmp,
    cmon_dep_graph_mark_perm
} cmon_dep_graph_mark;

typedef struct cmon_dep_graph_node
{
    cmon_idx data;
    cmon_dyn_arr(struct cmon_dep_graph_node *) deps;
    cmon_dep_graph_mark mark;
} cmon_dep_graph_node;

typedef struct cmon_dep_graph
{
    cmon_allocator * alloc;
    cmon_dyn_arr(cmon_dep_graph_node *) nodes;
    cmon_dyn_arr(cmon_dep_graph_node *) unresolved;
    cmon_dyn_arr(cmon_dep_graph_node *) unmarked;
    cmon_dyn_arr(cmon_idx) resolved;

    // thse are set to the first nodes with a cyclic dependency
    cmon_idx conflict_a, conflict_b;
    size_t resolved_count;
} cmon_dep_graph;

static cmon_dep_graph_node * _find_node(cmon_dep_graph * _g, cmon_idx _data)
{
    //@TODO: Add map/hash based lookup?
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_g->unresolved); ++i)
    {
        cmon_dep_graph_node * n = _g->unresolved[i];
        if (n->data == _data)
            return n;
    }
    return NULL;
}

static cmon_dep_graph_node * _find_or_create_node(cmon_dep_graph * _g,
                                                  cmon_idx _data,
                                                  cmon_idx * _deps,
                                                  size_t _count);

static void _set_deps(cmon_dep_graph * _g,
                      cmon_dep_graph_node * _n,
                      cmon_idx * _deps,
                      size_t _count)
{
    size_t i;
    for (i = 0; i < _count; ++i)
    {
        cmon_dyn_arr_append(&_n->deps, _find_or_create_node(_g, _deps[i], NULL, 0));
    }
}

static cmon_dep_graph_node * _create_node(cmon_dep_graph * _g,
                                          cmon_idx _data,
                                          cmon_idx * _deps,
                                          size_t _count)
{
    cmon_dep_graph_node * ret = CMON_CREATE(_g->alloc, cmon_dep_graph_node);
    ret->data = _data;
    ret->mark = cmon_dep_graph_mark_none;

    cmon_dyn_arr_init(&ret->deps, _g->alloc, 4);
    cmon_dyn_arr_append(&_g->nodes, ret);
    cmon_dyn_arr_append(&_g->unresolved, ret);
    cmon_dyn_arr_append(&_g->unmarked, ret);

    if (_deps)
    {
        _set_deps(_g, ret, _deps, _count);
    }

    return ret;
}

static cmon_dep_graph_node * _find_or_create_node(cmon_dep_graph * _g,
                                                  cmon_idx _data,
                                                  cmon_idx * _deps,
                                                  size_t _count)
{
    cmon_dep_graph_node * n = _find_node(_g, _data);
    if (n)
    {
        if (_deps)
            _set_deps(_g, n, _deps, _count);
        return n;
    }
    return _create_node(_g, _data, _deps, _count);
}

cmon_dep_graph * cmon_dep_graph_create(cmon_allocator * _alloc)
{
    cmon_dep_graph * ret;
    ret = CMON_CREATE(_alloc, cmon_dep_graph);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->nodes, _alloc, 32);
    cmon_dyn_arr_init(&ret->unresolved, _alloc, 32);
    cmon_dyn_arr_init(&ret->unmarked, _alloc, 32);
    cmon_dyn_arr_init(&ret->resolved, _alloc, 32);
    ret->resolved_count = 0;
    ret->conflict_a = ret->conflict_b = CMON_INVALID_IDX;
    return ret;
}

void cmon_dep_graph_destroy(cmon_dep_graph * _g)
{
    cmon_dep_graph_clear(_g);
    cmon_dyn_arr_dealloc(&_g->resolved);
    cmon_dyn_arr_dealloc(&_g->unmarked);
    cmon_dyn_arr_dealloc(&_g->unresolved);
    cmon_dyn_arr_dealloc(&_g->nodes);
    CMON_DESTROY(_g->alloc, _g);
}

//@TODO: add a simple free list to reuse nodes instead of deallocating?
void cmon_dep_graph_clear(cmon_dep_graph * _g)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_g->nodes); ++i)
    {
        cmon_dep_graph_node * n = _g->nodes[i];
        cmon_dyn_arr_dealloc(&n->deps);
        CMON_DESTROY(_g->alloc, n);
    }
    cmon_dyn_arr_clear(&_g->resolved);
    cmon_dyn_arr_clear(&_g->unmarked);
    cmon_dyn_arr_clear(&_g->unresolved);
    cmon_dyn_arr_clear(&_g->nodes);
}

void cmon_dep_graph_add(cmon_dep_graph * _g, cmon_idx _data, cmon_idx * _deps, size_t _count)
{
    _find_or_create_node(_g, _data, _deps, _count);
}

static cmon_bool _visit(cmon_dep_graph * _g, cmon_dep_graph_node * _n)
{
    size_t i;

    if (_n->mark == cmon_dep_graph_mark_perm)
        return cmon_false;

    //@TODO: set error message on _g
    // Graph can't be solved
    if (_n->mark == cmon_dep_graph_mark_tmp)
    {
        _g->conflict_b = _n->data;
        return cmon_true;
    }

    _n->mark = cmon_dep_graph_mark_tmp;
    _g->conflict_a = _n->data;

    for (i = 0; i < cmon_dyn_arr_count(&_g->unmarked); ++i)
    {
        if (_n == _g->unmarked[i])
        {
            cmon_dyn_arr_remove(&_g->unmarked, i);
            break;
        }
    }

    for (i = 0; i < cmon_dyn_arr_count(&_n->deps); ++i)
    {
        if (_visit(_g, _n->deps[i]))
            return cmon_true;
    }

    _n->mark = cmon_dep_graph_mark_perm;
    cmon_dyn_arr_append(&_g->resolved, _n->data);

    for (i = 0; i < cmon_dyn_arr_count(&_g->unresolved); ++i)
    {
        if (_n == _g->unresolved[i])
        {
            cmon_dyn_arr_remove(&_g->unresolved, i);
            break;
        }
    }

    return cmon_false;
}

cmon_dep_graph_result cmon_dep_graph_resolve(cmon_dep_graph * _g)
{
    cmon_bool err = cmon_false;

    while (cmon_dyn_arr_count(&_g->unresolved))
    {
        cmon_dep_graph_node * n = cmon_dyn_arr_last(&_g->unmarked);
        assert(n->mark == cmon_dep_graph_mark_none);
        if (_visit(_g, n))
        {
            err = cmon_true;
            break;
        }
    }

    if (err)
        return (cmon_dep_graph_result){ NULL, 0 };

    return (cmon_dep_graph_result){ _g->resolved, cmon_dyn_arr_count(&_g->resolved) };
}

cmon_idx cmon_dep_graph_conflict_a(cmon_dep_graph * _g)
{
    return _g->conflict_a;
}

cmon_idx cmon_dep_graph_conflict_b(cmon_dep_graph * _g)
{
    return _g->conflict_b;
}
