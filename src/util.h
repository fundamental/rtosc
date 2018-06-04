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
 * @file util.h
 * Utilities shared by rtosc functions
 *
 * @test util.c
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copy string to another memory location, including the terminating zero byte
 * @param dest Destination memory location
 * @param src Source string
 * @param buffersize Maximal number of bytes that you can write to @p dest
 * @return A pointer to @p dest
 * @warning @p dest and @p src shall not overlap
 * @warning if buffersize is larger than strlen(src)+1, unused bytes in @p dest
 *   are not overwritten. Secure information may be released. Don't use this if
 *   you want to send the string somewhere else, e.g. via IPC.
 */
char *fast_strcpy(char *dest, const char *src, size_t buffersize);

#ifdef __cplusplus
}
#endif

#endif // UTIL_H
