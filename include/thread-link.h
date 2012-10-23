#include <tuple>
#include <vector>
#include <functional>
#include <initializer_list>

#include <jack/ringbuffer.h>

#include <assert.h>
#include <stdio.h>
#include <rtosc.h>

typedef const char *msg_t;

static bool match(const char *a, const char *b)
{
    if(!a || !b)
        return false;

    for(;*a && *a==*b;++a,++b);

    return *a==0 && *b==0;
}

size_t msg_len(const char *msg);
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

        void write(const char *dest, const char *args, ...)
        {
            va_list va;
            va_start(va,args);
            const size_t len = vsosc(write_buffer,MAX_MSG,dest,args,va);
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,write_buffer,len);
        }

        void raw_write(const char *msg)
        {
            const size_t len = msg_len(msg);
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,msg,len);
        }

        //Assumes no partial writes can occur
        bool hasNext(void) const {return jack_ringbuffer_read_space(ring);};
        msg_t read(void) {
            ring_t r[2];
            jack_ringbuffer_get_read_vector(ring,(jack_ringbuffer_data_t*)r);
            const size_t len = msg_len_ring(r);
            assert(jack_ringbuffer_read_space(ring) >= len);
            assert(len <= MAX_MSG);
            jack_ringbuffer_read(ring, read_buffer, len);
            return read_buffer;
        };
        char *buffer(void) {return write_buffer;}
        size_t buffer_size(void) {return MAX_MSG;}
    private:
        char write_buffer[MAX_MSG];
        char read_buffer[MAX_MSG];

        //assumes jack ringbuffer
        jack_ringbuffer_t *ring;
};



//An entrie consists of a pattern, a gating function, and a application function
//
//eg "echo", "*", [](msg_t)->bool{return x;}, [](msg_t msg){printf("%s\n", msg);}
//
//This can be said to mean, when a message that matches the pattern "echo" is
//found and x is true, then print the message

typedef std::tuple<const char *, const char *, std::function<bool(msg_t)>, std::function<void(msg_t)> > entry_t;

template<int _ne>
class Dispatch
{
    std::vector<entry_t> arr;

    public:
    Dispatch(std::initializer_list<entry_t> init)
        :arr{init}{}//edLink<10,10> b2u;

    //Get called back later when events are checked
    //b2u.request("/dev/dsp", [](float f){printf("%f\n", f);});
    //
    ////write a normal message via vsosc
    //b2u.write("/echo", "s", "hello world")
    //;

    void operator()(msg_t msg)
    {
        for(entry_t e:arr)
            if(match(msg,std::get<0>(e)) && match(std::get<1>(e),arg_str(msg)) && std::get<2>(e)(msg))
                std::get<3>(e)(msg);
    }
};

//static void function(void)
//{
//    ThreadLink<10,10> b2u;
//
//    //Get called back later when events are checked
//    b2u.write("/dev/dsp","");//, [](float f){printf("%f\n", f);});
//
//    //write a normal message via vsosc
//    b2u.write("/echo", "s", "hello world");
//    Dispatch<1> disp {
//        std::make_tuple("null", "", [](msg_t m)->bool{return false;}, [](msg_t)->void{1;})
//    };
//}
