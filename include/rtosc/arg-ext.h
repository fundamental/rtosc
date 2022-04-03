#ifndef RTOSC_ARG_EXT_H
#define RTOSC_ARG_EXT_H

#include <rtosc/rtosc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
   getters/setters for arg_t
 */
extern char rtosc_arg_arr_type(const rtosc_arg_t* arg);
extern int32_t rtosc_arg_arr_len(const rtosc_arg_t* arg);
extern void rtosc_arg_arr_type_set(rtosc_arg_t* arg, char type);
extern void rtosc_arg_arr_len_set(rtosc_arg_t* arg, int32_t len);

extern int32_t rtosc_arg_rep_num(const rtosc_arg_t* arg);
extern int32_t rtosc_arg_rep_has_delta(const rtosc_arg_t* arg);
extern void rtosc_arg_rep_num_set(rtosc_arg_t* arg, int32_t num);
extern void rtosc_arg_rep_has_delta_set(rtosc_arg_t* arg, int32_t has_delta);

/*
   getters/setters for arg_val_t
 */
extern char rtosc_av_arr_type(const rtosc_arg_val_t* arg);
extern int32_t rtosc_av_arr_len(const rtosc_arg_val_t* arg);
extern void rtosc_av_arr_type_set(rtosc_arg_val_t* arg, char type);
extern void rtosc_av_arr_len_set(rtosc_arg_val_t* arg, int32_t len);

extern int32_t rtosc_av_rep_num(const rtosc_arg_val_t* arg);
extern int32_t rtosc_av_rep_has_delta(const rtosc_arg_val_t* arg);
extern void rtosc_av_rep_num_set(rtosc_arg_val_t* arg, int32_t num);
extern void rtosc_av_rep_has_delta_set(rtosc_arg_val_t* arg, int32_t has_delta);

#ifdef __cplusplus
}
#endif

#endif // RTOSC_ARG_EXT_H
