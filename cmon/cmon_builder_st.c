#include <cmon/cmon_builder_st.h>
#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_resolver.h>

typedef struct
{
    cmon_tokens * tokens;
    cmon_parser * parser;
    cmon_ast * ast;
    cmon_idx src_file_idx;
} _per_file_data;

typedef struct
{
    cmon_dyn_arr(_per_file_data) file_data;
    cmon_resolver * resolver;
} _per_module_data;

typedef struct cmon_builder_st
{
    cmon_allocator * alloc;
    size_t max_errors;
    cmon_src * src;
    cmon_modules * mods;
    cmon_dyn_arr(_per_module_data) mod_data;
    cmon_symbols * symbols;
    cmon_types * types;
    cmon_dyn_arr(cmon_idx) dep_buf;
    cmon_dep_graph * dep_graph;
    cmon_dyn_arr(cmon_err_report) errs;
} cmon_builder_st;

cmon_builder_st * cmon_builder_st_create(cmon_allocator * _alloc,
                                         size_t _max_errors,
                                         cmon_src * _src,
                                         cmon_modules * _mods)
{
    cmon_builder_st * ret = CMON_CREATE(_alloc, cmon_builder_st);
    ret->alloc = _alloc;
    ret->max_errors = _max_errors;
    ret->src = _src;
    ret->mods = _mods;
    cmon_dyn_arr_init(&ret->mod_data, _alloc, cmon_modules_count(_mods));
    ret->symbols = cmon_symbols_create(_alloc, _src, _mods);
    ret->types = cmon_types_create(_alloc, _mods);
    cmon_dyn_arr_init(&ret->dep_buf, _alloc, 4);
    ret->dep_graph = cmon_dep_graph_create(_alloc);
    cmon_dyn_arr_init(&ret->errs, _alloc, 8);
    return ret;
}

void cmon_builder_st_destroy(cmon_builder_st * _b)
{
    if (!_b)
        return;

    cmon_dyn_arr_dealloc(&_b->errs);
    cmon_dep_graph_destroy(_b->dep_graph);
    cmon_dyn_arr_dealloc(&_b->dep_buf);
    cmon_types_destroy(_b->types);
    cmon_symbols_destroy(_b->symbols);
    size_t i, j;
    for (i = 0; i < cmon_dyn_arr_count(&_b->mod_data); ++i)
    {
        for (j = 0; j < cmon_dyn_arr_count(&_b->mod_data[i].file_data); ++j)
        {
            cmon_parser_destroy(_b->mod_data[i].file_data[j].parser);
            cmon_tokens_destroy(_b->mod_data[i].file_data[j].tokens);
        }
        cmon_resolver_destroy(_b->mod_data[i].resolver);
    }
    cmon_dyn_arr_dealloc(&_b->mod_data);
    CMON_DESTROY(_b->alloc, _b);
}

static inline void _add_resolver_errors(cmon_builder_st * _b, cmon_resolver * _r)
{
    cmon_err_report * errs;
    size_t count, i;
    cmon_resolver_errors(_r, &errs, &count);
    for (i = 0; i < count; ++i)
    {
        cmon_dyn_arr_append(&_b->errs, errs[i]);
    }
}

cmon_bool cmon_builder_st_build(cmon_builder_st * _b)
{
    size_t i, j;

    // setup all the things needed per module
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _per_module_data mod_data;
        mod_data.resolver = cmon_resolver_create(_b->alloc, _b->max_errors);
        cmon_dyn_arr_init(&mod_data.file_data, _b->alloc, cmon_modules_src_file_count(_b->mods, i));
        // setup everything needed per file
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            cmon_err_report err = cmon_err_report_make_empty();
            _per_file_data pfd;
            pfd.src_file_idx = cmon_modules_src_file(_b->mods, i, j);
            // tokenize the modules files right here
            pfd.tokens = cmon_tokenize(_b->alloc, _b->src, pfd.src_file_idx, &err);
            pfd.parser = cmon_parser_create(_b->alloc);
            pfd.ast = NULL;

            // buffer potential tokenize errors
            if (!cmon_err_report_is_empty(&err))
                cmon_dyn_arr_append(&_b->errs, err);

            cmon_dyn_arr_append(&mod_data.file_data, pfd);
        }
        cmon_dyn_arr_append(&_b->mod_data, mod_data);
    }

    // return if files failed to tokenize
    if (cmon_dyn_arr_count(&_b->errs))
    {
        return cmon_true;
    }

    // parse all the files
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _per_module_data * pmd = &_b->mod_data[i];

        // setup everything needed per file
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            _per_file_data * pfd = &pmd->file_data[j];
            pfd->ast = cmon_parser_parse(pfd->parser, _b->src, pfd->src_file_idx, pfd->tokens);
            if (!pfd->ast)
            {
                cmon_dyn_arr_append(&_b->errs, cmon_parser_err(pfd->parser));
            }
            else
            {
                cmon_src_set_tokens(_b->src, pfd->src_file_idx, pfd->tokens);
                cmon_src_set_ast(_b->src, pfd->src_file_idx, pfd->ast);
            }
        }
    }

    // return if files failed to parse
    if (cmon_dyn_arr_count(&_b->errs))
    {
        return cmon_true;
    }

    // resolve all the top level names for each module to determine which other modules they depend
    // on
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _per_module_data * pmd = &_b->mod_data[i];
        cmon_resolver_set_input(pmd->resolver, _b->src, _b->types, _b->symbols, _b->mods, i);
        for (j = 0; j < cmon_modules_dep_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_top_lvl_pass(pmd->resolver, j))
                _add_resolver_errors(_b, pmd->resolver);
        }
    }

    // return if modules errored during top level pass.
    if (cmon_dyn_arr_count(&_b->errs))
    {
        return cmon_true;
    }

    // resolve dependency order between modules
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        cmon_dyn_arr_clear(&_b->dep_buf);
        for (j = 0; j < cmon_modules_dep_count(_b->mods, i); ++j)
        {
            cmon_dyn_arr_append(&_b->dep_buf, cmon_modules_dep(_b->mods, i, j));
        }

        cmon_dep_graph_add(_b->dep_graph, i, &_b->dep_buf[0], cmon_dyn_arr_count(&_b->dep_buf));
    }

    cmon_dep_graph_result result = cmon_dep_graph_resolve(_b->dep_graph);
    if (!result.array)
    {
        cmon_idx a, b;
        a = cmon_dep_graph_conflict_a(_b->dep_graph);
        b = cmon_dep_graph_conflict_b(_b->dep_graph);
        assert(0);
        if (a != b)
        {
            // _fr_err(fr,
            //         cmon_types_name_tok(_r->types, a),
            //         "circular dependency between types '%s' and '%s'",
            //         cmon_types_name(_r->types, a),
            //         cmon_types_name(_r->types, b));
        }
        else
        {
            // _fr_err(fr,
            //         cmon_types_name_tok(_r->types, a),
            //         "recursive type '%s'",
            //         cmon_types_name(_r->types, a));
        }
        return cmon_true;
    }

    // resolve each module
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _per_module_data * pmd = &_b->mod_data[i];
        if (cmon_resolver_globals_pass(pmd->resolver))
        {
            _add_resolver_errors(_b, pmd->resolver);
            return cmon_true;
        }

        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_usertypes_pass(pmd->resolver, j))
            {
                _add_resolver_errors(_b, pmd->resolver);
                return cmon_true;
            }
        }

        if (cmon_resolver_circ_pass(pmd->resolver))
        {
            _add_resolver_errors(_b, pmd->resolver);
            return cmon_true;
        }

        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_main_pass(pmd->resolver, j))
            {
                _add_resolver_errors(_b, pmd->resolver);
                return cmon_true;
            }
        }
    }

    return cmon_false;
}

cmon_bool cmon_builder_st_errors(cmon_builder_st * _b,
                                 cmon_err_report ** _out_errs,
                                 size_t * _out_count)
{
    *_out_errs = _b->errs;
    *_out_count = cmon_dyn_arr_count(&_b->errs);
    return cmon_dyn_arr_count(&_b->errs) > 0;
}
