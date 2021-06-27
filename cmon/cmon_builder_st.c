#include <cmon/cmon_builder_st.h>
#include <cmon/cmon_dep_graph.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_err_handler.h>
#include <cmon/cmon_parser.h>
#include <cmon/cmon_resolver.h>
#include <cmon/cmon_str_builder.h>
#include <setjmp.h>

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
    cmon_ir * ir;
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
    cmon_err_handler * err_handler;
    jmp_buf err_jmp;
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
    ret->err_handler = cmon_err_handler_create(_alloc, _src, _max_errors);
    return ret;
}

void cmon_builder_st_destroy(cmon_builder_st * _b)
{
    if (!_b)
        return;

    cmon_err_handler_destroy(_b->err_handler);
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
        cmon_dyn_arr_dealloc(&_b->mod_data[i].file_data);
        cmon_resolver_destroy(_b->mod_data[i].resolver);
    }
    cmon_dyn_arr_dealloc(&_b->mod_data);
    CMON_DESTROY(_b->alloc, _b);
}

static inline void _add_resolver_errors(cmon_builder_st * _b,
                                        cmon_resolver * _r,
                                        cmon_bool _jmp_on_any_err)
{
    cmon_err_report * errs;
    size_t count, i;
    cmon_resolver_errors(_r, &errs, &count);
    for (i = 0; i < count; ++i)
    {
        cmon_err_handler_add_err(_b->err_handler, cmon_true, &errs[i]);
    }
    if (_jmp_on_any_err)
    {
        cmon_err_handler_jump(_b->err_handler, cmon_true);
    }
}

static inline void _log_status(cmon_log * _log, const char * _fmt, ...)
{
    va_list args;
    va_start(args, _fmt);
    cmon_log_write_styled_v(_log,
                            cmon_log_level_info,
                            cmon_log_color_cyan,
                            cmon_log_color_default,
                            cmon_log_style_none,
                            _fmt,
                            args);
    va_end(args);
}

cmon_bool cmon_builder_st_build(cmon_builder_st * _b,
                                cmon_codegen * _codegen,
                                const char * _build_dir,
                                cmon_log * _log)
{
    size_t i, j;

    if (setjmp(_b->err_jmp))
    {
        goto err_end;
    }
    cmon_err_handler_set_jump(_b->err_handler, &_b->err_jmp);

    _log_status(_log, "-> cmon_builder_st start build\n");

    _log_status(_log, "    01. loading and tokenizing src files\n");

    // setup all the things needed per module
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _log_status(_log, "        » %s\n", cmon_modules_path(_b->mods, i));
        _per_module_data mod_data;
        mod_data.resolver = cmon_resolver_create(_b->alloc, _b->max_errors);
        cmon_dyn_arr_init(&mod_data.file_data, _b->alloc, cmon_modules_src_file_count(_b->mods, i));
        // setup everything needed per file
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            cmon_err_report err = cmon_err_report_make_empty();
            _per_file_data pfd;
            pfd.src_file_idx = cmon_modules_src_file(_b->mods, i, j);

            _log_status(_log, "            » %s\n", cmon_src_filename(_b->src, pfd.src_file_idx));

            //@NOTE: If the src code was already set on the src file (i.e. during unit testing)
            // the cmon_src_load_code funtions is a noop.
            if (cmon_src_load_code(_b->src, pfd.src_file_idx))
            {
                cmon_err_handler_err(_b->err_handler,
                                     cmon_true,
                                     pfd.src_file_idx,
                                     CMON_INVALID_IDX,
                                     CMON_INVALID_IDX,
                                     "failed to load src file %s",
                                     cmon_src_path(_b->src, pfd.src_file_idx));
            }
            else
            {
                // tokenize the modules files right here
                pfd.tokens = cmon_tokenize(_b->alloc, _b->src, pfd.src_file_idx, &err);
                pfd.parser = cmon_parser_create(_b->alloc);
                pfd.ast = NULL;

                // buffer potential tokenize errors
                if (!cmon_err_report_is_empty(&err))
                {
                    cmon_err_handler_add_err(_b->err_handler, cmon_true, &err);
                }
            }

            cmon_dyn_arr_append(&mod_data.file_data, pfd);
        }
        cmon_dyn_arr_append(&_b->mod_data, mod_data);
    }

    // return if files failed to tokenize
    cmon_err_handler_jump(_b->err_handler, cmon_true);

    // parse all the files
    _log_status(_log, "    02. parsing src files\n");
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _log_status(_log, "        » %s\n", cmon_modules_path(_b->mods, i));
        _per_module_data * pmd = &_b->mod_data[i];

        // setup everything needed per file
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            cmon_idx src_file_idx = cmon_modules_src_file(_b->mods, i, j);

            _log_status(_log, "            » %s\n", cmon_src_filename(_b->src, src_file_idx));

            _per_file_data * pfd = &pmd->file_data[j];
            pfd->ast = cmon_parser_parse(pfd->parser, _b->src, pfd->src_file_idx, pfd->tokens);
            if (!pfd->ast)
            {
                cmon_err_report err = cmon_parser_err(pfd->parser);
                cmon_err_handler_add_err(_b->err_handler, cmon_true, &err);
            }
        }
    }

    // return if files failed to parse
    cmon_err_handler_jump(_b->err_handler, cmon_true);

    // resolve all the top level names for each module to determine which other modules they depend
    // on
    _log_status(_log, "    03. resolving top level names\n");
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        _log_status(_log, "        » %s\n", cmon_modules_path(_b->mods, i));
        _per_module_data * pmd = &_b->mod_data[i];
        cmon_resolver_set_input(pmd->resolver, _b->src, _b->types, _b->symbols, _b->mods, i);
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_top_lvl_pass(pmd->resolver, j))
            {
                _add_resolver_errors(_b, pmd->resolver, cmon_false);
            }
        }

        if (cmon_resolver_finalize_top_lvl_names(pmd->resolver))
        {
            _add_resolver_errors(_b, pmd->resolver, cmon_true);
        }
    }

    // return if modules errored during top level pass.
    cmon_err_handler_jump(_b->err_handler, cmon_true);

    //@TODO: move that somewhere else so it can be used by different build implementations
    // resolve dependency order between modules
    _log_status(_log, "    04. resolving module dependency order\n");
    for (i = 0; i < cmon_modules_count(_b->mods); ++i)
    {
        cmon_dyn_arr_clear(&_b->dep_buf);
        for (j = 0; j < cmon_modules_dep_count(_b->mods, i); ++j)
        {
            cmon_dyn_arr_append(&_b->dep_buf, cmon_modules_dep_mod_idx(_b->mods, i, j));
        }

        cmon_dep_graph_add(_b->dep_graph, i, &_b->dep_buf[0], cmon_dyn_arr_count(&_b->dep_buf));
    }

    cmon_dep_graph_result result = cmon_dep_graph_resolve(_b->dep_graph);
    // ensure that there is no circular dependency
    if (!result.array)
    {
        cmon_idx a, b;
        a = cmon_dep_graph_conflict_a(_b->dep_graph);
        b = cmon_dep_graph_conflict_b(_b->dep_graph);

        printf("booo\n");
        printf("circular dependency between modules '%s' and '%s'\n",
               cmon_modules_path(_b->mods, a),
               cmon_modules_path(_b->mods, b));

        cmon_idx dep_idx = cmon_modules_find_dep_idx(_b->mods, a, b);
        assert(cmon_is_valid_idx(dep_idx));

        cmon_idx src_idx = cmon_modules_dep_src_file_idx(_b->mods, a, dep_idx);
        assert(cmon_is_valid_idx(src_idx));

        cmon_idx tok_idx = cmon_modules_dep_tok_idx(_b->mods, a, dep_idx);
        assert(cmon_is_valid_idx(tok_idx));

        cmon_err_handler_err(_b->err_handler,
                             cmon_true,
                             src_idx,
                             tok_idx,
                             tok_idx,
                             tok_idx,
                             "circular dependency between modules '%s' and '%s'",
                             cmon_modules_path(_b->mods, a),
                             cmon_modules_path(_b->mods, b));

        cmon_err_handler_jump(_b->err_handler, cmon_true);
    }

    // resolve each module
    _log_status(_log, "    05. compiling modules\n");
    for (i = 0; i < result.count; ++i)
    {
        _log_status(_log, "        » %s\n", cmon_modules_path(_b->mods, result.array[i]));
        _per_module_data * pmd = &_b->mod_data[result.array[i]];

        _log_status(_log, "        01. user type pass\n");
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_usertypes_pass(pmd->resolver, j))
            {
                _add_resolver_errors(_b, pmd->resolver, cmon_true);
            }
        }

        _log_status(_log, "        02. globals pass\n");
        if (cmon_resolver_globals_pass(pmd->resolver))
        {
            _add_resolver_errors(_b, pmd->resolver, cmon_true);
        }

        _log_status(_log, "        03. user type default expression pass\n");
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_usertypes_def_expr_pass(pmd->resolver, j))
            {
                _add_resolver_errors(_b, pmd->resolver, cmon_true);
            }
        }

        _log_status(_log, "        05. main pass\n");
        for (j = 0; j < cmon_modules_src_file_count(_b->mods, i); ++j)
        {
            if (cmon_resolver_main_pass(pmd->resolver, j))
            {
                _add_resolver_errors(_b, pmd->resolver, cmon_true);
            }
        }

        _log_status(_log, "        04. dependency order pass\n");
        if (cmon_resolver_circ_pass(pmd->resolver))
        {
            _add_resolver_errors(_b, pmd->resolver, cmon_true);
        }

        _log_status(_log, "        06. IR generation\n");
        cmon_ir * ir = cmon_resolver_finalize(pmd->resolver);

        if (!ir)
        {
            _add_resolver_errors(_b, pmd->resolver, cmon_true);
        }

        pmd->ir = ir;
    }

    // if codegen fails, we panic for now and call it a day.
    _log_status(_log, "    06. code generation\n");
    if (cmon_codegen_prepare(_codegen, _b->mods, _b->types, _build_dir))
    {
        cmon_panic(cmon_codegen_err_msg(_codegen));
    }

    for (i = 0; i < result.count; ++i)
    {
        _log_status(_log, "        » %s\n", cmon_modules_path(_b->mods, result.array[i]));
        _per_module_data * pmd = &_b->mod_data[result.array[i]];
        cmon_idx session = cmon_codegen_begin_session(_codegen, result.array[i], pmd->ir);
        if (cmon_codegen_gen(_codegen, session))
        {
            cmon_panic(cmon_codegen_session_err_msg(_codegen, session));
        }
        cmon_codegen_end_session(_codegen, session);
    }

    _log_status(_log, "<- cmon_builder_st finished\n");

    return cmon_false;
err_end:
    return cmon_true;
}

cmon_bool cmon_builder_st_errors(cmon_builder_st * _b,
                                 cmon_err_report ** _out_errs,
                                 size_t * _out_count)
{
    if (cmon_err_handler_count(_b->err_handler))
    {
        *_out_errs = cmon_err_handler_err_report(_b->err_handler, 0);
        *_out_count = cmon_err_handler_count(_b->err_handler);
        return cmon_true;
    }
    *_out_errs = NULL;
    *_out_count = 0;
    return cmon_false;
}

cmon_types * cmon_builder_st_types(cmon_builder_st * _b)
{
    return _b->types;
}