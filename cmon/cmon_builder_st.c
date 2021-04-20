#include <cmon/cmon_builder_st.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_dep_graph.h>

typedef struct cmon_builder_st
{

} cmon_builder_st;

cmon_builder_st * cmon_builder_st_create(cmon_allocator * _alloc, size_t _max_errors, cmon_src * _src, cmon_modules * _mods)
{

}

void cmon_builder_st_destroy(cmon_builder_st * _b)
{

}

cmon_bool cmon_builder_st_build(cmon_builder_st * _b)
{
    //01. resolve dependency order between modules.

    //02. for each module tokenize/parse all files

    //03. resolve each module
}
