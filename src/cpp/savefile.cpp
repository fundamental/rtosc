#include <limits>
#include <cassert>
#include <cstring>

#include "../util.h"
#include <rtosc/arg-val-cmp.h>
#include <rtosc/pretty-format.h>
#include <rtosc/bundle-foreach.h>
#include <rtosc/ports.h>
#include <rtosc/ports-runtime.h>
#include <rtosc/default-value.h>
#include <rtosc/savefile.h>

namespace rtosc {

std::string get_changed_values(const Ports& ports, void* runtime)
{
    std::string res;
    constexpr std::size_t buffersize = 8192;
    char port_buffer[buffersize];
    memset(port_buffer, 0, buffersize); // requirement for walk_ports

    const size_t max_arg_vals = 2048;

    auto on_reach_port =
            [](const Port* p, const char* port_buffer,
               const char* port_from_base, const Ports& base,
               void* data, void* runtime)
    {
        assert(runtime);
        const Port::MetaContainer meta = p->meta();
#if 0
// practical for debugging if a parameter was changed, but not saved
        const char* cmp = "/part15/kit0/adpars/GlobalPar/Reson/Prespoints";
        if(!strncmp(port_buffer, cmp, strlen(cmp)))
        {
            puts("break here");
        }
#endif

        if((p->name[strlen(p->name)-1] != ':' && !strstr(p->name, "::"))
            || meta.find("parameter") == meta.end())
        {
            // runtime information can not be retrieved,
            // thus, it can not be compared with the default value
            return;
        }
        else
        { // TODO: duplicate to above? (colon[1])
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
        rtosc_arg_val_t arg_vals_runtime[max_arg_vals];
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

#if 0 // debugging stuff
        if(!strncmp(port_buffer, "/part1/Penabled", 5) &&
           !strncmp(port_buffer+6, "/Penabled", 9))
        {
            printf("runtime: %ld\n", (long int)runtime);
        }
#endif
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

                size_t nargs_runtime_cur =
                    helpers::get_value_from_runtime(runtime, *p,
                                                    buffersize, loc, old_end,
                                                    buffer_with_port,
                                                    buffersize,
                                                    max_arg_vals,
                                                    arg_vals_runtime +
                                                        nargs_runtime);
                nargs_runtime += nargs_runtime_cur;
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

#if 0
// practical for debugging if a parameter was changed, but not saved
            const char* cmp = "/part15/kit0/adpars/GlobalPar/Reson/Prespoints";
            if(!strncmp(port_buffer, cmp, strlen(cmp)))
            {
                puts("break here");
            }
#endif
            canonicalize_arg_vals(arg_vals_default, nargs_default,
                                  strchr(p->name, ':'), meta);

            auto write_msg = [&res, &meta, &port_buffer]
                                 (const rtosc_arg_val_t* arg_vals_default,
                                  rtosc_arg_val_t* arg_vals_runtime,
                                  int nargs_default, size_t nargs_runtime)
            {
                if(!rtosc_arg_vals_eq(arg_vals_default, arg_vals_runtime,
                                      nargs_default, nargs_runtime, nullptr))
                {
                    char cur_value_pretty[buffersize] = " ";

                    map_arg_vals(arg_vals_runtime, nargs_runtime, meta);

                    rtosc_print_arg_vals(arg_vals_runtime, nargs_runtime,
                                         cur_value_pretty + 1, buffersize - 1,
                                         NULL, strlen(port_buffer) + 1);
                    *res += port_buffer;
                    *res += cur_value_pretty;
                    *res += "\n";
                }
            }; // functor write_msg

            if(arg_vals_runtime[0].type == 'a' && strchr(port_from_base, '/'))
            {
                // These are grouped as an array, but the port structure
                // implicits that they shall be handled as single values
                // inside their subtrees
                //  => We don't print this as an array
                //  => All arrays in savefiles have their numbers after
                //     the last port separator ('/')

                // used if the value of lhs or rhs is range-computed:
                rtosc_arg_val_t rlhs, rrhs;

                rtosc_arg_val_itr litr, ritr;
                rtosc_arg_val_itr_init(&litr, arg_vals_default+1);
                rtosc_arg_val_itr_init(&ritr, arg_vals_runtime+1);

                auto write_msg_adaptor = [&litr, &ritr,&rlhs,&rrhs,&write_msg](
                    const Port* p,
                    const char* port_buffer, const char* old_end,
                    const Ports&, void*, void*)
                {
                    const rtosc_arg_val_t
                        * lcur = rtosc_arg_val_itr_get(&litr, &rlhs),
                        * rcur = rtosc_arg_val_itr_get(&ritr, &rrhs);

                    if(!rtosc_arg_vals_eq_single(
                            rtosc_arg_val_itr_get(&litr, &rlhs),
                            rtosc_arg_val_itr_get(&ritr, &rrhs), nullptr))
                    {
                        auto get_sz = [](const rtosc_arg_val_t* a) {
                            return a->type == 'a' ? (a->val.a.len + 1) : 1; };
                        // the const-ness does not matter
                        write_msg(lcur,
                            const_cast<rtosc_arg_val_t*>(rcur),
                            get_sz(lcur), get_sz(rcur));
                    }

                    rtosc_arg_val_itr_next(&litr);
                    rtosc_arg_val_itr_next(&ritr);
                };

                char* old_end_noconst = const_cast<char*>(port_from_base);

                // iterate over the whole array
                bundle_foreach(*p, p->name, old_end_noconst, port_buffer,
                               base, NULL, NULL,
                               write_msg_adaptor, true);

                // glue the old end behind old_end_noconst again
                refix_old_end(p, old_end_noconst);

            }
            else
            {
                write_msg(arg_vals_default, arg_vals_runtime,
                          nargs_default, nargs_runtime);
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
                rtosc_arg_val_t arg_vals[maxargs];
                rd = rtosc_scan_message(msg_ptr, portname, buffersize,
                                        arg_vals, nargs, strbuf, buffersize);
                rd_total += rd;

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

                if(nargs == savefile_dispatcher_t::abort)
                    ok = false;
                else
                {
                    if(nargs != savefile_dispatcher_t::discard)
                    {
                        const rtosc_arg_val_t* arg_val_ptr;
                        bool is_array;
                        if(nargs && arg_vals[0].type == 'a')
                        {
                            is_array = true;
                            // arrays of arrays are not yet supported -
                            // neither by rtosc_*message, nor by the inner for
                            // loop below.
                            // arrays will probably have an 'a' (or #)
                            assert(arg_vals[0].val.a.type != 'a' &&
                                   arg_vals[0].val.a.type != '#');
                            // we won't read the array arg val anymore
                            --nargs;
                            arg_val_ptr = arg_vals + 1;
                        }
                        else {
                            is_array = false;
                            arg_val_ptr = arg_vals;
                        }

                        char* portname_end = portname + strlen(portname);

                        rtosc_arg_val_itr itr;
                        rtosc_arg_val_t buffer;
                        const rtosc_arg_val_t* cur;

                        rtosc_arg_val_itr_init(&itr, arg_val_ptr);

                        // for bundles, send each element separately
                        // for non-bundles, send all elements at once
                        for(size_t arr_idx = 0;
                            itr.i < (size_t)std::max(nargs,1) && ok; ++arr_idx)
                        {
                            // this will fail for arrays of arrays,
                            // since it only copies one arg val
                            // (arrays are not yet specified)
                            size_t i;
                            const size_t last_pos = itr.i;
                            const size_t elem_limit = is_array
                                  ? 1 : std::numeric_limits<int>::max();

                            // equivalent to the for loop below, in order to
                            // find out the array size
                            size_t val_max = 0;
                            {
                                rtosc_arg_val_itr itr2 = itr;
                                for(val_max = 0;
                                    itr2.i - last_pos < (size_t)nargs &&
                                        val_max < elem_limit;
                                    ++val_max)
                                {
                                    rtosc_arg_val_itr_next(&itr2);
                                }
                            }
                            rtosc_arg_t vals[val_max];
                            char argstr[val_max+1];

                            for(i = 0;
                                itr.i - last_pos < (size_t)nargs &&
                                    i < elem_limit;
                                ++i)
                            {
                                cur = rtosc_arg_val_itr_get(&itr, &buffer);
                                vals[i] = cur->val;
                                argstr[i] = cur->type;
                                rtosc_arg_val_itr_next(&itr);
                            }

                            argstr[i] = 0;

                            if(is_array)
                                snprintf(portname_end, 8, "%d", (int)arr_idx);

                            rtosc_amessage(message, buffersize, portname,
                                           argstr, vals);

                            ok = (*dispatcher)(message);
                        }
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
    return ok ? msgs_read : -rd_total-1;
}

std::string save_to_file(const Ports &ports, void *runtime,
                         const char *appname, rtosc_version appver)
{
    std::string res;
    char rtosc_vbuf[12], app_vbuf[12];

    {
        rtosc_version rtoscver = rtosc_current_version();
        rtosc_version_print_to_12byte_str(&rtoscver, rtosc_vbuf);
        rtosc_version_print_to_12byte_str(&appver, app_vbuf);
    }

    res += "% RT OSC v"; res += rtosc_vbuf; res += " savefile\n"
           "% "; res += appname; res += " v"; res += app_vbuf; res += "\n";
    res += get_changed_values(ports, runtime);

    return res;
}

int load_from_file(const char* file_content,
                   const Ports& ports, void* runtime,
                   const char* appname,
                   rtosc_version appver,
                   savefile_dispatcher_t* dispatcher)
{
    char appbuf[128];
    int bytes_read = 0;

    if(dispatcher)
    {
        dispatcher->app_curver = appver;
        dispatcher->rtosc_curver = rtosc_current_version();
    }

    unsigned vma, vmi, vre;
    int n = 0;

    sscanf(file_content,
           "%% RT OSC v%u.%u.%u savefile%n ", &vma, &vmi, &vre, &n);
    if(n <= 0 || vma > 255 || vmi > 255 || vre > 255)
        return -bytes_read-1;
    if(dispatcher)
    {
        dispatcher->rtosc_filever.major = vma;
        dispatcher->rtosc_filever.minor = vmi;
        dispatcher->rtosc_filever.revision = vre;
    }
    file_content += n;
    bytes_read += n;
    n = 0;

    sscanf(file_content,
           "%% %128s v%u.%u.%u%n ", appbuf, &vma, &vmi, &vre, &n);
    if(n <= 0 || strcmp(appbuf, appname) || vma > 255 || vmi > 255 || vre > 255)
        return -bytes_read-1;

    if(dispatcher)
    {
        dispatcher->app_filever.major = vma;
        dispatcher->app_filever.minor = vmi;
        dispatcher->app_filever.revision = vre;
    }
    file_content += n;
    bytes_read += n;
    n = 0;

    int rval = dispatch_printed_messages(file_content,
                                         ports, runtime, dispatcher);
    return (rval < 0) ? (rval-bytes_read) : rval;
}

}

