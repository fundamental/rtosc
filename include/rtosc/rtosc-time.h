/*
 * Copyright (c) 2017 Johannes Lorenz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef RTOSC_TIME
#define RTOSC_TIME

/**
 * @file rtosc-time.h
 * Functions and helper functions for conversion between time and arg vals
 */

#include <stdint.h>
#include <time.h>

#include <rtosc/rtosc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * creating arg vals
 */

rtosc_arg_val_t* rtosc_arg_val_from_time_t(rtosc_arg_val_t* dest, time_t time,
                                           uint64_t secfracs);
rtosc_arg_val_t* rtosc_arg_val_from_params(rtosc_arg_val_t* dest,
                                           struct tm *m_tm,
                                           uint64_t secfracs);
rtosc_arg_val_t* rtosc_arg_val_current_time(rtosc_arg_val_t* dest);
rtosc_arg_val_t* rtosc_arg_val_immediatelly(rtosc_arg_val_t* dest);

uint64_t rtosc_float2secfracs(float secfracsf);

/*
 * extracting arg vals
 */

time_t rtosct_time_t_from_arg_val(const rtosc_arg_val_t* arg);
struct tm *rtosct_params_from_arg_val(const rtosc_arg_val_t* arg);
uint64_t rtosct_secfracs_from_arg_val(const rtosc_arg_val_t* arg);
bool rtosc_arg_val_is_immediatelly(const rtosc_arg_val_t* arg);

float rtosc_secfracs2float(uint64_t secfracs);

#ifdef __cplusplus
}
#endif
#endif // RTOSC_TIME
