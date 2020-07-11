#include <limits>
#include <cassert>
#include <cstring>
#include <algorithm>

#include "../util.h"
//#include <rtosc/arg-val-cmp.h>
#include <rtosc/pretty-format.h>
#include <rtosc/bundle-foreach.h>
#include <rtosc/ports.h>
#include <rtosc/ports-runtime.h>
#include <rtosc/default-value.h>
#include <rtosc/savefile.h>

namespace rtosc {

namespace {
    constexpr std::size_t buffersize = 8192;
    constexpr size_t max_arg_vals = 2048;
}

bool arg_eq(rtosc_arg_val_t a, rtosc_arg_val_t b)
{
    if(a.type != b.type)
        return false;
    if(a.type != 'i')
        return false;
    if(a.val.i == b.val.i)
        return true;
    return false;
}

std::string get_changed_values(const Ports& ports, void *runtime)
{
    std::string res;
    char port_buffer[buffersize];
    memset(port_buffer, 0, buffersize); // requirement for walk_ports

    auto on_reach_port =
            [](const Port* p, const char* port_buffer,
               const char* port_from_base, const Ports& base,
               void* data, void* runtime)
    {
        printf("Reached port <%s><%s>\n", port_buffer, port_from_base);
        assert(runtime);
        const Port::MetaContainer meta = p->meta();

        if((p->name[strlen(p->name)-1] != ':' && !strstr(p->name, "::"))
            || meta.find("parameter") == meta.end())
        {
            // runtime information can not be retrieved,
            // thus, it can not be compared with the default value
            return;
        } else { // TODO: duplicate to above? (colon[1])
            const char* colon = strchr(p->name, ':');
            if(!colon || !colon[1])
            {
                // runtime information can not be loaded, so don't save it
                // a possible FEATURE would be to save it anyways
                return;
            }
        }

        char loc[buffersize] = ""; // buffer to hold the dispatched path
        rtosc_arg_val_t arg_vals_default[max_arg_vals];
        std::vector<rtosc_arg_val_t> arg_vals_runtime;
        // buffer to hold the message (i.e. /port ..., without port's bases)
        char buffer_with_port[buffersize];
        char strbuf[buffersize]; // temporary string buffer for pretty-printing

        std::string* res = (std::string*)data;
        assert(strlen(port_buffer) + 1 < buffersize);
        // copy the path until before the message
        fast_strcpy(loc, port_buffer, std::min((ptrdiff_t)buffersize,
                                               port_from_base - port_buffer + 1
                                               ));
        char* loc_end = loc + (port_from_base - port_buffer);
        size_t loc_remain_size = buffersize - (port_from_base - port_buffer);
        *loc_end = 0;

        const char* portargs = strchr(p->name, ':');
        if(!portargs)
            portargs = p->name + strlen(p->name);

// TODO: p->name: duplicate to p
        int nargs_default = get_default_value(p->name,
                                              portargs,
                                              base,
                                              runtime,
                                              p,
                                              -1,
                                              max_arg_vals,
                                              arg_vals_default,
                                              strbuf,
                                              buffersize);
        printf("nargs_default = %d\n", nargs_default);

        if(nargs_default > 0)
        {
            size_t nargs_runtime = 0;

            auto ftor = [&](const Port* p, const char* ,
                            const char* old_end,
                            const Ports& ,void* ,void* runtime)
            {
                fast_strcpy(buffer_with_port, p->name, buffersize);

                // the caller of ftor (in some cases bundle_foreach) has
                // already filled old_end correctly, but we have to copy this
                // over to loc_end
                fast_strcpy(loc_end, old_end, loc_remain_size);

                auto values = helpers::runtime_values(runtime,
                        *p, loc);
                for(auto v: values)
                    arg_vals_runtime.push_back(v);
            };

            auto refix_old_end = [&base](const Port* _p, char* _old_end)
            { // TODO: remove base capture
                bundle_foreach(*_p, _p->name, _old_end, NULL,
                               base, NULL, NULL, bundle_foreach_do_nothing,
                               false, false);
            };

            if(strchr(p->name, '#'))
            {
                // idea:
                //                    p/a/b
                // bundle_foreach =>  p/a#0/b, p/a#1/b, ... p/a#n/b, p
                // bundle_foreach =>  p/a/b
                // => justification for const_cast

                // Skip the array element (type 'a') for now...
                ++nargs_runtime;

                // Start filling at arg_vals_runtime + 1
                char* old_end_noconst = const_cast<char*>(port_from_base);
                bundle_foreach(*p, p->name, old_end_noconst, port_buffer + 1,
                               base, data, runtime,
                               ftor, true);

                // glue the old end behind old_end_noconst again
                refix_old_end(p, old_end_noconst);

                // "Go back" to fill arg_vals_runtime + 0
                arg_vals_runtime[0].type = 'a';
                arg_vals_runtime[0].val.a.len = nargs_runtime-1;
                arg_vals_runtime[0].val.a.type = arg_vals_runtime[1].type;
            }
            else
                ftor(p, port_buffer, port_from_base, base, NULL, runtime);

            canonicalize_arg_vals(arg_vals_default, nargs_default,
                                  strchr(p->name, ':'), meta);

            auto write_msg = [&res, &meta, &port_buffer]
                                 (const rtosc_arg_val_t* arg_vals_default,
                                  std::vector<rtosc_arg_val_t> arg_vals_runtime,
                                  int nargs_default)
            {
                printf("Write message?\n");
                printf("t1 = %c\n", arg_vals_default[0].type);
                printf("t2 = %c\n", arg_vals_runtime[0].type);
                printf("v1 = %d\n", arg_vals_default[0].val.i);
                printf("v2 = %d\n", arg_vals_runtime[0].val.i);
                bool args_equal = true;
                if(arg_vals_runtime.size() != nargs_default) {
                    printf("HERE\n");
                    args_equal = false;
                }
                if(args_equal)
                    for(int i=0; i<nargs_default; ++i)
                        if(!arg_eq(arg_vals_runtime[i], arg_vals_default[i]))
                            args_equal = false;
                if(!args_equal)
                {
                    printf("ARGS DIFFERENT!!(%s)\n", port_buffer);
                    char cur_value_pretty[buffersize] = " ";

                    map_arg_vals(arg_vals_runtime.data(),
                                 arg_vals_runtime.size(), meta);

                    rtosc_print_arg_vals(arg_vals_runtime.data(),
                            arg_vals_runtime.size(),
                                         cur_value_pretty + 1, buffersize - 1,
                                         NULL, strlen(port_buffer) + 1);
                    *res += port_buffer;
                    *res += cur_value_pretty;
                    *res += "\n";
                    printf("<%s>\n", res->c_str());
                }
            }; // functor write_msg

            printf("Here type = %c\n", arg_vals_runtime[0].type);
            if(false) {
            } else
            {
                printf("\n");
                printf("Default path to write_msg\n");
                write_msg(arg_vals_default, 
                        arg_vals_runtime,
                        nargs_default);
            }
        }
    };

    walk_ports(&ports, port_buffer, buffersize, &res, on_reach_port, false,
               runtime);

    if(res.length()) // remove trailing newline
        res.resize(res.length()-1);
    return res;
}

bool savefile_dispatcher_t::do_dispatch(const char* msg)
{
    *loc = 0;
    RtData d;
    d.obj = runtime;
    d.loc = loc; // we're always dispatching at the base
    d.loc_size = 1024;
    if(msg[0] == '/')
        msg++;
    printf("dispatch on <%s>\n", msg);
    ports->dispatch(msg, d, true);
    return !!d.matches;
}

int savefile_dispatcher_t::default_response(size_t nargs,
                                            bool first_round,
                                            savefile_dispatcher_t::dependency_t
                                                dependency)
{
    // default implementation:
    // no dependencies  => round 0,
    // has dependencies => round 1,
    // not specified    => both rounds
    return (dependency == not_specified
            || !(dependency ^ first_round))
           ? nargs // argument number is not changed
           : (int)discard;
}

int savefile_dispatcher_t::on_dispatch(size_t, char *,
                                       size_t, size_t nargs,
                                       rtosc_arg_val_t *,
                                       bool round2,
                                       dependency_t dependency)
{
    return default_response(nargs, round2, dependency);
}

int dispatch_printed_messages(const char* messages,
                              const Ports& ports, void* runtime,
                              savefile_dispatcher_t* dispatcher)
{
    constexpr std::size_t buffersize = 8192;
    char portname[buffersize], message[buffersize], strbuf[buffersize];
    int rd, rd_total = 0;
    int nargs;
    int msgs_read = 0;
    bool ok = true;

    savefile_dispatcher_t dummy_dispatcher;
    if(!dispatcher)
        dispatcher = &dummy_dispatcher;
    dispatcher->ports = &ports;
    dispatcher->runtime = runtime;

    // scan all messages twice:
    //  * in the second round, only dispatch those with ports that depend on
    //    other ports
    //  * in the first round, only dispatch all others
    for(int round = 0; round < 2 && ok; ++round)
    {
        msgs_read = 0;
        rd_total = 0;
        const char* msg_ptr = messages;
        while(*msg_ptr && ok)
        {
            nargs = rtosc_count_printed_arg_vals_of_msg(msg_ptr);
            if(nargs >= 0)
            {
                // nargs << 1 is usually too much, but it allows the user to use
                // these values (using on_dispatch())
                size_t maxargs = std::max(nargs << 1, 16);
                STACKALLOC(rtosc_arg_val_t, arg_vals, maxargs);
                rd = rtosc_scan_message(msg_ptr, portname, buffersize,
                                        arg_vals, nargs, strbuf, buffersize);
                rd_total += rd;
                printf("scanned <%s>:%d\n", portname, nargs);

                const Port* port = ports.apropos(portname);
                savefile_dispatcher_t::dependency_t dependency =
                    (savefile_dispatcher_t::dependency_t)
                    (port
                    ? !!port->meta()["default depends"]
                    : (int)savefile_dispatcher_t::not_specified);

                // let the user modify the message and the args
                // the argument number may have changed, or the user
                // wants to discard the message or abort the savefile loading
                nargs = dispatcher->on_dispatch(buffersize, portname,
                                                maxargs, nargs, arg_vals,
                                                round, dependency);
                printf("nargs after ondispatch = %d\n", nargs);

                if(nargs == savefile_dispatcher_t::abort)
                    ok = false;
                else
                {
                    if(nargs != savefile_dispatcher_t::discard)
                    {
                        const rtosc_arg_val_t* arg_val_ptr;
                        bool is_array = false;

                        char* portname_end = portname + strlen(portname);
                        char args[32]  = {0};
                        char msg[1024] = {0};
                        rtosc_arg_t arg[32];
                        for(int i=0; i<nargs; ++i) {
                            args[i] = arg_vals[i].type;
                            arg[i]  = arg_vals[i].val;
                        }

                        size_t sz = rtosc_amessage(msg, sizeof(msg),
                                portname, args,
                                arg);
                        printf("msg = <%s>\n", msg);
                        if(sz)
                            ok = (*dispatcher)(msg);
                        printf("ok -> %d\n", ok);
                    }
                }

                msg_ptr += rd;
                ++msgs_read;
            }
            else if(nargs == std::numeric_limits<int>::min())
            {
                // this means the (rest of the) file is whitespace only
                // => don't increase msgs_read
                while(*++msg_ptr) ;
            }
            else {
                ok = false;
            }
        }
    }
    printf("msg_read = <%d>\n", msgs_read);
    printf("ok = %d\n", ok);
    return ok ? msgs_read : -rd_total-1;
}

std::string save_to_file(const Ports &ports, void *runtime,
                         const char *appname)
{
    return get_changed_values(ports, runtime);
}

int load_from_file(const char* file_content,
                   const Ports& ports, void* runtime,
                   const char* appname,
                   savefile_dispatcher_t* dispatcher)
{
    int bytes_read = 0;
    int rval = dispatch_printed_messages(file_content,
                                         ports, runtime, dispatcher);
    return (rval < 0) ? (rval-bytes_read) : rval;
}

}

