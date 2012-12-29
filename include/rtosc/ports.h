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

#include <array>
#include <functional>
#include <initializer_list>
#include <rtosc/rtosc.h>
#include <string.h>

namespace rtosc {

//First define all types
typedef const char *msg_t;

class mPorts;//metaclass for a collection of ports
class mPort; //metaclass for a port


/**
 * This is a non-compliant pattern matcher for dispatching OSC messages
 *
 * Overall the pattern specification is
 *   (normal-path)(\#digit-specifier)?(/)?(:argument-restrictor)*
 *
 * @param pattern The pattern string stored in the Port
 * @param msg     The OSC message to be matched
 * @returns 3 if a path match, 1 if a normal match, and 0 if unmatched
 */
inline int match(const char *pattern, const char *msg)
{
    const char *_msg = msg;
    bool path_flag   = false;

    unsigned val, max;

normal:; //Match character by character or hop to speical cases

    //Check for special characters
    if(*pattern == ':') {
        ++pattern;
        goto args;
    }

    if(*pattern == '#') {
        ++pattern;
        goto number;
    }

    if(*pattern == '/' && *msg == '/') {
        path_flag  = 1;
        ++pattern;
        if(*pattern == ':') {
            ++pattern;
            goto args;
        }
        else
            return 3;
    }

    //Verify they are still the same and return if both are fully parsed
    if((*pattern == *msg)) {
        if(*msg)
            ++pattern, ++msg;
        else
            return (path_flag<<1)|1;
        goto normal;
    } else
        return false;

number:; //Match the number

    //Verify both hold digits
    if(!isdigit(*pattern) || !isdigit(*msg))
        return false;

    //Read in both numeric values
    max = atoi(pattern);
    val = atoi(msg);

    //Match iff msg number is strictly less than pattern
    if(val < max) {

        //Advance pointers
        while(isdigit(*pattern))++pattern;
        while(isdigit(*msg))++msg;

        goto normal;
    } else
        return false;

args:; //Match the arg string or fail

    const char *arg_str = rtosc_argument_string(_msg);

    bool arg_match = *pattern || *pattern == *arg_str;
    while(*pattern) {
        if(*pattern==':') {
            if(arg_match && !*arg_str)
                return (path_flag<<1)|1;
            else {
                ++pattern;
                goto args; //retry
            }
        }
        arg_match &= (*pattern++==*arg_str++);
    }

    if(arg_match)
        return (path_flag<<1)|1;
    return false;
}


class mPort
{
    public:
        mPort(const char*_n, const char *m, mPorts *_p=NULL)
            :name(_n),metadata(m),ports(_p){}
        mPort(const char*_n, mPorts *_p)
            :name(_n),metadata(""),ports(_p){}
        const char *name;
        const char *metadata;
        mPorts *ports;
};

template<class T>
class Port:public mPort {
    public:
        Port(const char*_n, const char* _m, std::function<void(msg_t, T*)> _f)
            :mPort(_n, _m, NULL), cb(_f)
        {}
        Port(const char*_n, mPorts *_p, std::function<void(msg_t, T*)> _f)
            :mPort(_n, _p), cb(_f)
        {}
        Port(const char*_n, const char *_m, mPorts *_p, std::function<void(msg_t, T*)> _f)
            :mPort(_n, _m, _p), cb(_f)
        {}
        Port(const Port<T> &port)
            :mPort(port.name,port.metadata,port.ports),cb(port.cb){}
        std::function<void(msg_t, T*)> cb;
};

class mPorts
{
    public:
        virtual const mPort &port(unsigned i) const=0;
        virtual unsigned     nports(void)     const=0;

        //Lookup a port without respect to arguments
        //This only applies to leaf nodes
        virtual const mPort *operator[](const char *) const=0;
        virtual const char *meta_data(const char *path) const=0;
        virtual const mPort *apropos(const char*path) const = 0;
        virtual void dispatchCast(msg_t m, void *v) = 0;
};

template<int len, class T>
class Ports : public mPorts
{
    public:
        std::array<Port<T>,len> ports;


        const mPort &port(unsigned i) const
        {
            return ports[i];
        }
        unsigned nports(void) const
        {
            return len;
        }
        const Port<T> &operator[](unsigned i)const
        {
            return ports[i];
        }

        Ports(const std::array<Port<T>,len> &data) : ports(data) {}

        Ports(const Ports&) = delete;

        void dispatch(msg_t m, T*t)
        {
            for(Port<T> &port: ports) {
                if(match(port.name,m))
                    port.cb(m,t);
            }
        }

        void dispatchCast(msg_t m, void *v)
        {
            dispatch(m, (T*)v);
        }

        const mPort *operator[](const char *name) const
        {
            for(const Port<T> &port:ports) {
                const char *_needle = name,
                           *_haystack = port.name;
                while(*_needle && *_needle==*_haystack)_needle++,_haystack++;

                if(*_needle == 0 && *_haystack == ':') {
                    return &port;
                }
            }
            return NULL;
        }

        msg_t snip(msg_t m) const
        {
            while(*m && *m != '/') ++m;
            return m+1;
        }

        //Generate the deepest working match
        const mPort *apropos(const char *path) const
        {
            for(const Port<T> &port: ports)
                if(index(port.name,'/') && match(port.name,path))
                    return port.ports->apropos(this->snip(path));

            //This is the lowest level, now find the best port
            for(const Port<T> &port: ports)
                if(strstr(port.name, path)==port.name)
                    return &port;

            return NULL;
        }

        const char *meta_data(const char *path) const
        {
            for(const Port<T> &port: ports) {
                const char *p = mmatch(port.name,path);
                if(*p == '/') {
                    ++path;
                    while(*++path!='/');
                    return port.ports->meta_data(path+1);
                }
                if(*p == ':') {
                    return port.metadata;
                }
            }
            puts("failed to get meta-data...");
            return "";
        }

        /**
         * Match partial path
         */
        inline const char *mmatch(const char *pattern, const char *path) const
        {
            for(;*pattern&&*path&&*path!=':'&&*path!='/'&&*path==*pattern;++path,++pattern);
            return pattern;
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

typedef std::function<void(const mPort*,const char*)> port_walker_t;

static __attribute__((unused))
void walk_ports(const mPorts *base,
                char         *name_buffer,
                size_t        buffer_size,
                port_walker_t walker)
{
    if(name_buffer[0] == 0)
        name_buffer[0] = '/';

    const unsigned nports = base->nports();
    char *old_end         = name_buffer;
    while(*old_end) ++old_end;

    for(unsigned i=0; i<nports; ++i) {
        const mPort &p = base->port(i);

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
