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

/**
 * @brief           Create rtosc_arg_val_t from time_t and additional
 *                  fractions of seconds
 *
 * @param dest      Where to store the result
 * @param time      The input time_t time
 * @param secfracs  "Fractional parts of a second", as specified by the OSC
 *                  specs. To get this integer from a float value, see
 *                  rtosc_float2secfracs.
 * @return          The same as dest
 */
rtosc_arg_val_t* rtosc_arg_val_from_time_t(rtosc_arg_val_t* dest, time_t time,
                                           uint64_t secfracs);

//! @brief Same as rtosc_arg_val_from_time_t, but with `struct tm` input params
rtosc_arg_val_t* rtosc_arg_val_from_params(rtosc_arg_val_t* dest,
                                           struct tm *m_tm,
                                           uint64_t secfracs);

//! @brief Same as rtosc_arg_val_from_time_t, but returns the current time
rtosc_arg_val_t* rtosc_arg_val_current_time(rtosc_arg_val_t* dest);

/**
 * @brief       Create a time tag with the "immediatelly" value.
 * @note        This is different to the current time, see OSC specs
 * @param dest  Where to store the result
 * @return      The same as dest
 */
rtosc_arg_val_t* rtosc_arg_val_immediatelly(rtosc_arg_val_t* dest);

/**
 * @brief       Convert time represented by float into OSC secfracs
 *
 *              Convert a float number in [0,1) into an integer in
 *              [0,2^32). The conversion maps 0 -> 0, 1^-32 -> 1
 *              and ~1 -> 2^32-1 in a linear way.
 *
 *              If the input represents an amount of seconds, then the output
 *              can be interpreted as "fractional parts of a second", as
 *              required by the OSC spec and especially by
 *              rtosc_arg_val_from_time_t.
 *
 * @param       The input number as a float, in range [0,1).
 * @return      The second fraction. Always in range [0,2^32).
 */
uint64_t rtosc_float2secfracs(float secfracsf);

/*
 * extracting arg vals
 */

/**
 * @brief       Return the integer part represented by a time arg val in time_t
 * @param arg   The arg val
 * @return      The number of seconds in time_t
 */
time_t rtosc_time_t_from_arg_val(const rtosc_arg_val_t* arg);

/**
 * @brief       Return the integer part represented by a time arg val in
 *              struct tm
 * @param arg   The arg val
 * @return      The number of seconds in struct tm
 * @warning     Not thread safe! This function uses localtime().
 */
struct tm *rtosc_params_from_arg_val(const rtosc_arg_val_t* arg);

/**
 * @brief       Return the fractional part represented by a time arg val as
 *              "fractional parts of a second"
 * @param arg   The arg val
 * @return      The "fractional parts of a second". This integer is in range
 *              [0,2^32), as required in the OSC specs. To convert the result
 *              futher into a float value, see rtosc_secfracs2float.
 */
uint64_t rtosc_secfracs_from_arg_val(const rtosc_arg_val_t* arg);
bool rtosc_arg_val_is_immediatelly(const rtosc_arg_val_t* arg);

/**
 * @brief       Convert time represented by OSC secfracs into float
 *
 *              Convert an integer in [0,2^32) into a float number in [0,1).
 *              The conversion maps 0 -> 0, 1 -> 1^-32 and 2^32-1 -> ~1 in a
 *              linear way.
 *
 *              If the input represents "fractional parts of a second", as
 *              required by the OSC spec and especially returned by
 *              rtosc_secfracs_from_arg_val, then the output can be
 *              interpreted as an amount of seconds.
 *
 * @param       The second fraction, in range [0,2^32).
 * @return      The float number. Always in range [0,1).
 */
float rtosc_secfracs2float(uint64_t secfracs);

#ifdef __cplusplus
}
#endif
#endif // RTOSC_TIME
