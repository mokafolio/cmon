#include <cmon/cmon_argparse.h>
#include <cmon/cmon_dyn_arr.h>
#include <cmon/cmon_util.h>

typedef struct
{
    char key_short[CMON_ARG_KEY_MAX];
    char key_long[CMON_ARG_KEY_MAX];
    char help[CMON_ARG_VAL_MAX];
    cmon_bool expects_value;
    cmon_bool accepts_any;
    cmon_dyn_arr(char *) possible_vals;
    char val[CMON_ARG_VAL_MAX];
    cmon_idx default_idx;
    cmon_bool was_set;
} _arg;

typedef struct cmon_argparse
{
    cmon_allocator * alloc;
    cmon_dyn_arr(_arg) args;
    char name[CMON_ARG_KEY_MAX];
} cmon_argparse;

static inline const char * _get_key(cmon_argparse * _a, cmon_idx _arg)
{
    return strlen(_a->args[_arg].key_long) ? _a->args[_arg].key_long : _a->args[_arg].key_short;
}

cmon_argparse * cmon_argparse_create(cmon_allocator * _alloc, const char * _cmd_name)
{
    cmon_argparse * ret = CMON_CREATE(_alloc, cmon_argparse);
    ret->alloc = _alloc;
    cmon_dyn_arr_init(&ret->args, _alloc, 8);
    strcpy(ret->name, _cmd_name);
    return ret;
}

void cmon_argparse_destroy(cmon_argparse * _a)
{
    for(size_t i=0; i<cmon_dyn_arr_count(&_a->args); ++i)
    {
        for(size_t j=0; j<cmon_dyn_arr_count(&_a->args[i].possible_vals); ++j)
        {
            cmon_c_str_free(_a->alloc, _a->args[i].possible_vals[j]);
        }
        cmon_dyn_arr_dealloc(&_a->args[i].possible_vals);
    }
    cmon_dyn_arr_dealloc(&_a->args);
    CMON_DESTROY(_a->alloc, _a);
}

void cmon_argparse_parse(cmon_argparse * _a, const char ** _args, size_t _count)
{
    size_t i, j;
    // skip bin name hence start at 1
    for (i = 1; i < _count; ++i)
    {
        cmon_idx idx = CMON_INVALID_IDX;
        for (j = 0; j < cmon_dyn_arr_count(&_a->args); ++j)
        {
            if (strcmp(_args[i], _a->args[j].key_short) == 0 ||
                strcmp(_args[i], _a->args[j].key_long) == 0)
            {
                idx = j;
                break;
            }
        }

        if (cmon_is_valid_idx(idx))
        {
            _a->args[idx].was_set = cmon_true;
            if (_a->args[idx].expects_value)
            {
                if (i >= _count - 1)
                {
                    cmon_panic("expected value for argument '%s'\n", _a->args[idx].key_short);
                }

                const char * val = _args[++i];
                cmon_bool is_valid;
                if (cmon_dyn_arr_count(&_a->args[idx].possible_vals) && !_a->args[idx].accepts_any)
                {
                    is_valid = cmon_false;
                    for (j = 0; j < cmon_dyn_arr_count(&_a->args[idx].possible_vals); ++j)
                    {
                        if (strcmp(_a->args[idx].possible_vals[j], val) == 0)
                        {
                            is_valid = cmon_true;
                            break;
                        }
                    }
                }
                else
                {
                    is_valid = cmon_true;
                }

                if (!is_valid)
                {
                    cmon_panic("invalid value '%s' for argument '%s'\n", val, _get_key(_a, idx));
                }
                strcpy(_a->args[idx].val, val);
            }
        }
        else
        {
            cmon_panic("unknown argument '%s'\n", _args[i]);
        }
    }
}

cmon_idx cmon_argparse_add_arg(cmon_argparse * _a,
                               const char * _key_short,
                               const char * _key_long,
                               cmon_bool _expects_value,
                               const char * _help)
{
    _arg a;
    strcpy(a.key_short, _key_short);
    strcpy(a.key_long, _key_long);
    strcpy(a.help, _help);
    cmon_dyn_arr_init(&a.possible_vals, _a->alloc, 4);
    strcpy(a.val, "");
    a.default_idx = -1;
    a.expects_value = _expects_value;
    a.was_set = cmon_false;
    a.accepts_any = cmon_false;
    cmon_dyn_arr_append(&_a->args, a);
    return cmon_dyn_arr_count(&_a->args) - 1;
}

void cmon_argparse_add_possible_val(cmon_argparse * _a,
                                    cmon_idx _arg,
                                    const char * _val,
                                    cmon_bool _is_default)
{
    if (_is_default)
    {
        if (_a->args[_arg].default_idx != -1)
        {
            cmon_panic("'%s' already has a default value\n", _get_key(_a, _arg));
        }
        _a->args[_arg].default_idx = cmon_dyn_arr_count(&_a->args[_arg].possible_vals);
    }
    if (strcmp(_val, "?") == 0)
        _a->args[_arg].accepts_any = cmon_true;
    cmon_dyn_arr_append(&_a->args[_arg].possible_vals, cmon_c_str_copy(_a->alloc, _val));
    _a->args[_arg].expects_value = cmon_true;
}

const char * cmon_argparse_value(cmon_argparse * _a, const char * _key)
{
    cmon_idx a = cmon_argparse_find(_a, _key);
    if (cmon_is_valid_idx(a))
    {
        if (_a->args[a].was_set)
            return _a->args[a].val;
        else if (_a->args[a].default_idx != -1)
        {
            assert(cmon_dyn_arr_count(&_a->args[a].possible_vals));
            return _a->args[a].possible_vals[_a->args[a].default_idx];
        }
    }
    return NULL;
}

cmon_bool cmon_argparse_is_set(cmon_argparse * _a, const char * _key)
{
    cmon_idx a = cmon_argparse_find(_a, _key);
    if (cmon_is_valid_idx(a))
    {
        return _a->args[a].was_set;
    }
    return cmon_false;
}

cmon_idx cmon_argparse_find(cmon_argparse * _a, const char * _key)
{
    size_t i;
    for (i = 0; i < cmon_dyn_arr_count(&_a->args); ++i)
    {
        _arg * a = &_a->args[i];
        if (strcmp(_key, a->key_short) == 0 || strcmp(_key, a->key_long) == 0)
        {
            return i;
        }
    }

    return CMON_INVALID_IDX;
}

static inline int _arg_cmp(const void * _a, const void * _b)
{
    return strcmp(((const _arg*)_a)->key_short, ((const _arg*)_b)->key_short);
}

void cmon_argparse_print_help(cmon_argparse * _a)
{
    size_t i, j, count;
    static char s_whitespace[] = "                     ";
    printf("usage: %s [options]\n", _a->name);
    //@TODO: Is this a safe/good place to sort the args? maybe make a tmp copy of the args first?
    qsort(_a->args, cmon_dyn_arr_count(&_a->args), sizeof(_arg), _arg_cmp);
    for (i = 0; i < cmon_dyn_arr_count(&_a->args); ++i)
    {
        _arg * a = &_a->args[i];

        count = cmon_dyn_arr_count(&a->possible_vals);
        if (strlen(a->key_long))
        {
            printf("%-2s, %-16s %s\n", a->key_short, a->key_long, a->help);
        }
        else
        {
            printf("%-20s %s\n", a->key_short, a->help);
        }

        if (count)
        {
            printf("%spossible values: ", s_whitespace);
            for (j = 0; j < count; ++j)
            {
                printf("%s", a->possible_vals[j]);
                if (j < count - 1)
                    printf(", ");
            }
            printf("\n");
        }

        if (a->default_idx != -1)
        {
            printf("%sdefault: %s\n", s_whitespace, a->possible_vals[a->default_idx]);
        }
    }
}
