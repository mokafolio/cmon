#include <cmon/cmon_codegen_c.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_exec.h>
#include <cmon/cmon_fs.h>
#include <cmon/cmon_str_builder.h>
#include <cmon/cmon_util.h>

typedef struct _codegen_c _codegen_c;

typedef struct
{
    _codegen_c * cgen;
    cmon_idx mod_idx;
    cmon_ir * ir;
    cmon_str_builder * str_builder;
    cmon_str_builder * tmp_str_builder;
    cmon_str_builder * c_compiler_output_builder;
    char c_path[CMON_PATH_MAX];
    char o_path[CMON_PATH_MAX];
    char err_msg[CMON_ERR_MSG_MAX];
} _session;

typedef struct _codegen_c
{
    cmon_allocator * alloc;
    cmon_types * types;
    cmon_modules * mods;
    char build_dir[CMON_PATH_MAX];
    char cgen_dir[CMON_PATH_MAX];
    char c_dir[CMON_PATH_MAX];
    char o_dir[CMON_PATH_MAX];
    char err_msg[CMON_ERR_MSG_MAX];
    cmon_dyn_arr(_session) sessions;
    cmon_dyn_arr(cmon_idx) free_sessions;
} _codegen_c;

static inline cmon_bool _set_err(_codegen_c * _cg, const char * _msg)
{
    assert(sizeof(_cg->err_msg) > strlen(_msg));
    strcpy(_cg->err_msg, _msg);
    return cmon_true;
}

static inline cmon_bool _set_sess_err(_session * _s, const char * _msg)
{
    assert(sizeof(_s->err_msg) > strlen(_msg));
    strcpy(_s->err_msg, _msg);
    return cmon_true;
}

static inline const char * _top_code()
{
    return "#include <stdint.h>\n"
           "#include <stdio.h>\n"
           "#include <stdbool.h>\n\n"
           "typedef int8_t s8;\n"
           "typedef int16_t s16;\n"
           "typedef int32_t s32;\n"
           "typedef int64_t s64;\n"
           "typedef uint8_t u8;\n"
           "typedef uint16_t u16;\n"
           "typedef uint32_t u32;\n"
           "typedef uint64_t u64;\n"
           "#ifndef __STDC_IEC_559__\n"
           "#error \"c compiler with __STDC_IEC_559__ required\"\n"
           "#endif\n"
           "typedef float f32;\n"
           "typedef double f64;\n\n";
}

static inline void _write_indent(_session * _s, size_t _indent)
{
    size_t i;
    for (i = 0; i < _indent; ++i)
    {
        cmon_str_builder_append(_s->str_builder, "    ");
    }
}

static inline void _write_type(_session * _s, cmon_idx _idx)
{
    assert(cmon_is_valid_idx(_idx));
    if(cmon_types_kind(_s->cgen->types, _idx) == cmon_typek_ptr)
    {
        _write_type(_s, cmon_types_ptr_type(_s->cgen->types, _idx));
        //@TODO: Add const for non mutable pointers? Should not make a difference at this level I guess...
        cmon_str_builder_append(_s->str_builder, " *");
    }
    else
    {
        cmon_str_builder_append(_s->str_builder, cmon_types_unique_name(_s->cgen->types, _idx));
    }
}

// helper to retrieve the type for an ast node and write it
// static inline void _write_ast_type(_codegen_c * _cg, cmon_idx _file_idx, cmon_idx _ast_idx)
// {
//     _write_type(_cg, cmon_resolved_mod_resolved_type(_cg->resolved_mod, _file_idx, _ast_idx));
// }

static inline void _write_fn_name(_session * _s, cmon_idx _idx)
{
    cmon_str_builder_append(_s->str_builder, cmon_ir_fn_name(_s->ir, _idx));
}

static inline void _write_global_init_fn_name(_session * _s, const char * _dep_name)
{
    cmon_str_builder_append_fmt(_s->str_builder, "__%s_init_globals()", _dep_name);
}

static inline void _write_global_init_fn_head(_session * _s,
                                              const char * _dep_name,
                                              cmon_bool _is_extern)
{
    if (_is_extern)
    {
        cmon_str_builder_append(_s->str_builder, "extern void ");
    }
    else
    {
        cmon_str_builder_append(_s->str_builder, "void ");
    }
    _write_global_init_fn_name(_s, _dep_name);
}

static inline void _write_fn_head(_session * _s, cmon_idx _idx)
{
    if (!cmon_is_valid_idx(cmon_ir_fn_body(_s->ir, _idx)))
    {
        cmon_str_builder_append(_s->str_builder, "extern ");
    }
    //@TODO: Add pub to IR and make all non pub functions static.
    //@TODO: Pick a function body length threshold under which to add inline keyword?
    _write_type(_s, cmon_ir_fn_return_type(_s->ir, _idx));
    cmon_str_builder_append(_s->str_builder, " ");
    _write_fn_name(_s, _idx);
    cmon_str_builder_append(_s->str_builder, "(");
    size_t pcount = cmon_ir_fn_param_count(_s->ir, _idx);
    for (size_t i = 0; i < pcount; ++i)
    {
        cmon_idx decl = cmon_ir_fn_param(_s->ir, _idx, i);
        _write_type(_s, cmon_ir_var_decl_type(_s->ir, decl));
        cmon_str_builder_append_fmt(_s->str_builder, " %s", cmon_ir_var_decl_name(_s->ir, decl));
        if (i < pcount - 1)
        {
            cmon_str_builder_append(_s->str_builder, ", ");
        }
    }
    cmon_str_builder_append(_s->str_builder, ")");
}

static inline void _write_expr(_session * _s, cmon_idx _idx)
{
    cmon_irk kind = cmon_ir_kind(_s->ir, _idx);
    if (kind == cmon_irk_int_lit)
    {
        cmon_str_builder_append(_s->str_builder, cmon_ir_int_lit_value(_s->ir, _idx));
    }
    else if (kind == cmon_irk_float_lit)
    {
        cmon_str_builder_append(_s->str_builder, cmon_ir_float_lit_value(_s->ir, _idx));
    }
    else if (kind == cmon_irk_string_lit)
    {
        cmon_str_builder_append(_s->str_builder, cmon_ir_string_lit_value(_s->ir, _idx));
    }
    else if (kind == cmon_irk_ident)
    {
        cmon_str_builder_append(_s->str_builder, cmon_ir_ident_name(_s->ir, _idx));
    }
    else if (kind == cmon_irk_bool_lit)
    {
        cmon_str_builder_append(_s->str_builder,
                                cmon_ir_bool_lit_value(_s->ir, _idx) ? "true" : "false");
    }
    else if (kind == cmon_irk_noinit)
    {
        // cmon_str_builder_append(_cg->ir, "---");
    }
    else if (kind == cmon_irk_addr)
    {
        cmon_str_builder_append(_s->str_builder, "&");
        _write_expr(_s, cmon_ir_addr_expr(_s->ir, _idx));
    }
    else if (kind == cmon_irk_deref)
    {
        cmon_str_builder_append(_s->str_builder, "*");
        _write_expr(_s, cmon_ir_deref_expr(_s->ir, _idx));
    }
    else if (kind == cmon_irk_paran_expr)
    {
        cmon_str_builder_append(_s->str_builder, "(");
        _write_expr(_s, cmon_ir_paran_expr(_s->ir, _idx));
        cmon_str_builder_append(_s->str_builder, ")");
    }
    else if (kind == cmon_irk_call)
    {
        _write_expr(_s, cmon_ir_call_left(_s->ir, _idx));
        cmon_str_builder_append(_s->str_builder, "(");
        for (size_t i = 0; i < cmon_ir_call_arg_count(_s->ir, _idx); ++i)
        {
            _write_expr(_s, cmon_ir_call_arg(_s->ir, _idx, i));
            if (i < cmon_ir_call_arg_count(_s->ir, _idx) - 1)
                cmon_str_builder_append(_s->str_builder, ", ");
        }
        cmon_str_builder_append(_s->str_builder, ")");
    }
    else if (kind == cmon_irk_struct_init)
    {
        cmon_str_builder_append_fmt(
            _s->str_builder,
            "((%s){",
            cmon_types_unique_name(_s->cgen->types, cmon_ir_struct_init_type(_s->ir, _idx)));
        for (size_t i = 0; i < cmon_ir_struct_init_expr_count(_s->ir, _idx); ++i)
        {
            _write_expr(_s, cmon_ir_struct_init_expr(_s->ir, _idx, i));
            if (i < cmon_ir_struct_init_expr_count(_s->ir, _idx) - 1)
                cmon_str_builder_append(_s->str_builder, ", ");
        }
        cmon_str_builder_append(_s->str_builder, "})");
    }
    else if (kind == cmon_irk_array_init)
    {
        cmon_str_builder_append_fmt(
            _s->str_builder,
            "((%s){.data={",
            cmon_types_unique_name(_s->cgen->types, cmon_ir_array_init_type(_s->ir, _idx)));
        for (size_t i = 0; i < cmon_ir_array_init_expr_count(_s->ir, _idx); ++i)
        {
            _write_expr(_s, cmon_ir_array_init_expr(_s->ir, _idx, i));
            if (i < cmon_ir_array_init_expr_count(_s->ir, _idx) - 1)
                cmon_str_builder_append(_s->str_builder, ", ");
        }
        cmon_str_builder_append(_s->str_builder, "}})");
    }
    else if (kind == cmon_irk_index)
    {
        _write_expr(_s, cmon_ir_index_left(_s->ir, _idx));
        cmon_str_builder_append(_s->str_builder, "[");
        _write_expr(_s, cmon_ir_index_expr(_s->ir, _idx));
        cmon_str_builder_append(_s->str_builder, "]");
    }
    else if (kind == cmon_irk_selector)
    {
        _write_expr(_s, cmon_ir_selector_left(_s->ir, _idx));
        cmon_str_builder_append_fmt(_s->str_builder, ".%s", cmon_ir_selector_name(_s->ir, _idx));
    }
    else if (kind == cmon_irk_prefix)
    {
        cmon_str_builder_append_fmt(_s->str_builder, "%c", cmon_ir_prefix_op(_s->ir, _idx));
        _write_expr(_s, cmon_ir_prefix_expr(_s->ir, _idx));
    }
    else if (kind == cmon_irk_binary)
    {
        _write_expr(_s, cmon_ir_binary_left(_s->ir, _idx));
        cmon_str_builder_append_fmt(_s->str_builder, " %c ", cmon_ir_binary_op(_s->ir, _idx));
        _write_expr(_s, cmon_ir_binary_right(_s->ir, _idx));
    }
    else
    {
        assert(0);
    }
}

static inline void _write_stmt(_session * _s, cmon_idx _idx, size_t _indent);

static inline void _write_var_decl(_session * _s, cmon_idx _idx, cmon_bool _is_global)
{
    cmon_idx expr = cmon_ir_var_decl_expr(_s->ir, _idx);
    if (_is_global && !cmon_is_valid_idx(expr))
    {
        cmon_str_builder_append(_s->str_builder, "extern ");
    }
    _write_type(_s, cmon_ir_var_decl_type(_s->ir, _idx));
    cmon_str_builder_append_fmt(
        _s->str_builder,
        " %s",
        cmon_ir_var_decl_name(_s->ir, _idx));
    if (!_is_global && cmon_is_valid_idx(expr))
    {
        cmon_str_builder_append(_s->str_builder, " = ");
        _write_expr(_s, expr);
    }
}

static inline void _write_block(_session * _s, cmon_idx _idx, size_t _indent)
{
    _write_indent(_s, _indent);
    cmon_str_builder_append_fmt(_s->str_builder, "{\n");
    for (size_t i = 0; i < cmon_ir_block_child_count(_s->ir, _idx); ++i)
    {
        _write_stmt(_s, cmon_ir_block_child(_s->ir, _idx, i), _indent + 1);
    }
    _write_indent(_s, _indent);
    cmon_str_builder_append(_s->str_builder, "}\n\n");
}

static inline void _write_stmt(_session * _s, cmon_idx _idx, size_t _indent)
{
    cmon_irk kind = cmon_ir_kind(_s->ir, _idx);
    if (kind == cmon_irk_block)
    {
        _write_block(_s, _idx, _indent);
    }
    else if (kind == cmon_irk_var_decl)
    {
        _write_indent(_s, _indent);
        _write_var_decl(_s, _idx, cmon_false);
        cmon_str_builder_append(_s->str_builder, ";\n");
    }
    else
    {
        // expr stmt
        _write_indent(_s, _indent);
        _write_expr(_s, _idx);
        cmon_str_builder_append(_s->str_builder, ";\n");
    }
}

static inline cmon_bool _codegen_c_prep_fn(void * _cg, cmon_modules *_mods, cmon_types * _types, const char * _build_dir)
{
    _codegen_c * cg = (_codegen_c *)_cg;

    cg->mods = _mods;
    cg->types = _types;
    strcpy(cg->build_dir, _build_dir);

    if (!cmon_fs_exists(cg->build_dir))
    {
        return _set_err(cg, "missing build directory");
    }

    cmon_join_paths(cg->build_dir, "cgen", cg->cgen_dir, sizeof(cg->cgen_dir));
    cmon_join_paths(cg->cgen_dir, "c", cg->c_dir, sizeof(cg->c_dir));
    cmon_join_paths(cg->cgen_dir, "o", cg->o_dir, sizeof(cg->o_dir));

    if (!cmon_fs_exists(cg->cgen_dir))
    {
        if (cmon_fs_mkdir(cg->cgen_dir) == -1)
        {
            return _set_err(cg, "failed to create cgen directory");
        }
    }

    if (!cmon_fs_exists(cg->c_dir))
    {
        if (cmon_fs_mkdir(cg->c_dir) == -1)
        {
            return _set_err(cg, "failed to create c directory");
        }
    }

    if (!cmon_fs_exists(cg->o_dir))
    {
        if (cmon_fs_mkdir(cg->o_dir) == -1)
        {
            return _set_err(cg, "failed to create o directory");
        }
    }

    return cmon_false;
}

static inline cmon_bool _gen_fn(_session * _s)
{
    cmon_str_builder_append(_s->str_builder, _top_code());

    // forward declare all types used by the module
    size_t i, j, type_count;
    type_count = cmon_ir_type_count(_s->ir);
    for (i = 0; i < type_count; ++i)
    {
        cmon_idx tidx = cmon_ir_type(_s->ir, i);
        cmon_typek kind = cmon_types_kind(_s->cgen->types, tidx);
        if (kind != cmon_typek_ptr && kind != cmon_typek_fn &&
            !cmon_types_is_builtin(_s->cgen->types, tidx))
        {
            const char * uname = cmon_types_unique_name(_s->cgen->types, tidx);
            cmon_str_builder_append_fmt(_s->str_builder, "typedef struct %s %s;\n", uname, uname);
        }
    }

    cmon_str_builder_append(_s->str_builder, "\n");

    // full type definitions
    for (i = 0; i < type_count; ++i)
    {
        cmon_idx tidx = cmon_ir_type(_s->ir, i);
        cmon_typek kind = cmon_types_kind(_s->cgen->types, tidx);
        const char * uname = cmon_types_unique_name(_s->cgen->types, tidx);
        printf("KIND %lu\n", kind);
        if (kind == cmon_typek_struct)
        {
            cmon_str_builder_append_fmt(_s->str_builder, "typedef struct %s{\n", uname);
            for (j = 0; j < cmon_types_struct_field_count(_s->cgen->types, tidx); ++j)
            {
                _write_indent(_s, 1);
                _write_type(_s, cmon_types_struct_field_type(_s->cgen->types, tidx, j));
                cmon_str_builder_append_fmt(_s->str_builder,
                                            " %s;\n",
                                            cmon_types_struct_field_name(_s->cgen->types, tidx, j));
            }
            cmon_str_builder_append_fmt(_s->str_builder, "} %s;\n\n", uname);
        }
        else if (kind == cmon_typek_array)
        {
            cmon_str_builder_append(_s->str_builder, "typedef struct{\n");
            _write_indent(_s, 1);
            _write_type(_s, cmon_types_array_type(_s->cgen->types, tidx));
            cmon_str_builder_append_fmt(_s->str_builder,
                                        "data[%lu];\n} %s;\n\n",
                                        cmon_types_array_count(_s->cgen->types, tidx),
                                        uname);
        }
        else if (kind == cmon_typek_ptr || kind == cmon_typek_fn ||
                 cmon_types_is_builtin(_s->cgen->types, tidx))
        {
            // types to skip
        }
        else
        {
            assert(0);
        }
    }

    cmon_str_builder_append(_s->str_builder, "\n");

    // declare all global variables
    for (i = 0; i < cmon_ir_global_var_count(_s->ir); ++i)
    {
        _write_var_decl(_s, cmon_ir_global_var(_s->ir, i), cmon_true);
        cmon_str_builder_append(_s->str_builder, ";\n");
    }

    cmon_str_builder_append(_s->str_builder, "\n");

    // declare all functions (including extern functions in other modules)
    for (i = 0; i < cmon_ir_fn_count(_s->ir); ++i)
    {
        _write_fn_head(_s, cmon_ir_fn(_s->ir, i));
        cmon_str_builder_append(_s->str_builder, ";\n");
    }

    cmon_str_builder_append(_s->str_builder, "\n");

    // declare all global init functions for all the modules dependencies
    for (i = 0; i < cmon_ir_dep_count(_s->ir); ++i)
    {
        _write_global_init_fn_head(_s, cmon_ir_dep_name(_s->ir, (cmon_idx)i), cmon_true);
        cmon_str_builder_append(_s->str_builder, ";\n");
    }

    cmon_str_builder_append(_s->str_builder, "\n");

    // define all functions (except ones in other modules)
    for (i = 0; i < cmon_ir_fn_count(_s->ir); ++i)
    {
        if (cmon_is_valid_idx(cmon_ir_fn_body(_s->ir, cmon_ir_fn(_s->ir, i))))
        {
            _write_fn_head(_s, cmon_ir_fn(_s->ir, i));
            cmon_str_builder_append(_s->str_builder, "\n");
            _write_block(_s, cmon_ir_fn_body(_s->ir, cmon_ir_fn(_s->ir, i)), 0);
        }
    }

    // write the global init function for this module
    _write_global_init_fn_head(_s, cmon_modules_prefix(_s->cgen->mods, _s->mod_idx), cmon_false);
    cmon_str_builder_append(_s->str_builder, "\n{\n");
    for (i = 0; i < cmon_ir_global_var_count(_s->ir); ++i)
    {
        cmon_idx var = cmon_ir_global_var(_s->ir, i);
        if (cmon_is_valid_idx(cmon_ir_var_decl_expr(_s->ir, var)))
        {
            _write_indent(_s, 1);
            cmon_str_builder_append_fmt(
                _s->str_builder, "%s = ", cmon_ir_var_decl_name(_s->ir, var));
            _write_expr(_s, cmon_ir_var_decl_expr(_s->ir, var));
            cmon_str_builder_append(_s->str_builder, ";\n");
        }
    }
    cmon_str_builder_append(_s->str_builder, "}\n\n");

    // write c main function (if needed)
    cmon_idx main_fn = cmon_ir_main_fn(_s->ir);
    if (cmon_is_valid_idx(main_fn))
    {
        cmon_str_builder_append(_s->str_builder, "\nint main(int _argc, const char ** _args)\n{\n");

        // call other modules global init functions
        for (i = 0; i < cmon_ir_dep_count(_s->ir); ++i)
        {
            _write_indent(_s, 1);
            _write_global_init_fn_name(_s, cmon_ir_dep_name(_s->ir, (cmon_idx)i));
            cmon_str_builder_append(_s->str_builder, ";\n");
        }

        // init the globals in this module
        _write_indent(_s, 1);
        _write_global_init_fn_name(_s, cmon_modules_prefix(_s->cgen->mods, _s->mod_idx));
        cmon_str_builder_append(_s->str_builder, ";\n\n");

        // call cmon main function
        _write_indent(_s, 1);
        cmon_idx ret_type = cmon_ir_fn_return_type(_s->ir, main_fn);
        if (cmon_types_kind(_s->cgen->types, ret_type) == cmon_typek_s32)
        {
            cmon_str_builder_append(_s->str_builder, "return ");
        }
        cmon_str_builder_append_fmt(_s->str_builder, "%s();\n", cmon_ir_fn_name(_s->ir, main_fn));

        cmon_str_builder_append(_s->str_builder, "}\n");
    }

    cmon_join_paths(_s->cgen->c_dir,
                    cmon_str_builder_tmp_str(_s->tmp_str_builder,
                                             "%s.c",
                                             cmon_modules_prefix(_s->cgen->mods, _s->mod_idx)),
                    _s->c_path,
                    sizeof(_s->c_path));

    // create output path name
    if (!cmon_is_valid_idx(main_fn))
    {
        cmon_join_paths(_s->cgen->o_dir,
                        cmon_str_builder_tmp_str(_s->tmp_str_builder,
                                                 "%s.o",
                                                 cmon_modules_prefix(_s->cgen->mods, _s->mod_idx)),
                        _s->o_path,
                        sizeof(_s->o_path));
    }
    else
    {
        cmon_join_paths(_s->cgen->build_dir,
                        cmon_str_builder_tmp_str(_s->tmp_str_builder,
                                                 "%s",
                                                 cmon_modules_prefix(_s->cgen->mods, _s->mod_idx)),
                        _s->o_path,
                        sizeof(_s->o_path));
    }

    if (cmon_fs_write_txt_file(_s->c_path, cmon_str_builder_c_str(_s->str_builder)) == -1)
    {
        return _set_sess_err(_s, "could not save c file");
    }

    // generate the command to compile the c code
    cmon_str_builder_clear(_s->tmp_str_builder);
    if (!cmon_is_valid_idx(main_fn))
    {
        // for non-executable modules, simply build the .o file
        cmon_str_builder_append_fmt(
            _s->tmp_str_builder, "clang -c %s -o %s 2>&1", _s->c_path, _s->o_path);
    }
    else
    {
        // build the .c file and link all dependencies .o files to create the executable
        cmon_str_builder_append_fmt(_s->tmp_str_builder, "clang %s ", _s->c_path);
        for (i = 0; i < cmon_ir_dep_count(_s->ir); ++i)
        {
            cmon_str_builder_append_fmt(_s->tmp_str_builder,
                                        "%s/%s.o ",
                                        _s->cgen->o_dir,
                                        cmon_ir_dep_name(_s->ir, (cmon_idx)i));
        }
        cmon_str_builder_append_fmt(_s->tmp_str_builder, "-o %s 2>&1", _s->o_path);
    }

    printf("DA FOCKING CMD %s\n", cmon_str_builder_c_str(_s->tmp_str_builder));

    int status =
        cmon_exec(cmon_str_builder_c_str(_s->tmp_str_builder), _s->c_compiler_output_builder);

    printf("DA FOCKING STATUS %i\n", status);
    if (status != 0)
    {
        return _set_sess_err(
            _s,
            cmon_str_builder_tmp_str(_s->tmp_str_builder,
                                     "c compiler error: %s",
                                     cmon_str_builder_c_str(_s->c_compiler_output_builder)));
    }

    return cmon_false;
}

static inline cmon_bool _codegen_c_gen_fn(void * _cg, cmon_idx _session_idx)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    _session * s = &cg->sessions[_session_idx];
    return _gen_fn(s);
}

static inline cmon_idx _reset_session(_codegen_c * _cg,
                                      cmon_idx _session_idx,
                                      cmon_idx _mod_idx,
                                      cmon_ir * _ir)
{
    _session * s = &_cg->sessions[_session_idx];
    cmon_str_builder_clear(s->str_builder);
    cmon_str_builder_clear(s->tmp_str_builder);
    cmon_str_builder_clear(s->c_compiler_output_builder);
    s->mod_idx = _mod_idx;
    s->ir = _ir;
    return _session_idx;
}

static inline cmon_idx _codegen_c_begin_session(void * _cg, cmon_idx _mod_idx, cmon_ir * _ir)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    if (cmon_dyn_arr_count(&cg->free_sessions))
    {
        return _reset_session(cg, cmon_dyn_arr_pop(&cg->free_sessions), _mod_idx, _ir);
    }

    _session s;
    s.str_builder = cmon_str_builder_create(cg->alloc, 2048);
    s.tmp_str_builder = cmon_str_builder_create(cg->alloc, CMON_PATH_MAX);
    s.c_compiler_output_builder = cmon_str_builder_create(cg->alloc, CMON_PATH_MAX);
    s.mod_idx = _mod_idx;
    s.ir = _ir;
    s.cgen = cg;
    cmon_dyn_arr_append(&cg->sessions, s);
    return cmon_dyn_arr_count(&cg->sessions) - 1;
}

static inline void _codegen_c_end_session(void * _cg, cmon_idx _session_idx)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    cmon_dyn_arr_append(&cg->free_sessions, _session_idx);
}

static inline void _codegen_c_shutdown_fn(void * _cg)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    cmon_dyn_arr_dealloc(&cg->free_sessions);
    for (size_t i = 0; i < cmon_dyn_arr_count(&cg->sessions); ++i)
    {
        _session * s = &cg->sessions[i];
        cmon_str_builder_destroy(s->c_compiler_output_builder);
        cmon_str_builder_destroy(s->tmp_str_builder);
        cmon_str_builder_destroy(s->str_builder);
    }
    cmon_dyn_arr_dealloc(&cg->sessions);
    CMON_DESTROY(cg->alloc, cg);
}

static inline const char * _codegen_c_err_msg_fn(void * _cg)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    return cg->err_msg;
}

static inline const char * _codegen_c_sess_err_msg_fn(void * _cg, cmon_idx _session_idx)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    return cg->sessions[_session_idx].err_msg;
}

cmon_codegen cmon_codegen_c_make(cmon_allocator * _alloc)
{
    _codegen_c * cgen = CMON_CREATE(_alloc, _codegen_c);
    cgen->alloc = _alloc;
    cgen->types = NULL;
    cgen->mods = NULL;
    cmon_dyn_arr_init(&cgen->sessions, _alloc, 4);
    cmon_dyn_arr_init(&cgen->free_sessions, _alloc, 4);
    return (cmon_codegen){ cgen,
                           _codegen_c_prep_fn,
                           _codegen_c_begin_session,
                           _codegen_c_end_session,
                           _codegen_c_gen_fn,
                           _codegen_c_shutdown_fn,
                           _codegen_c_err_msg_fn,
                           _codegen_c_sess_err_msg_fn };
}
