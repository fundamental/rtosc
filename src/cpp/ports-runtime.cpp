#include "../util.h"
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
        rtosc_arg_val_t arg_vals[nargs];

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

    void chain(const char *path, const char *args, ...) override
    {
        nargs = 0;
    }

    void chain(const char *msg) override
    {
        nargs = 0;
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
    }

    void reply_va(const char *args, va_list va)
    {
        nargs = strlen(args);
        assert((size_t)nargs <= max_args);

        rtosc_v2argvals(arg_vals, nargs, args, va);
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
    Capture(std::size_t max_args, rtosc_arg_val_t* arg_vals) :
        max_args(max_args), arg_vals(arg_vals), nargs(-1) {}
    //! Silence compiler warnings
    std::size_t dont_use_this_function() { return max_args; }
};

size_t get_value_from_runtime(void* runtime, const Port& port,
                              size_t loc_size, char* loc,
                              const char* portname_from_base,
                              char* buffer_with_port, std::size_t buffersize,
                              std::size_t max_args, rtosc_arg_val_t* arg_vals)
{
    fast_strcpy(buffer_with_port, portname_from_base, buffersize);
    std::size_t addr_len = strlen(buffer_with_port);

    Capture d(max_args, arg_vals);
    d.obj = runtime;
    d.loc_size = loc_size;
    d.loc = loc;
    d.port = &port;
    d.matches = 0;
    assert(*loc);

    // does the message at least fit the arguments?
    assert(buffersize - addr_len >= 8);
    // append type
    memset(buffer_with_port + addr_len, 0, 8); // cover string end and arguments
    buffer_with_port[addr_len + (4-addr_len%4)] = ',';

    // TODO? code duplication

    // buffer_with_port is a message in this call:
    d.message = buffer_with_port;
    port.cb(buffer_with_port, d);

    assert(d.size() >= 0);
    return d.size();
}

} // namespace helpers
} // namespace rtosc

