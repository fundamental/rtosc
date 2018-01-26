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
 * @file default-value.h
 * Functions for retrieving default values from metadata (and runtime)
 *
 * @test default-value.cpp
 */

#ifndef RTOSC_DEFAULT_VALUE
#define RTOSC_DEFAULT_VALUE

#include <cstdint>
#include <rtosc/rtosc.h>

namespace rtosc {

/**
 * Return a port's default value
 *
 * Returns the default value of a given port, if any exists, as a string.
 * For the parameters, see the overloaded function.
 * @note The recursive parameter should never be specified.
 * @return The default value(s), pretty-printed, or NULL if there is no
 *     valid default annotation
 */
const char* get_default_value(const char* port_name, const struct Ports& ports,
                              void* runtime,
                              const struct Port* port_hint = nullptr,
                              int32_t idx = -1, int recursive = 1);

/**
 * Return a port's default value
 *
 * Returns the default value of a given port, if any exists, as an array of
 * rtosc_arg_vals . The values in the resulting array are being canonicalized,
 * i.e. mapped values are being converted to integers; see
 * canonicalize_arg_vals() .
 *
 * @param port_name the port's OSC path.
 * @param port_args the port's arguments, e.g. '::i:c:S'
 * @param ports the ports where @a portname is to be searched
 * @param runtime object holding @a ports . Optional. Helps finding
 *        default values dependent on others, such as presets.
 * @param port_hint The port itself corresponding to portname (including
 *        the args). If not specified, will be found using @p portname .
 * @param idx If the port is an array (using the '#' notation), this specifies
 *        the index required for the default value
 * @param n Size of the output parameter @res . This size can not be known,
 *        so you should provide a large enough array.
 * @param res The output parameter for the argument values.
 * @param strbuf String buffer for storing pretty printed strings and blobs.
 * @param strbufsize Size of @p strbuf
 * @return The actual number of aruments written to @p res (can be smaller
 *         than @p n) or -1 if there is no valid default annotation
 */
int get_default_value(const char* port_name, const char *port_args,
                      const struct Ports& ports,
                      void* runtime, const struct Port* port_hint,
                      int32_t idx,
                      std::size_t n, rtosc_arg_val_t* res,
                      char *strbuf, size_t strbufsize);

}

#endif // RTOSC_DEFAULT_VALUE
