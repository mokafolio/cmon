#include <cmon/cmon_codegen_c.h>
#include <cmon/cmon_str_builder.h>

typedef struct
{
    cmon_allocator * alloc;
    cmon_resolved_mod * resolved_mod;
    cmon_types * types;
    cmon_src * src;
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
           "typedef uint64_t u64;\n\n";
}

static inline cmon_tokens * _tokens(_codegen_c * _cg, cmon_idx _file_idx)
{
    return cmon_src_tokens(_cg->src, _file_idx);
}

static inline cmon_ast * _ast(_codegen_c * _cg, cmon_idx _file_idx)
{
    return cmon_src_ast(_cg->src, _file_idx);
}

static inline cmon_idx _ast_resolved_type(_codegen_c * _cg, cmon_idx _file_idx, cmon_idx _ast_idx)
{
    return cmon_resolved_mod_resolved_type(_cg->resolved_mod, _file_idx, _ast_idx);
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

static inline void _write_fn_name(_codegen_c * _cg, cmon_idx _file_idx, cmon_idx _ast_idx)
{
    const char * pref = cmon_modules_prefix(_cg->mods, _cg->mod_idx);
    cmon_tokens * toks = _tokens(_cg, _file_idx);
    cmon_ast * ast = _ast(_cg, _file_idx);
    cmon_str_view name = cmon_tokens_str_view(toks, cmon_ast_var_decl_name_tok(ast, _ast_idx));
    cmon_str_builder_append_fmt(
        _cg->str_builder, "%s_%.*s", pref, name.end - name.begin, name.begin);
}

static inline void _write_fn_head(_codegen_c * _cg, cmon_idx _file_idx, cmon_idx _ast_idx)
{
    assert(cmon_ast_kind(_ast(_cg, _file_idx), _ast_idx) == cmon_astk_var_decl);
    cmon_idx fn = cmon_ast_var_decl_expr(_ast(_cg, _file_idx), _ast_idx);
    cmon_idx ast_ret = cmon_ast_fn_ret_type(_ast(_cg, _file_idx), fn);
    _write_type(_cg, _ast_resolved_type(_cg, _file_idx, ast_ret));
    _write_fn_name(_cg, _file_idx, _ast_idx);
    cmon_str_builder_append(_cg->str_builder, "(");
    // cmon_ast_iter it = cmon_ast_fn_params_begin(_ast(_cg, _file_idx), _ast_idx);
    // cmon_idx idx;
    // while (cmon_is_valid_idx(idx = cmon_ast_iter_next(_ast(_cg, _file_idx), &it)))
    // {
        
    // }
    cmon_str_builder_append(_cg->str_builder, ")");
}

static inline cmon_bool _codegen_c_gen_fn(void * _cg, cmon_resolved_mod * _mod)
{
    _codegen_c * cg = (_codegen_c *)_cg;
    cg->resolved_mod = _mod;
    cg->types = cmon_resolved_mod_types(_mod);
    cg->src = cmon_resolved_mod_src(_mod);
    cg->mods = cmon_resolved_mod_modules(_mod);
    cg->mod_idx = cmon_resolved_mod_module_idx(_mod);
    cmon_str_builder_append(cg->str_builder, _top_code());

    // forward declare all types used by the module
    size_t i, j, type_count;
    type_count = cmon_resolved_mod_type_count(_mod);
    for (i = 0; i < type_count; ++i)
    {
        cmon_idx tidx = cmon_resolved_mod_type(_mod, i);
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
        cmon_idx tidx = cmon_resolved_mod_type(_mod, i);
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
                                        "[%lu];\n} %s;\n\n",
                                        cmon_types_array_count(cg->types, tidx),
                                        uname);
        }
    }

    // declare all global functions

    // define all global functions

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
    _codegen_c * cgen = CMON_CREATE(_alloc, cgen);
    cgen->alloc = _alloc;
    cgen->str_builder = cmon_str_builder_create(_alloc, 2048);
    cgen->resolved_mod = NULL;
    cgen->types = NULL;
    cgen->src = NULL;
    cgen->mods = NULL;
    return (cmon_codegen){ cgen, _codegen_c_gen_fn, _codegen_c_shutdown_fn, _codegen_c_err_msg_fn };
}
