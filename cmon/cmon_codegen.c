#include <cmon/cmon_codegen.h>

static inline cmon_bool _empty_prep(
    void * _obj, cmon_modules *_mods, cmon_types * _types, const char * _build_dir)
{
    return cmon_false;
}

static inline cmon_idx _empty_begin_session(void * _obj, cmon_idx _mod_idx, cmon_ir * _ir)
{
    return CMON_INVALID_IDX;
}

static inline void _empty_end_session(void * _obj, cmon_idx _session_idx)
{

}

static inline cmon_bool _empty_gen(void * _obj, cmon_idx _session_idx)
{
    return cmon_false;
}

static inline const char * _empty_err_msg(void * _obj)
{
    return "";
}

static inline const char * _empty_session_err_msg(void * _obj, cmon_idx _session_idx)
{
    return "";
}

cmon_codegen cmon_codegen_make_empty()
{
    cmon_codegen ret;
    ret.obj = NULL;
    ret.prep_fn = _empty_prep;
    ret.begin_session_fn = _empty_begin_session;
    ret.end_session_fn = _empty_end_session;
    ret.fn = _empty_gen;
    ret.err_msg_fn = _empty_err_msg;
    ret.sess_err_msg_fn = _empty_session_err_msg;
    ret.shutdown_fn = NULL;
    return ret;
}

cmon_bool cmon_codegen_prepare(cmon_codegen * _cg, cmon_modules * _mods, cmon_types * _types, const char * _build_dir)
{
    assert(_cg->prep_fn);
    return _cg->prep_fn(_cg->obj, _mods, _types, _build_dir);
}

cmon_idx cmon_codegen_begin_session(cmon_codegen * _cg, cmon_idx _mod_idx, cmon_ir * _ir)
{
    assert(_cg->begin_session_fn);
    return _cg->begin_session_fn(_cg->obj, _mod_idx, _ir);
}

void cmon_codegen_end_session(cmon_codegen * _cg, cmon_idx _session_idx)
{
    assert(_cg->end_session_fn);
    return _cg->end_session_fn(_cg->obj, _session_idx);
}

cmon_bool cmon_codegen_gen(cmon_codegen * _cg, cmon_idx _session_idx)
{
    assert(_cg->fn);
    return _cg->fn(_cg->obj, _session_idx);
}

const char * cmon_codegen_err_msg(cmon_codegen * _cg)
{
    assert(_cg->err_msg_fn);
    return _cg->err_msg_fn(_cg->obj);
}

const char * cmon_codegen_session_err_msg(cmon_codegen * _cg, cmon_idx _session_idx)
{
    assert(_cg->sess_err_msg_fn);
    return _cg->sess_err_msg_fn(_cg->obj, _session_idx);
}

void cmon_codegen_dealloc(cmon_codegen * _cg)
{
    if (_cg->shutdown_fn)
        _cg->shutdown_fn(_cg->obj);
}
