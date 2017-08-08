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
 * @file arg-val-math.h
 */

#ifndef ARG_VAL_MATH_H
#define ARG_VAL_MATH_H

#include <rtosc/rtosc.h>

int rtosc_arg_val_null(rtosc_arg_val_t* av, char type);
int rtosc_arg_val_from_int(rtosc_arg_val_t* av, char type, int number);
int rtosc_arg_val_negate(rtosc_arg_val_t *av);
int rtosc_arg_val_round(rtosc_arg_val_t *av);
int rtosc_arg_val_add(const rtosc_arg_val_t *lhs, const rtosc_arg_val_t *rhs,
                      rtosc_arg_val_t* res);
int rtosc_arg_val_sub(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                      rtosc_arg_val_t* res);
int rtosc_arg_val_mult(const rtosc_arg_val_t *lhs, const rtosc_arg_val_t *rhs,
                       rtosc_arg_val_t* res);
int rtosc_arg_val_div(const rtosc_arg_val_t *lhs, const rtosc_arg_val_t *rhs,
                      rtosc_arg_val_t* res);
int rtosc_arg_val_to_int(const rtosc_arg_val_t *av, int* res);

#endif // ARG_VAL_MATH_H
