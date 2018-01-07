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

 /**
 * @file arg-val-cmp.h
 * Two- and three-way comparison of argument values
 *
 * @test arg-val-cmp.c
 */

#ifndef ARG_VAL_CMP
#define ARG_VAL_CMP

#include <rtosc/rtosc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * arg val comparing helpers
 */
int rtosc_arg_vals_cmp_has_next(const rtosc_arg_val_itr* litr,
                                const rtosc_arg_val_itr* ritr,
                                size_t lsize, size_t rsize);
int rtosc_arg_vals_eq_after_abort(const rtosc_arg_val_itr* litr,
                                  const rtosc_arg_val_itr* ritr,
                                  size_t lsize, size_t rsize);
int rtosc_arg_vals_eq_single(const rtosc_arg_val_t* _lhs,
                             const rtosc_arg_val_t* _rhs,
                             const rtosc_cmp_options* opt);
int rtosc_arg_vals_cmp_single(const rtosc_arg_val_t* _lhs,
                              const rtosc_arg_val_t* _rhs,
                              const rtosc_cmp_options* opt);

/**
 * Check if two arrays of rtosc_arg_val_t are equal
 *
 * @param lsize Array size of lhs, e.g. 3 if lhs is just one counting range
 * @param opt Comparison options or NULL for default options
 * @return One if they are equal, zero if not
 */
int rtosc_arg_vals_eq(const rtosc_arg_val_t *lhs, const rtosc_arg_val_t *rhs,
                      size_t lsize, size_t rsize,
                      const rtosc_cmp_options* opt);

/**
 * Compare two arrays of rtosc_arg_val_t.
 * Whether an argument value is less or greater than another is computed
 * - using memcmp for blobs
 * - using strcmp for strings and identifiers
 * - using numerical comparison for all other types.
 * As an exception, the timestamp "immediately" is defined to be smaller than
 * every other timestamp.
 *
 * @param opt Comparison options or NULL for default options
 * @param lsize Array size of lhs, e.g. 3 if lhs is just one counting range
 * @return An integer less than, equal to, or greater than zero if lhs is found,
 *         respectively, to be less than, to match, or be greater than rhs.
 */
int rtosc_arg_vals_cmp(const rtosc_arg_val_t *lhs, const rtosc_arg_val_t *rhs,
                       size_t lsize, size_t rsize,
                       const rtosc_cmp_options* opt);

#ifdef __cplusplus
};
#endif
#endif // ARG_VAL_CMP
