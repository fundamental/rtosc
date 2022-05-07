#include <rtosc/arg-ext.h>

typedef union
{
    int64_t store;
    struct {
        char type;
        int32_t len;
    } arr;
} rtosc_pack_arr;

extern inline char rtosc_arg_arr_type(const rtosc_arg_t *arg)
{
    rtosc_pack_arr pack;
    pack.store = arg->h;
    return pack.arr.type;
}

extern inline int32_t rtosc_arg_arr_len(const rtosc_arg_t *arg)
{
    rtosc_pack_arr pack;
    pack.store = arg->h;
    return pack.arr.len;
}

extern inline void rtosc_arg_arr_type_set(rtosc_arg_t *arg, char type)
{
    rtosc_pack_arr pack;
    pack.store = arg->h;
    pack.arr.type = type;
    arg->h = pack.store;
}

extern inline void rtosc_arg_arr_len_set(rtosc_arg_t *arg, int32_t len)
{
    rtosc_pack_arr pack;
    pack.store = arg->h;
    pack.arr.len = len;
    arg->h = pack.store;
}

typedef union
{
    int64_t store;
    struct {
        int32_t num;
        int32_t has_delta;
    } rep;
} rtosc_pack_rep;

extern inline int32_t rtosc_arg_rep_num(const rtosc_arg_t *arg)
{
    rtosc_pack_rep pack;
    pack.store = arg->h;
    return pack.rep.num;
}

extern inline int32_t rtosc_arg_rep_has_delta(const rtosc_arg_t *arg)
{
    rtosc_pack_rep pack;
    pack.store = arg->h;
    return pack.rep.has_delta;
}

extern inline void rtosc_arg_rep_num_set(rtosc_arg_t *arg, int32_t num)
{
    rtosc_pack_rep pack;
    pack.store = arg->h;
    pack.rep.num = num;
    arg->h = pack.store;
}

extern inline void rtosc_arg_rep_has_delta_set(rtosc_arg_t *arg, int32_t has_delta)
{
    rtosc_pack_rep pack;
    pack.store = arg->h;
    pack.rep.has_delta = has_delta;
    arg->h = pack.store;
}

/*
 * comfort functions
 */

extern inline char rtosc_av_arr_type(const rtosc_arg_val_t *arg)
{
    return rtosc_arg_arr_type(&arg->val);
}

extern inline int32_t rtosc_av_arr_len(const rtosc_arg_val_t *arg)
{
    return rtosc_arg_arr_len(&arg->val);
}

extern inline void rtosc_av_arr_type_set(rtosc_arg_val_t *arg, char type)
{
    rtosc_arg_arr_type_set(&arg->val, type);
}

extern inline void rtosc_av_arr_len_set(rtosc_arg_val_t *arg, int32_t len)
{
    rtosc_arg_arr_len_set(&arg->val, len);
}

extern inline int32_t rtosc_av_rep_num(const rtosc_arg_val_t *arg)
{
    return rtosc_arg_rep_num(&arg->val);
}

extern inline int32_t rtosc_av_rep_has_delta(const rtosc_arg_val_t *arg)
{
    return rtosc_arg_rep_has_delta(&arg->val);
}

extern inline void rtosc_av_rep_num_set(rtosc_arg_val_t *arg, int32_t num)
{
    rtosc_arg_rep_num_set(&arg->val, num);
}

extern inline void rtosc_av_rep_has_delta_set(rtosc_arg_val_t *arg, int32_t has_delta)
{
    rtosc_arg_rep_has_delta_set(&arg->val, has_delta);
}

