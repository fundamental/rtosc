#ifndef THREAD_LINK
#define THREAD_LINK

#include <array>
#include <functional>
#include <initializer_list>
#include <iostream>

#include <jack/ringbuffer.h>

#include <cstring>
#include <assert.h>
#include <stdio.h>
#include <rtosc.h>

typedef const char *msg_t;

/**
 * ThreadLink - A simple wrapper around jack's ringbuffers desinged to make
 * sending messages via rt-osc trivial.
 * This class provides the basics of reading and writing events via fixed sized
 * buffers, which can be specified at compile time.
 */
template<int MAX_MSG, int MAX_MSGS>
class ThreadLink
{
    public:
        ThreadLink(void)
        {
            ring = jack_ringbuffer_create(MAX_MSG*MAX_MSGS);
            jack_ringbuffer_mlock(ring);
        }

        ~ThreadLink(void)
        {
            jack_ringbuffer_free(ring);
        }

        /**
         * Write message to ringbuffer
         */
        void write(const char *dest, const char *args, ...)
        {
            va_list va;
            va_start(va,args);
            const size_t len = vsosc(write_buffer,MAX_MSG,dest,args,va);
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,write_buffer,len);
        }

        /**
         * Directly write message to ringbuffer
         */
        void raw_write(const char *msg)
        {
            const size_t len = msg_len(msg);
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,msg,len);
        }

        /**
         * @returns true iff there is another message to be read in the buffer
         */
        bool hasNext(void) const
        {
            return jack_ringbuffer_read_space(ring);
        }

        /**
         * Read a new message from the ringbuffer
         */
        msg_t read(void) {
            ring_t r[2];
            jack_ringbuffer_get_read_vector(ring,(jack_ringbuffer_data_t*)r);
            const size_t len = msg_len_ring(r);
            assert(jack_ringbuffer_read_space(ring) >= len);
            assert(len <= MAX_MSG);
            jack_ringbuffer_read(ring, read_buffer, len);
            return read_buffer;
        }

        /**
         * Peak at last message read without reading another
         */
        msg_t peak(void) const
        {
            return read_buffer;
        }

        /**
         * Raw write buffer access for more complicated task
         */
        char *buffer(void) {return write_buffer;}
        /**
         * Access to write buffer length
         */
        constexpr size_t buffer_size(void) {return MAX_MSG;}
    private:
        char write_buffer[MAX_MSG];
        char read_buffer[MAX_MSG];

        //assumes jack ringbuffer
        jack_ringbuffer_t *ring;
};



class _Ports;

class _Port
{
    public:
        _Port(const char*_n, int _t, const char *_m)
            :name(_n),traits(_t),midi(_m){}
        _Port(const char*_n, int _t, _Ports *_p)
            :name(_n),traits(_t),ports(_p){}
        const char *name;
        int traits;
        union {
            const char *midi;
            _Ports *ports;
        };
};

template<class T>
class Port:public _Port {
    public:
        Port(const char*_n, int _t, const char* _m, std::function<void(msg_t, T*)> _f)
            :_Port(_n,_t,_m), cb(_f)
        {};
        Port(const char*_n, int _t, _Ports *_p, std::function<void(msg_t, T*)> _f)
            :_Port(_n,_t,_p), cb(_f)
        {};
        Port(const Port<T> &port)
            :_Port(port.name,port.traits,port.ports),cb(port.cb){}
        std::function<void(msg_t, T*)> cb;
};

class _Ports
{
    public:
        virtual const _Port &port(unsigned i) const=0;
        virtual unsigned     nports(void)     const=0;

        //Lookup a port without respect to arguments
        //This only applies to leaf nodes
        virtual const _Port *operator[](const char *) const=0;
        virtual const char *meta_data(const char *path) const=0;
};

template<int len, class T>
class Ports : public _Ports
{
    public:
        std::array<Port<T>,len> ports;


        const _Port &port(unsigned i) const
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
                const char *p = match(port.name,m);
                if(*p == '/' || (*p == ':' && arg_match(p+1,arg_str(m))))
                    port.cb(m,t);
            }
        }

        const _Port *operator[](const char *name) const
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

        const char *meta_data(const char *path) const
        {
            for(const Port<T> &port: ports) {
                const char *p = match(port.name,path);
                if(*p == '/') {
                    ++path;
                    while(*++path!='/');
                    return port.ports->meta_data(path+1);
                }
                if(*p == ':') {
                    return port.midi;
                }
            }
            puts("failed to get meta-data...");
            return "";
        }

        /**
         * Match partial path
         */
        inline const char *match(const char *pattern, const char *path) const
        {
            for(;*pattern&&*path&&*path!=':'&&*path!='/'&&*path==*pattern;++path,++pattern);
            return pattern;
        }

        /**
         * Match argument string
         */
        inline bool arg_match(const char *pattern, const char *path) const
        {
            const char *_path = path;
            bool match = true;
            while(*pattern) {
                match &= (*pattern++==*path++);
                if(*pattern==':') {
                    if(match)
                        return true;
                    else
                        path = _path, ++pattern, match=true;
                }
            }
            return match;
        }
};

template<int len>
struct MidiAddr
{
    MidiAddr(void):ch(255),ctl(255){}
    unsigned char ch, ctl;
    char path[len], conversion[len];
};

/**
 * Table of midi mappings
 */
template<int len, int elms>
struct MidiTable
{
    std::array<MidiAddr<len>,elms> table;
    bool has(unsigned char ch, unsigned char ctl) const
    {
        for(auto e:table) {
            if(e.ch == ch && e.ctl == ctl)
                return true;
        }
        return false;
    }
    const MidiAddr<len> *get(unsigned char ch, unsigned char ctl) const
    {
        for(auto &e:table)
            if(e.ch==ch && e.ctl == ctl)
                return &e;
        return NULL;
    }
    void addElm(unsigned char ch, unsigned char ctl, const char *path,
            const char *mapping)
    {
        for(MidiAddr<len> &e:table) {
            if(e.path[0]=='\0'&& e.conversion[0]=='\0') {//free spot
                e.ch  = ch;
                e.ctl = ctl;
                strncpy(e.path,path,len);
                strncpy(e.conversion,mapping,len);
                return;
            }
        }
    }
};

struct OSC_Message
{
    OSC_Message(const char *msg)
        :message(msg){}

    const char *message;
};


std::ostream &operator<<(std::ostream &o, const OSC_Message &msg)
{
    const char *args = arg_str(msg.message);
    o << '<' << msg.message << ':' << args << ':';

    int _args = nargs(msg.message);

    for(int i=0; i<_args; ++i) {
        arg_t arg = argument(msg.message,i);
        switch(type(msg.message,i))
        {
            case 'T':
            case 'F':
                o << arg.T;
                break;
            case 'i':
                o << arg.i;
                break;
            case 'f':
                o << arg.f;
                break;
            case 's':
                o << arg.s;
                break;
            case 'b':
                o << "{blob:" << arg.b.len << "}";
                break;
        }
        if(i!=_args-1)
            o << ",";
    }
    o << '>';
    return o;
}

#endif
