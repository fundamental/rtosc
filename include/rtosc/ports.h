/*
 * Copyright (c) 2012 Mark McCurry
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

#ifndef RTOSC_PORTS
#define RTOSC_PORTS

#include <vector>
#include <functional>
#include <initializer_list>
#include <rtosc/rtosc.h>
#include <rtosc/matcher.h>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>

namespace rtosc {

//First define all types
typedef const char *msg_t;

struct Port;
struct Ports;

struct RtData
{
    char *loc;
    size_t loc_size;
    void *obj;
};

/**
 * Port in rtosc dispatching hierarchy
 */
struct Port {
        const char *name;    //< Pattern for messages to match
        const char *metadata;//< Statically accessable data about port
        Ports *ports;        //< Pointer to further ports
        std::function<void(msg_t, RtData)> cb;//< Callback for matching functions
};

static void scat(char *dest, const char *src);

/**
 * Ports - a dispatchable collection of Port entries
 *
 * This structure makes it somewhat easier to perform actions on collections of
 * port entries and it is responsible for the dispatching of OSC messages to
 * their respective ports.
 * That said, it is a very simple structure, which uses a stl container to store
 * all data in a simple dispatch table.
 * All methods post-initialization are RT safe (assuming callbacks are RT safe)
 */
struct Ports
{
    std::vector<Port> ports;

    /**Forwards to builtin container*/
    auto begin() const -> decltype(ports.begin())
    {
        return ports.begin();
    }

    /**Forwards to builtin container*/
    auto end() const -> decltype(ports.end())
    {
        return ports.end();
    }

    /**Forwards to builtin container*/
    const Port &operator[](unsigned i) const
    {
        return ports[i];
    }

    Ports(std::initializer_list<Port> l)
        :ports(l)
    {}

    Ports(const Ports&) = delete;

    /**
     * Dispatches message to all matching ports.
     * This uses simple pattern matching available in rtosc::match.
     *
     * @param loc      A buffer for storing path information or NULL
     * @param loc_size The length of the provided buffer
     * @param m        a valid OSC message
     * @param v        a pointer to data or NULL
     */
    void dispatch(char *loc, size_t loc_size, msg_t m, void *v)
    {
        //simple case [very very cheap]
        if(!loc || !loc_size) {
            for(Port &port: ports) {
                if(rtosc_match(port.name,m))
                    port.cb(m,{NULL,0,v});
            }
        } else { //somewhat cheap

            //TODO this function is certainly buggy at the moment, some tests
            //are needed to make it clean
            //XXX buffer_size is not properly handled yet
            if(loc[0] == 0)
                loc[0] = '/';

            char *old_end = loc;
            while(*old_end) ++old_end;

            for(const Port &port: ports) {
                if(!rtosc_match(port.name, m))
                    continue;

                //Append the path
                if(index(port.name,'#')) {
                    const char *msg = m;
                    char       *pos = old_end;
                    while(*msg && *msg != '/')
                        *pos++ = *msg++;
                    *pos = '/';
                } else
                    scat(loc, port.name);

                //Apply callback
                port.cb(m,{loc,loc_size,v});

                //Remove the rest of the path
                char *tmp = old_end;
                while(*tmp) *tmp++=0;
            }
        }
    }

    /**
     * Retrieve local port by name
     * TODO implement full matching
     */
    const Port *operator[](const char *name) const
    {
        for(const Port &port:ports) {
            const char *_needle = name,
                  *_haystack = port.name;
            while(*_needle && *_needle==*_haystack)_needle++,_haystack++;

            if(*_needle == 0 && *_haystack == ':') {
                return &port;
            }
        }
        return NULL;
    }

    //util
    msg_t snip(msg_t m) const
    {
        while(*m && *m != '/') ++m;
        return m+1;
    }

    /** Find the best match for a given path or NULL*/
    const Port *apropos(const char *path) const
    {
        if(path && path[0] == '/')
            ++path;

        for(const Port &port: ports)
            if(index(port.name,'/') && rtosc_match_path(port.name,path))
                return (index(path,'/')[1]==0) ? &port :
                    port.ports->apropos(this->snip(path));

        //This is the lowest level, now find the best port
        for(const Port &port: ports)
            if(*path && strstr(port.name, path)==port.name)
                return &port;

        return NULL;
    }
};


/*********************
 * Port walking code *
 *********************/
static void scat(char *dest, const char *src)
{
    while(*dest) dest++;
    if(*dest) dest++;
    while(*src && *src!=':') *dest++ = *src++;
    *dest = 0;
}

typedef std::function<void(const Port*,const char*)> port_walker_t;

static
void walk_ports(const Ports *base,
        char         *name_buffer,
        size_t        buffer_size,
        port_walker_t walker)
{
    //XXX buffer_size is not properly handled yet
    if(name_buffer[0] == 0)
        name_buffer[0] = '/';

    char *old_end         = name_buffer;
    while(*old_end) ++old_end;

    for(const Port &p: *base) {
        if(index(p.name, '/')) {//it is another tree
            if(index(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"%d",i);

                    //Ensure the result is a path
                    if(rindex(name_buffer, '/')[1] != '/')
                        strcat(name_buffer, "/");

                    //Recurse
                    walk_ports(p.ports, name_buffer, buffer_size, walker);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Recurse
                walk_ports(p.ports, name_buffer, buffer_size, walker);
            }
        } else {
            //Append the path
            scat(name_buffer, p.name);

            //Apply walker function
            walker(&p, name_buffer);
        }

        //Remove the rest of the path
        char *tmp = old_end;
        while(*tmp) *tmp++=0;
    }
}
};


#endif
