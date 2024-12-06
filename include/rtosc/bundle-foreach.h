/*
 * Copyright (c) 2017-2024 Johannes Lorenz
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
 * @file bundle-foreach.h
 * Define the bundle_foreach template function
 * @note Include this file only from cpp files
 */

#ifndef BUNDLE_FOREACH
#define BUNDLE_FOREACH

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include "ports.h"

namespace rtosc {

/*
                          Ports& base
                               |p.name
                               vv
               "/path/from/root/new#2/path#2/to#2/"

               "/path/from/root/new1/path1/"
                ^               ^
   name_buffer[buffer_size]     old_end

*/
/**
 * Execute a callback for all bundle elements of a bundle port
 * @param p the bundle port
 * @param name Should be p.name
 * @param old_end points at end of name_buffer
 * @param name_buffer functor-data: buffer containing the path until and including `base`
 * @param base functor-data: base Ports struct containing `p`
 * @param data functor-data: arbitrary data for the functor
 * @param runtime functor-data: the OSC runtime object
 * @param ftor the functor to be called for each port of the bundle
 * @param expand_bundles if false, don't iterate, just print name without numbers
 * @param cut_afterwards if true, before returning, the string is cut at old_end
 * @param ranges don't iterate, just print `[0,max]`
 *
 * TODO: pass base as pointer, so it can be null?
 */
template<class F>
void bundle_foreach(const struct Port& p, const char* name, char* const old_end,
                    const char* const name_buffer, size_t buffer_size,
                    /* data which is only being used by the ftors */
                    const struct Ports& base,
                    void* const data, void* const runtime, const F& ftor,
                    /* options */
                    const bool expand_bundles = true,
                    const bool cut_afterwards = true,
                    const bool ranges = false)
{
    ssize_t space_left = buffer_size - (ssize_t)(old_end - name_buffer);
#ifdef NDEBUG
    (void)space_left;
#endif
    assert(space_left > 0);
    assert(space_left <= (ssize_t)buffer_size);

    char *pos = old_end;
    while(*name != '#') { assert(space_left); *pos++ = *name++; --space_left; }
    const unsigned max = (!expand_bundles || ranges) ? 1 : atoi(name+1);
    while(isdigit(*++name)) ;

    char* pos2 = pos;

    const char* name_precheck = name;
    while(*name_precheck && *name_precheck != ':')
        name_precheck++;
    assert(space_left > 16 + (ssize_t)(name_precheck - name) + 1);

    for(unsigned i=0; i<max; ++i)
    {
        const char* name2_2 = name;
        if (ranges)
            pos2 += snprintf(pos,16,"[0,%d]",max-1);
        else if(expand_bundles)
            pos2 = pos + snprintf(pos,16,"%d",i);

        // append everything behind the '#' (for cases like a#N/b)
        while(*name2_2 && *name2_2 != ':')
            *pos2++ = *name2_2++;
        *pos2 = 0;

        ftor(&p, name_buffer, old_end, base, data, runtime);
    }

    *(cut_afterwards ? old_end : pos2) = 0;
}

// use this function if you don't want to do anything in bundle_foreach
// (useful to create paths if cut_afterwards is true)
inline void bundle_foreach_do_nothing(const Port*, const char*, const char*,
                                      const Ports&, void*, void*){}

}

#endif // BUNDLE_FOREACH
