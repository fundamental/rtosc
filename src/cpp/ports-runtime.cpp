#include "util.h"
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <cassert>

#include <rtosc/pretty-format.h>
#include <rtosc/ports.h>
#include <rtosc/ports-runtime.h>

namespace rtosc {
namespace helpers {

//! RtData subclass to capture argument values pretty-printed from
//! a runtime object
class CapturePretty : public RtData
{
    char* buffer;
    std::size_t buffersize;
    int cols_used;

    void reply(const char *) override { assert(false); }

/*    void replyArray(const char*, const char *args,
                    rtosc_arg_t *vals)
    {
        size_t cur_idx = 0;
        for(const char* ptr = args; *ptr; ++ptr, ++cur_idx)
        {
            assert(cur_idx < max_args);
            arg_vals[cur_idx].type = *ptr;
            arg_vals[cur_idx].val  = vals[cur_idx];
        }

        // TODO: refactor code, also with Capture ?
        size_t wrt = rtosc_print_arg_vals(arg_vals, cur_idx,
                                          buffer, buffersize, NULL,
                                          cols_used);
        assert(wrt);
    }*/

    void reply_va(const char *args, va_list va)
    {
        size_t nargs = strlen(args);
        STACKALLOC(rtosc_arg_val_t, arg_vals, nargs);

        rtosc_v2argvals(arg_vals, nargs, args, va);

        size_t wrt = rtosc_print_arg_vals(arg_vals, nargs,
                                          buffer, buffersize, NULL,
                                          cols_used);
        assert(wrt);
        (void)wrt;
    }

    void broadcast(const char *, const char *args, ...) override
    {
        va_list va;
        va_start(va,args);
        reply_va(args, va);
        va_end(va);
    }

    void reply(const char *, const char *args, ...) override
    {
        va_list va;
        va_start(va,args);
        reply_va(args, va);
        va_end(va);
    }

public:
    //! Return the argument values, pretty-printed
    const char* value() const { return buffer; }
    CapturePretty(char* buffer, std::size_t size, int cols_used) :
        buffer(buffer), buffersize(size), cols_used(cols_used) {}
};

const char* get_value_from_runtime(void* runtime, const Ports& ports,
                                   size_t loc_size, char* loc,
                                   char* buffer_with_port,
                                   std::size_t buffersize,
                                   int cols_used)
{
    std::size_t addr_len = strlen(buffer_with_port);

    // use the port buffer to print the result, but do not overwrite the
    // port name
    CapturePretty d(buffer_with_port + addr_len, buffersize - addr_len,
                    cols_used);
    d.obj = runtime;
    d.loc_size = loc_size;
    d.loc = loc;
    d.matches = 0;

    // does the message at least fit the arguments?
    assert(buffersize - addr_len >= 8);
    // append type
    memset(buffer_with_port + addr_len, 0, 8); // cover string end and arguments
    buffer_with_port[addr_len + (4-addr_len%4)] = ',';

    d.message = buffer_with_port;

    // buffer_with_port is a message in this call:
    ports.dispatch(buffer_with_port, d, false);

    return d.value();
}

//! RtData subclass to capture argument values from a runtime object
class Capture : public RtData
{
    size_t max_args;
    rtosc_arg_val_t* arg_vals;
    int nargs;
    std::vector<std::vector<char>>* scratch_bufs;

    void chain(const char *path, const char *args, ...) override
    {
        (void)path;
        (void)args;
        nargs = 0;
    }

    void chain(const char *msg) override
    {
        (void)msg;
        nargs = 0;
    }

    // save blobs to *this, so the pointers still point to valid memory
    // if the caller deletes it at the end of the capture
    void map_blobs()
    {
        for(int i = 0; i < nargs; ++i)
        {
            if(arg_vals[i].type == 'b'
               && arg_vals[i].val.b.data && arg_vals[i].val.b.len)
            {
                scratch_bufs->resize(scratch_bufs->size()+1);
                scratch_bufs->back().resize(arg_vals[i].val.b.len);
                std::copy_n(arg_vals[i].val.b.data,
                            arg_vals[i].val.b.len,
                            scratch_bufs->back().begin());
                arg_vals[i].val.b.data = (uint8_t*)scratch_bufs->back().data();
            }
        }
    }

    void reply(const char *) override { assert(false); }

    void replyArray(const char*, const char *args,
                    rtosc_arg_t *vals) override
    {
        size_t cur_idx = 0;
        for(const char* ptr = args; *ptr; ++ptr, ++cur_idx)
        {
            assert(cur_idx < max_args);
            arg_vals[cur_idx].type = *ptr;
            arg_vals[cur_idx].val  = vals[cur_idx];
        }
        nargs = cur_idx;
        map_blobs();
    }

    void reply_va(const char *args, va_list va)
    {
        nargs = strlen(args);
        assert((size_t)nargs <= max_args);

        rtosc_v2argvals(arg_vals, nargs, args, va);
        map_blobs();
    }

    void broadcast(const char *, const char *args, ...) override
    {
        va_list va;
        va_start(va, args);
        reply_va(args, va);
        va_end(va);
    }

    void reply(const char *, const char *args, ...) override
    {
        va_list va;
        va_start(va,args);
        reply_va(args, va);
        va_end(va);
    }
public:
    //! Return the number of argument values stored
    int size() const { return nargs; }
    Capture(std::size_t max_args, rtosc_arg_val_t* arg_vals, std::vector<std::vector<char>>* scratch_bufs) :
        max_args(max_args), arg_vals(arg_vals), nargs(-1),
        scratch_bufs(scratch_bufs) {}
    //! Silence compiler warnings
    std::size_t dont_use_this_function() { return max_args; }
};

size_t get_value_from_runtime(void* runtime, const Port& port,
                              size_t loc_size, char* loc,
                              const char* portname_from_base,
                              std::size_t buffersize,
                              std::size_t max_args, rtosc_arg_val_t* arg_vals,
                              std::vector<std::vector<char> >* scratch_bufs)
{
    assert(portname_from_base);

    STACKALLOC(char, buffer_with_port, buffersize);
    fast_strcpy(buffer_with_port, loc, buffersize);
    std::size_t addr_len = strlen(buffer_with_port);

    Capture d(max_args, arg_vals, scratch_bufs);
    d.obj = runtime;
    d.loc_size = loc_size;
    d.loc = loc;
    d.port = &port;
    d.matches = 0;
    assert(*loc == '/');

    // does the message at least fit the arguments?
    assert(buffersize - addr_len >= 8);
    // append type
    memset(buffer_with_port + addr_len, 0, 8); // cover string end and arguments
    buffer_with_port[addr_len + (4-addr_len%4)] = ',';

    // TODO? code duplication

    // buffer_with_port is a message in this call:
    const char* start_of_port_in_msg = buffer_with_port + strlen(buffer_with_port) - strlen(portname_from_base);
    assert(start_of_port_in_msg);
    assert(!strcmp(start_of_port_in_msg, portname_from_base));
    d.message = buffer_with_port;
    port.cb(start_of_port_in_msg, d);

    assert(d.size() >= 0);
    return d.size();
}

} // namespace helpers
} // namespace rtosc

