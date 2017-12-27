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
 * @file bundle-foreach.h
 * Define the bundle_foreach template function
 * @note Include this file only from cpp files
 */

#ifndef BUNDLE_FOREACH
#define BUNDLE_FOREACH

#include <cctype>
#include <cstdlib>
#include <cstdio>

namespace rtosc {

/**
 * Execute a callback for all bundle elements of a bundle port
 * @param name Should be p.name
 * TODO: pass base as pointer, so it can be null?
 */
template<class F>
void bundle_foreach(const struct Port& p, const char* name, char* old_end,
                    /* data which is only being used by the ftors */
                    const char* name_buffer, const struct Ports& base,
                    void* data, void* runtime, const F& ftor,
                    /* options */
                    bool expand_bundles = true,
                    bool cut_afterwards = true)
{
    char       *pos  = old_end;
    while(*name != '#') *pos++ = *name++;
    const unsigned max = atoi(name+1);
    while(isdigit(*++name)) ;

    char* pos2;

    if(expand_bundles)
    for(unsigned i=0; i<max; ++i)
    {
        const char* name2_2 = name;
        pos2 = pos + sprintf(pos,"%d",i);

        // append everything behind the '#' (for cases like a#N/b)
        while(*name2_2 && *name2_2 != ':')
            *pos2++ = *name2_2++;

        ftor(&p, name_buffer, old_end, base, data, runtime);
    }
    else
    {
        const char* name2_2 = name;
        pos2 = pos;

        // append everything behind the '#' (for cases like a#N/b)
        while(*name2_2 && *name2_2 != ':')
            *pos2++ = *name2_2++;

        ftor(&p, name_buffer, old_end, base, data, runtime);
    }

    if(cut_afterwards)
        *old_end = 0;
    else
        *pos2 = 0;
}

// use this function if you don't want to do anything in bundle_foreach
// (useful to create paths if cut_afterwards is true)
inline void bundle_foreach_do_nothing(const Port*, const char*, const char*,
                                      const Ports&, void*, void*){}

}

#endif // BUNDLE_FOREACH
