#include "../../include/rtosc/ports.h"
#include <cassert>

using namespace rtosc;

static void scat(char *dest, const char *src)
{
    while(*dest) dest++;
    if(*dest) dest++;
    while(*src && *src!=':') *dest++ = *src++;
    *dest = 0;
}

RtData::RtData(void)
    :loc(NULL), loc_size(0), obj(NULL), matches(0)
{}

void RtData::reply(const char *path, const char *args, ...){(void)path;(void)args;};
void RtData::reply(const char *msg){(void)msg;};
void RtData::broadcast(const char *path, const char *args, ...){(void)path;(void)args;};
void RtData::broadcast(const char *msg){(void)msg;};

void metaiterator_advance(const char *&title, const char *&value)
{
    if(!title || !*title) {
        value = NULL;
        return;
    }

    //Try to find "\0=" after title string
    value = title;
    while(*value)
        ++value;
    if(*++value != '=')
        value = NULL;
    else
        value++;
}

Port::MetaIterator::MetaIterator(const char *str)
    :title(str), value(NULL)
{
    metaiterator_advance(title, value);
}

Port::MetaIterator& Port::MetaIterator::operator++(void)
{
    if(!title || !*title) {
        title = NULL;
        return *this;
    }
    //search for next parameter start
    //aka "\0:" unless "\0\0" is seen
    char prev = 0;
    while(prev || (*title && *title != ':'))
        prev = *title++;

    if(!*title)
        title = NULL;
    else
        ++title;

    metaiterator_advance(title, value);
    return *this;
}

Port::MetaContainer::MetaContainer(const char *str_)
:str_ptr(str_)
{}

Port::MetaIterator Port::MetaContainer::begin(void) const
{
    if(str_ptr && *str_ptr == ':')
        return Port::MetaIterator(str_ptr+1);
    else
        return Port::MetaIterator(str_ptr);
}

Port::MetaIterator Port::MetaContainer::end(void) const
{
    return MetaIterator(NULL);
}

Port::MetaIterator Port::MetaContainer::find(const char *str) const
{
    for(const auto x : *this)
        if(!strcmp(x.title, str))
            return x;
    return NULL;
}

size_t Port::MetaContainer::length(void) const
{
        if(!str_ptr || !*str_ptr)
            return 0;
        char prev = 0;
        const char *itr = str_ptr;
        while(prev || *itr)
            prev = *itr++;
        return 2+(itr-str_ptr);
}

const char *Port::MetaContainer::operator[](const char *str) const
{
    for(const auto x : *this)
        if(!strcmp(x.title, str))
            return x.value;
    return NULL;
}
//Match the arg string or fail
inline bool arg_matcher(const char *pattern, const char *args)
{
    //match anything if now arg restriction is present (ie the ':')
    if(*pattern++ != ':')
        return true;

    const char *arg_str = args;
    bool      arg_match = *pattern || *pattern == *arg_str;

    while(*pattern && *pattern != ':')
        arg_match &= (*pattern++==*arg_str++);

    if(*pattern==':') {
        if(arg_match && !*arg_str)
            return true;
        else
            return arg_matcher(pattern, args); //retry
    }

    return arg_match;
}

inline bool scmp(const char *a, const char *b)
{
    while(*a && *a == *b) a++, b++;
    return a[0] == b[0];
}

namespace rtosc{
class Port_Matcher
{
    public:

        const char *pattern;

        bool fast;
        char literal[128];
        unsigned literal_len;
        char args[128];

        void detectFast(void)
        {
            const char *pat = pattern;
            if(!index(pat, '#') && !index(pat, '/')) {
                char *ptr = literal;
                while(*pat && *pat != ':')
                    *ptr++ = *pat++;
                *ptr = 0;
                literal_len = strlen(literal);
                *args = 0;
                strcpy(args, pat);
                fast = true;
            } else
                fast = false;
        }

        inline bool match(const char *path, const char *args_, bool long_enough) const
        {
            if(fast && long_enough) {
                return (*(int32_t*)literal)==(*(int32_t*)path) && scmp(literal+4, path+4) && arg_matcher(args, args_);
            } else if(fast) {
                return scmp(literal, path) && arg_matcher(args, args_);
            } else //no precompilation was done...
                return rtosc_match(pattern, path);
        }
};
}

Ports::Ports(std::initializer_list<Port> l)
    :ports(l), impl(new Port_Matcher[ports.size()])
{
    unsigned nfast = 0;
    for(unsigned i=0; i<ports.size(); ++i) {
        impl[i].pattern = ports[i].name;
        impl[i].detectFast();
        nfast += impl[i].fast;
    }

    elms = ports.size();
    unambigious = true;

    if(16 < ports.size() && ports.size() <= 64) {
        use_mask = true;

        //if ANY ambigious character was encountered in a given port
        bool special[64];
        char freq[255];//character freqeuency table
        memset(special, 0, 64);

        for(int i=0; i<4; ++i) {
            memset(freq, 0, 255);
            for(unsigned j=0; j<ports.size(); ++j) {
                if(strlen(ports[j].name) < (size_t)i || special[j])
                    continue;
                if(ports[j].name[i] == ':' || ports[j].name[i] == '#') {
                    special[j] = true;
                    unambigious = false;
                    continue;
                }
                freq[(int)ports[j].name[i]]++;
            }

            //Find the most frequent character which *should* make the best
            //discriminting function under a _reasonable_ set of assumptions
            char best=0;
            char votes=0;
            for(int j=0; j<255; ++j) {
                if(votes < freq[j]) {
                    best  = j;
                    votes = freq[j];
                }
            }

            mask_chars[i] = best;
            //Generate the masks depending on the results of the comparison
            masks[2*i]   = 0x0; //true mask
            masks[2*i+1] = 0x0; //false mask
            for(unsigned j=0; j < ports.size(); ++j) {
                bool tbit = strlen(ports[j].name) < (size_t)i || special[j] ||
                    ports[j].name[i] == best;
                bool fbit = strlen(ports[j].name) < (size_t)i || special[j] ||
                    ports[j].name[i] != best;
                masks[2*i]   |=  (tbit<<j);
                masks[2*i+1] |=  (fbit<<j);

            }
        }
    }
}

Ports::~Ports()
{
    delete [] impl;
}

#if !defined(__GNUC__)
#define __builtin_expect(a,b) a
#endif

void Ports::dispatch(const char *m, rtosc::RtData &d) const
{
    void *obj = d.obj;
    const char *args = rtosc_argument_string(m);
    //simple case
    if(!d.loc || !d.loc_size) {
        for(const Port &port: ports) {
            if(rtosc_match(port.name,m))
                d.port = &port, port.cb(m,d), d.obj = obj;
        }
    } else {

        //TODO this function is certainly buggy at the moment, some tests
        //are needed to make it clean
        //XXX buffer_size is not properly handled yet
        if(__builtin_expect(d.loc[0] == 0, 0)) {
            memset(d.loc, 0, d.loc_size);
            d.loc[0] = '/';
        }

        uint64_t mask = 0xffffffffffffffff;
        bool fast_path = (m[0] && m[1] && m[2] && m[3]);
        if(use_mask && __builtin_expect(fast_path, 1)) {
            if(unambigious)
                mask = (m[0]==mask_chars[0] ? masks[0] : ~masks[0])
                     & (m[1]==mask_chars[1] ? masks[2] : ~masks[2])
                     & (m[2]==mask_chars[2] ? masks[4] : ~masks[4])
                     & (m[3]==mask_chars[3] ? masks[6] : ~masks[6]);
            else
                mask = (m[0]==mask_chars[0] ? masks[0] : masks[1])
                     & (m[1]==mask_chars[1] ? masks[2] : masks[3])
                     & (m[2]==mask_chars[2] ? masks[4] : masks[5])
                     & (m[3]==mask_chars[3] ? masks[6] : masks[7]);
        }

        char *old_end = d.loc;
        while(*old_end) ++old_end;

        unsigned i=0;
        if(!(mask&0xffff)) { //skip ahead when possible
            i = 16;
            mask >>= 16;
        }
        for(; i<elms; ++i, mask >>= 1) {
            if(__builtin_expect(!(mask&1) || !impl[i].match(m, args, fast_path), 1))
                continue;

            const Port &port = ports[i];
            if(!port.ports)
                d.matches++;

            //Append the path
            if(index(port.name,'#')) {
                const char *msg = m;
                char       *pos = old_end;
                while(*msg && *msg != '/')
                    *pos++ = *msg++;
                if(index(port.name, '/'))
                    *pos++ = '/';
                *pos = '\0';
            } else
                scat(d.loc, port.name);

            d.port = &port;

            //Apply callback
            port.cb(m,d), d.obj = obj;

            //Remove the rest of the path
            char *tmp = old_end;
            while(*tmp) *tmp++=0;
        }
    }
}

const Port *Ports::operator[](const char *name) const
{
    for(const Port &port:ports) {
        const char *_needle = name,
              *_haystack = port.name;
        while(*_needle && *_needle==*_haystack)_needle++,_haystack++;

        if(*_needle == 0 && (*_haystack == ':' || *_haystack == '\0')) {
            return &port;
        }
    }
    return NULL;
}

msg_t snip(msg_t m)
{
    while(*m && *m != '/') ++m;
    return m+1;
}

const Port *Ports::apropos(const char *path) const
{
    if(path && path[0] == '/')
        ++path;

    for(const Port &port: ports)
        if(index(port.name,'/') && rtosc_match_path(port.name,path))
            return (index(path,'/')[1]==0) ? &port :
                port.ports->apropos(snip(path));

    //This is the lowest level, now find the best port
    for(const Port &port: ports)
        if(*path && strstr(port.name, path)==port.name)
            return &port;

    return NULL;
}

void rtosc::walk_ports(const Ports *base,
                       char         *name_buffer,
                       size_t        buffer_size,
                       void         *data,
                       port_walker_t walker)
{
    assert(name_buffer);
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
                    rtosc::walk_ports(p.ports, name_buffer, buffer_size,
                            data, walker);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Recurse
                rtosc::walk_ports(p.ports, name_buffer, buffer_size,
                        data, walker);
            }
        } else {
            if(index(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"%d",i);

                    //Apply walker function
                    walker(&p, name_buffer, data);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Apply walker function
                walker(&p, name_buffer, data);
            }
        }

        //Remove the rest of the path
        char *tmp = old_end;
        while(*tmp) *tmp++=0;
    }
}

