#include "../util.h"
#include <cstdarg>
#include <cstring>
#include <cassert>

#include <rtosc/ports.h>
#include <rtosc/ports-runtime.h>

namespace rtosc {
namespace helpers {

//! RtData subclass to capture argument values from a runtime object
class Capturev2 : public RtData
{
    void chain(const char *path, const char *args, ...) override
    { assert(false); }

    void chain(const char *msg) override
    { assert(false); }

    void reply(const char *) override { assert(false); }

    void replyArray(const char *path, const char *args,
                    rtosc_arg_t *vals) override
    {
        rtosc_amessage(buffer, sizeof(buffer), path, args, vals);
    }

    void broadcast(const char *path, const char *args, ...) override
    {
        va_list va;
        va_start(va, args);
        rtosc_vmessage(buffer, sizeof(buffer), path, args, va);
        va_end(va);
    }

    void reply(const char *path, const char *args, ...) override
    {
        va_list va;
        va_start(va,args);
        rtosc_vmessage(buffer, sizeof(buffer), path, args, va);
        va_end(va);
    }

public:
    Capturev2(void) {}
    char buffer[1024] = {0};

};

std::vector<rtosc_arg_val_t>
runtime_values(void *runtime,
                    const Port &port,
                    const char *name)
{
    std::vector<rtosc_arg_val_t> vls;
    printf("runtime values...\n");
    char loc[256];
    strncpy(loc, name, sizeof(loc));
    Capturev2 d;
    d.obj      = runtime;
    d.loc_size = sizeof(loc);
    d.loc      = loc;
    d.port     = &port;
    d.matches  = 0;
    assert(*loc);

    char message[1024];
    size_t len = rtosc_message(message, sizeof(message),
            name, "");

    if(!len)
        return vls;

    port.cb(message, d);

    if(!rtosc_message_length(d.buffer, sizeof(d.buffer)))
        return vls;

    printf("<%s:%s>\n", d.buffer,
            rtosc_message_length(d.buffer, sizeof(d.buffer)) ? rtosc_argument_string(d.buffer):"nil");

    int N = rtosc_narguments(d.buffer);
    for(int i=0; i<N; ++i) {
        rtosc_arg_val_t val;
        val.val  = rtosc_argument(d.buffer, i);
        val.type = rtosc_type(d.buffer, i);
        vls.push_back(val);
    }
    return vls;
}

rtosc_arg_val_t
simple_runtime_value(void *runtime,
                    const Port &port,
                    const char *name)
{
    rtosc_arg_val_t vls;
    memset(&vls, 0, sizeof(vls));
    printf("runtime values...\n");
    char loc[256];
    strncpy(loc, name, sizeof(loc));
    Capturev2 d;
    d.obj      = runtime;
    d.loc_size = sizeof(loc);
    d.loc      = loc;
    d.port     = &port;
    d.matches  = 0;
    assert(*loc);

    char message[1024];
    size_t len = rtosc_message(message, sizeof(message),
            name, "");

    if(!len)
        return vls;

    port.cb(message, d);

    if(!rtosc_message_length(d.buffer, sizeof(d.buffer)))
        return vls;

    printf("<%s:%s>\n", d.buffer,
            rtosc_message_length(d.buffer, sizeof(d.buffer)) ? rtosc_argument_string(d.buffer):"nil");

    int N = rtosc_narguments(d.buffer);
    if(N == 1) {
        vls.type = rtosc_type(d.buffer, 0);
        vls.val  = rtosc_argument(d.buffer, 0);
    }
    return vls;
}

} // namespace helpers
} // namespace rtosc

