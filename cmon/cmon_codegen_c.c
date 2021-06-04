#include <cmon/cmon_codegen_c.h>
#include <cmon/cmon_str_builder.h>

typedef struct
{
    cmon_allocator * alloc;
    cmon_ir * ir;
    cmon_types * types;
    cmon_modules * mods;
    cmon_idx mod_idx;
    cmon_str_builder * str_builder;
} _codegen_c;

static inline const char * _top_code()
{
    return "#include <stdint.h>\n"
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
           "#error \"c compiler with __STDC_IEC_559__ required\""
           "#endif\n"
           "typedef float f32;\n"
           "typedef double f64;\n\n";
}

static inline void _write_indent(_codegen_c * _cg, size_t _indent)
{
    size_t i;
    for (i = 0; i < _indent; ++i)
    {
        cmon_str_builder_append(_cg->str_builder, "    ");
    }
}

static inline void _write_type(_codegen_c * _cg, cmon_idx _idx)
{
    assert(cmon_is_valid_idx(_idx));
    cmon_str_builder_append(_cg->str_builder, cmon_types_unique_name(_cg->types, _idx));
}

// helper to retrieve the type for an ast node and write it
// static inline void _write_ast_type(_codegen_c * _cg, cmon_idx _file_idx, cmon_idx _ast_idx)
// {
//     _write_type(_cg, cmon_resolved_mod_resolved_type(_cg->resolved_mod, _file_idx, _ast_idx));
// }

static inline void _write_fn_name(_codegen_c * _cg, cmon_idx _idx)
{
    cmon_str_builder_append(_cg->str_builder, cmon_ir_fn_name(_cg->ir, _idx));
}

static inline void _write_fn_head(_codegen_c * _cg, cmon_idx _idx)
{
    if (!cmon_is_valid_idx(cmon_ir_fn_body(_cg->ir, _idx)))
    {
        cmon_str_builder_append(_cg->str_builder, "extern ");
    }
    //@TODO: Add pub to IR and make all non pub functions static.
    //@TODO: Pick a function body length threshold under which to add inline keyword?
    _write_type(_cg, cmon_ir_fn_return_type(_cg->ir, _idx));
    _write_fn_name(_cg, _idx);
    cmon_str_builder_append(_cg->str_builder, "(");
    size_t pcount = cmon_ir_fn_param_count(_cg->ir, _idx);
    for (size_t i = 0; i < pcount; ++i)
    {
        cmon_idx decl = cmon_ir_fn_param(_cg->ir, _idx, i);
        _write_type(_cg, cmon_ir_var_decl_type(_cg->ir, decl));
        cmon_str_builder_append_fmt(_cg->str_builder, " %s", cmon_ir_var_decl_name(_cg->ir, decl));
        if (i < pcount - 1)
        {
            cmon_str_builder_append(_cg->str_builder, ", ");
        }
    }
    cmon_str_builder_append(_cg->str_builder, ")");
}

static inline void _write_expr(_codegen_c * _cg, cmon_idx _idx)
{
    cmon_irk kind = cmon_ir_kind(_cg->ir, _idx);
    if (kind == cmon_irk_int_lit)
    {
        cmon_str_builder_append(_cg->str_builder, cmon_ir_int_lit_value(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_float_lit)
    {
        cmon_str_builder_append(_cg->str_builder, cmon_ir_float_lit_value(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_string_lit)
    {
        cmon_str_builder_append(_cg->str_builder, cmon_ir_string_lit_value(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_ident)
    {
        cmon_str_builder_append(_cg->str_builder, cmon_ir_ident_name(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_bool_lit)
    {
        cmon_str_builder_append(_cg->str_builder,
                                cmon_ir_bool_lit_value(_cg->ir, _idx) ? "true" : "false");
    }
    else if (kind == cmon_irk_noinit)
    {
        // cmon_str_builder_append(_cg->ir, "---");
    }
    else if (kind == cmon_irk_addr)
    {
        cmon_str_builder_append(_cg->str_builder, "&");
        _write_expr(_cg, cmon_ir_addr_expr(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_deref)
    {
        cmon_str_builder_append(_cg->str_builder, "*");
        _write_expr(_cg, cmon_ir_deref_expr(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_paran_expr)
    {
        cmon_str_builder_append(_cg->str_builder, "(");
        _write_expr(_cg, cmon_ir_paran_expr(_cg->ir, _idx));
        cmon_str_builder_append(_cg->str_builder, ")");
    }
    else if (kind == cmon_irk_call)
    {
        _write_expr(_cg, cmon_ir_call_left(_cg->ir, _idx));
        cmon_str_builder_append(_cg->str_builder, "(");
        for (size_t i = 0; i < cmon_ir_call_arg_count(_cg->ir, _idx); ++i)
        {
            _write_expr(_cg, cmon_ir_call_arg(_cg->ir, _idx, i));
            if (i < cmon_ir_call_arg_count(_cg->ir, _idx) - 1)
                cmon_str_builder_append(_cg->str_builder, ", ");
        }
        cmon_str_builder_append(_cg->str_builder, ")");
    }
    else if (kind == cmon_irk_struct_init)
    {
        cmon_str_builder_append_fmt(
            _cg->str_builder,
            "((%s){",
            cmon_types_unique_name(_cg->types, cmon_ir_struct_init_type(_cg->ir, _idx)));
        for (size_t i = 0; i < cmon_ir_struct_init_expr_count(_cg->ir, _idx); ++i)
        {
            _write_expr(_cg, cmon_ir_struct_init_expr(_cg->ir, _idx, i));
            if (i < cmon_ir_struct_init_expr_count(_cg->ir, _idx) - 1)
                cmon_str_builder_append(_cg->str_builder, ", ");
        }
        cmon_str_builder_append(_cg->str_builder, "})");
    }
    else if (kind == cmon_irk_array_init)
    {
        cmon_str_builder_append_fmt(
            _cg->str_builder,
            "((%s){.data={",
            cmon_types_unique_name(_cg->types, cmon_ir_array_init_type(_cg->ir, _idx)));
        for (size_t i = 0; i < cmon_ir_array_init_expr_count(_cg->ir, _idx); ++i)
        {
            _write_expr(_cg, cmon_ir_array_init_expr(_cg->ir, _idx, i));
            if (i < cmon_ir_array_init_expr_count(_cg->ir, _idx) - 1)
                cmon_str_builder_append(_cg->str_builder, ", ");
        }
        cmon_str_builder_append(_cg->str_builder, "}})");
    }
    else if (kind == cmon_irk_index)
    {
        _write_expr(_cg, cmon_ir_index_left(_cg->ir, _idx));
        cmon_str_builder_append(_cg->str_builder, "[");
        _write_expr(_cg, cmon_ir_index_expr(_cg->ir, _idx));
        cmon_str_builder_append(_cg->str_builder, "]");
    }
    else if (kind == cmon_irk_selector)
    {
        _write_expr(_cg, cmon_ir_selector_left(_cg->ir, _idx));
        cmon_str_builder_append_fmt(_cg->str_builder, ".%s", cmon_ir_selector_name(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_prefix)
    {
        cmon_str_builder_append_fmt(_cg->str_builder, "%c", cmon_ir_prefix_op(_cg->ir, _idx));
        _write_expr(_cg, cmon_ir_prefix_expr(_cg->ir, _idx));
    }
    else if (kind == cmon_irk_binary)
    {
        _write_expr(_cg, cmon_ir_binary_left(_cg->ir, _idx));
        cmon_str_builder_append_fmt(_cg->str_builder, " %c ", cmon_ir_binary_op(_cg->ir, _idx));
        _write_expr(_cg, cmon_ir_binary_right(_cg->ir, _idx));
    }
    else
    {
        assert(0);
    }
}

static inline void _write_stmt(_codegen_c * _cg, cmon_idx _idx, size_t _indent);

static inline void _write_var_decl(_codegen_c * _cg, cmon_idx _idx, cmon_bool _is_global)
{
    cmon_idx expr = cmon_ir_var_decl_expr(_cg->ir, _idx);
    if (_is_global && !cmon_is_valid_idx(expr))
    {
        cmon_str_builder_append(_cg->str_builder, "extern ");
    }
    cmon_str_builder_append_fmt(
        _cg->str_builder,
        "%s %s",
        cmon_types_unique_name(_cg->types, cmon_ir_var_decl_type(_cg->ir, _idx)),
        cmon_ir_var_decl_name(_cg->ir, _idx));
    if (!_is_global && cmon_is_valid_idx(expr))
    {
        cmon_str_builder_append(_cg->str_builder, " = ");
        _write_expr(_cg, expr);
    }
}

static inline void _write_block(_codegen_c * _cg, cmon_idx _idx, size_t _indent)
{
    _write_indent(_cg, _indent);
    cmon_str_builder_append_fmt(_cg->str_builder, "{\n");
    for (size_t i = 0; i < cmon_ir_block_child_count(_cg->ir, _idx); ++i)
    {
        _write_stmt(_cg, cmon_ir_block_child(_cg->ir, _idx, i), _indent + 1);
    }
    _write_indent(_cg, _indent);
    cmon_str_builder_append(_cg->str_builder, "}\n");
}

static inline void _write_stmt(_codegen_c * _cg, cmon_idx _idx, size_t _indent)
{
    cmon_irk kind = cmon_ir_kind(_cg->ir, _idx);
    if (kind == cmon_irk_block)
    {
        _write_block(_cg, _idx, _indent);
    }
    else if (kind == cmon_irk_var_decl)
    {
        _write_indent(_cg, _indent);
        _write_var_decl(_cg, _idx, cmon_false);
        cmon_str_builder_append(_cg->str_builder, ";\n");
    }
    else
    {
        // expr stmt
        _write_indent(_cg, _indent);
        _write_expr(_cg, _idx);
        cmon_str_builder_append(_cg->str_builder, ";\n");
    }
}

static inline cmon_bool _codegen_c_gen_fn(
    void * _cg, cmon_modules * _mods, cmon_idx _mod_idx, cmon_types * _types, cmon_ir * _ir)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    cg->ir = _ir;
    cg->types = _types;
    cg->mods = _mods;
    cg->mod_idx = _mod_idx;
    cmon_str_builder_append(cg->str_builder, _top_code());

    // forward declare all types used by the module
    size_t i, j, type_count;
    type_count = cmon_ir_type_count(_ir);
    for (i = 0; i < type_count; ++i)
    {
        cmon_idx tidx = cmon_ir_type(_ir, i);
        cmon_typek kind = cmon_types_kind(cg->types, tidx);
        if (kind != cmon_typek_ptr)
        {
            const char * uname = cmon_types_unique_name(cg->types, tidx);
            cmon_str_builder_append_fmt(cg->str_builder, "typedef struct %s %s;\n", uname, uname);
        }
    }

    // full type definitions
    for (i = 0; i < type_count; ++i)
    {
        cmon_idx tidx = cmon_ir_type(_ir, i);
        cmon_typek kind = cmon_types_kind(cg->types, tidx);
        const char * uname = cmon_types_unique_name(cg->types, tidx);
        if (kind == cmon_typek_struct)
        {
            cmon_str_builder_append(cg->str_builder, "typedef struct{\n");
            for (j = 0; j < cmon_types_struct_field_count(cg->types, tidx); ++j)
            {
                _write_indent(cg, 1);
                _write_type(cg, cmon_types_struct_field_type(cg->types, tidx, j));
                cmon_str_builder_append_fmt(
                    cg->str_builder, " %s;\n", cmon_types_struct_field_name(cg->types, tidx, j));
            }
            cmon_str_builder_append_fmt(cg->str_builder, "} %s;\n\n", uname);
        }
        else if (kind == cmon_typek_array)
        {
            cmon_str_builder_append(cg->str_builder, "typedef struct{\n");
            _write_indent(cg, 1);
            _write_type(cg, cmon_types_array_type(cg->types, tidx));
            cmon_str_builder_append_fmt(cg->str_builder,
                                        "data[%lu];\n} %s;\n\n",
                                        cmon_types_array_count(cg->types, tidx),
                                        uname);
        }
        else if (kind == cmon_typek_ptr)
        {
            // types to skip
        }
        else
        {
            assert(0);
        }
    }

    // declare all global variables
    for (i = 0; i < cmon_ir_global_var_count(_ir); ++i)
    {
        _write_var_decl(cg, cmon_ir_global_var(_ir, i), cmon_true);
        cmon_str_builder_append(cg->str_builder, ";\n");
    }

    // declare all functions (including extern functions in other modules)
    for (i = 0; i < cmon_ir_fn_count(_ir); ++i)
    {
        _write_fn_head(cg, cmon_ir_fn(_ir, i));
        cmon_str_builder_append(cg->str_builder, ";\n");
    }

    // define all functions (except ones in other modules)
    for (i = 0; i < cmon_ir_fn_count(_ir); ++i)
    {
        //main function is written later
        if(cmon_ir_fn(_ir, i) == cmon_ir_main_fn(_ir))
            continue;

        if (cmon_is_valid_idx(cmon_ir_fn_body(_ir, cmon_ir_fn(_ir, i))))
        {
            _write_fn_head(cg, cmon_ir_fn(_ir, i));
            cmon_str_builder_append(cg->str_builder, "\n");
            _write_block(cg, cmon_ir_fn_body(_ir, cmon_ir_fn(_ir, i)), 0);
        }
    }

    // write c main function
    // define all globals in dependency order (including the ones from other modules)
    // call cmon main function

    return cmon_false;
}

static inline void _codegen_c_shutdown_fn(void * _cg)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    cmon_str_builder_destroy(cg->str_builder);
    CMON_DESTROY(cg->alloc, cg);
}

static inline const char * _codegen_c_err_msg_fn(void * _cg)
{
}

cmon_codegen cmon_codegen_c_make(cmon_allocator * _alloc)
{
    _codegen_c * cgen = CMON_CREATE(_alloc, _codegen_c);
    cgen->alloc = _alloc;
    cgen->str_builder = cmon_str_builder_create(_alloc, 2048);
    cgen->ir = NULL;
    cgen->types = NULL;
    // cgen->src = NULL;
    cgen->mods = NULL;
    return (cmon_codegen){ cgen, _codegen_c_gen_fn, _codegen_c_shutdown_fn, _codegen_c_err_msg_fn };
}
