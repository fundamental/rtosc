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

void Ports::dispatch(const char *m, rtosc::RtData &d) const
{
    void *obj = d.obj;
    //simple case [very very cheap]
    if(!d.loc || !d.loc_size) {
        for(const Port &port: ports) {
            if(rtosc_match(port.name,m))
                d.port = &port, port.cb(m,d), d.obj = obj;
        }
    } else { //somewhat cheap

        //TODO this function is certainly buggy at the moment, some tests
        //are needed to make it clean
        //XXX buffer_size is not properly handled yet
        if(d.loc[0] == 0) {
            memset(d.loc, 0, d.loc_size);
            d.loc[0] = '/';
        }

        char *old_end = d.loc;
        while(*old_end) ++old_end;

        for(const Port &port: ports) {
            if(!rtosc_match(port.name, m))
                continue;

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

