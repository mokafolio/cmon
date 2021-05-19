#include <cmon/cmon_codegen.h>

static inline cmon_bool _empty(
    void * _obj, cmon_modules * _mods, cmon_idx _mod_idx, cmon_types * _types, cmon_ir * _ir)
{
    return cmon_false;
}

static inline const char * _empty_msg(void * _obj)
{
    return "";
}

cmon_codegen cmon_codegen_make_empty()
{
    cmon_codegen ret;
    ret.obj = NULL;
    ret.fn = _empty;
    ret.err_msg_fn = _empty_msg;
    ret.shutdown_fn = NULL;
    return ret;
}

cmon_bool cmon_codegen_gen(cmon_codegen * _cg,
                                    cmon_modules * _mods,
                                    cmon_idx _mod_idx,
                                    cmon_types * _types,
                                    cmon_ir * _ir)
{
    assert(_cg->fn);
    return _cg->fn(_cg->obj, _mods, _mod_idx, _types, _ir);
}

const char * cmon_codegen_err_msg(cmon_codegen * _cg)
{
    assert(_cg->err_msg_fn);
    return _cg->err_msg_fn(_cg->obj);
}

void cmon_codegen_dealloc(cmon_codegen * _cg)
{
    if (_cg->shutdown_fn)
        _cg->shutdown_fn(_cg->obj);
}
