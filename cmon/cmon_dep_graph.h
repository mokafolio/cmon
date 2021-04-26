#ifndef CMON_CMON_DEP_GRAPH_H
#define CMON_CMON_DEP_GRAPH_H

#include <cmon/cmon_allocator.h>

typedef struct
{
    cmon_idx * array;
    size_t count;
} cmon_dep_graph_result;

typedef struct cmon_dep_graph cmon_dep_graph;

CMON_API cmon_dep_graph * cmon_dep_graph_create(cmon_allocator * _alloc);
CMON_API void cmon_dep_graph_destroy(cmon_dep_graph * _g);
CMON_API void cmon_dep_graph_clear(cmon_dep_graph * _g);
CMON_API void cmon_dep_graph_add(cmon_dep_graph * _g, cmon_idx _item, cmon_idx * _deps, size_t _count);
CMON_API cmon_dep_graph_result cmon_dep_graph_resolve(cmon_dep_graph * _g);
CMON_API cmon_idx cmon_dep_graph_conflict_a(cmon_dep_graph * _g);
CMON_API cmon_idx cmon_dep_graph_conflict_b(cmon_dep_graph * _g);

#endif // CMON_CMON_DEP_GRAPH_H
