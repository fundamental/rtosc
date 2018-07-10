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
 * @file ports-runtime.h Functions to retrieve runtime values from ports
 */

#ifndef PORTS_RUNTIME
#define PORTS_RUNTIME

#include <cstddef>
#include <rtosc/rtosc.h>

namespace rtosc {
namespace helpers {

/**
 * @brief Returns a port's value pretty-printed from a runtime object. The
 *        port object must not be known.
 *
 * For the parameters, see the overloaded function
 * @return The argument values, pretty-printed
 */
const char* get_value_from_runtime(void* runtime, const struct Ports& ports,
                                   size_t loc_size, char* loc,
                                   char* buffer_with_port,
                                   std::size_t buffersize,
                                   int cols_used);

/**
 * @brief Returns a port's current value(s)
 *
 * This function returns the value(s) of a known port object and stores them as
 * rtosc_arg_val_t.
 * @param runtime The runtime object
 * @param port the port where loc is relative to
 * @param loc A buffer where dispatch can write down the currently dispatched
 *   path
 * @param loc_size Size of loc
 * @param portname_from_base The name of the port, relative to its base
 * @param buffer_with_port A buffer which already contains the port.
 *   This buffer will be modified and must at least have space for 8 more bytes.
 * @param buffersize Size of @p buffer_with_port
 * @param max_args Maximum capacity of @p arg_vals
 * @param arg_vals Argument buffer for returned argument values
 * @return The number of argument values stored in @p arg_vals
 */
size_t get_value_from_runtime(void* runtime, const struct Port& port,
                              size_t loc_size, char* loc,
                              const char* portname_from_base,
                              char* buffer_with_port, std::size_t buffersize,
                              std::size_t max_args, rtosc_arg_val_t* arg_vals);

// TODO: loc should probably not be passed,
//       since it can be allocated in constant time?
// TODO: clean up those funcs:
// * use dispatch instead of cb
// * don't pass loc etc

}
}

#endif // PORTS_RUNTIME
