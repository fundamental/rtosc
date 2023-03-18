#include <limits>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <map>
#include <set>
#include <queue>

#include "util.h"
#include <rtosc/arg-ext.h>
#include <rtosc/arg-val-cmp.h>
#include <rtosc/arg-val-itr.h>
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

// mostly a copy of rtosc_arg_vals_eq, but this tells us the first equal index
// in the final range of equal elements
size_t first_equal_index(const rtosc_arg_val_t* lhs, rtosc_arg_val_t* rhs,
                         size_t lsize, size_t rsize)
{
    if(lhs[0].type == 'a' && rhs[0].type == 'a')
    {
        assert((size_t)(rtosc_arg_arr_len(&lhs[0].val)) == lsize - 1);

        size_t first_equal = 0, count = 0;

        ++lhs;
        ++rhs;
        --lsize;
        --rsize;

        // used if the value of lhs or rhs is range-computed:
        rtosc_arg_val_t rlhs, rrhs;

        rtosc_arg_val_itr litr, ritr;
        rtosc_arg_val_itr_init(&litr, lhs);
        rtosc_arg_val_itr_init(&ritr, rhs);

        for( ; rtosc_arg_vals_cmp_has_next(&litr, &ritr, lsize, rsize);
            rtosc_arg_val_itr_next(&litr),
            rtosc_arg_val_itr_next(&ritr),
            ++count)
        {
            const int equal = rtosc_arg_vals_eq_single(
                                rtosc_arg_val_itr_get(&litr, &rlhs),
                                rtosc_arg_val_itr_get(&ritr, &rrhs),
                                get_default_cmp_options());
            if(!equal)
            {
                first_equal = ritr.i + 1;
            }
        }
        rtosc_arg_arr_len_set(&rhs[-1].val, first_equal);
        assert(1 + first_equal <= rsize + 1); // TODO: <= rsize?
        return 1 + first_equal;
    }
    else
        return rsize;
}

// this basically does:
// walk over all ports and for each port:
// * get default value
// * get current value
// * compare: if values are different -> write to savefile
std::string get_changed_values(const Ports& ports, void* runtime,
                               std::set<std::string>& alreadyWritten,
                               const std::vector<std::string> &propsToExclude)
{
    char port_buffer[buffersize];
    memset(port_buffer, 0, buffersize); // requirement for walk_ports

    struct data_t
    {
        std::string res;
        std::set<std::string> written;
        const std::vector<std::string>* propsToExclude;
    } data;
    std::swap(data.written, alreadyWritten);
    data.propsToExclude = &propsToExclude;

    /* example:
        port_buffer:     /insefx5/EQ/filter3/Pstages
        port_from_base:  Pstages
        p->name          Pstages::i
     */
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

        if(meta.find("alias") != meta.end())
        {
            // this param is already saved in another port
            return;
        }

        for(const std::string& prop : *((data_t*)data)->propsToExclude)
            if(meta.find(prop.c_str()) != meta.end())
                return;

        {
            std::set<std::string>& written = ((data_t*)data)->written;
            if(written.find(port_buffer) != written.end()) {
                // we have already saved this port - duplicate path?
                // TODO: should this trigger a warning?
                return;
            }
            else
                written.insert(port_buffer);
        }

        char loc[buffersize] = ""; // buffer to hold the dispatched path
        rtosc_arg_val_t arg_vals_default[max_arg_vals];
        rtosc_arg_val_t arg_vals_runtime[max_arg_vals];
        // buffer to hold the message (i.e. /port ..., without port's bases)
        char buffer_with_port[buffersize];
        char strbuf[buffersize]; // temporary string buffer for pretty-printing

        std::string* res = &((data_t*)data)->res;
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

                // example 1:
                // p->name: "Pstages::i"
                // loc:     "/insefx5/EQ/filter3/Pstages"
                // old_end:                      ^
                // buffer_with_port: "Pstages::i"  // will be overwritten

                // example 2:
                // p->name: "VoicePar#8/Enabled::T:F"
                // loc:     "/part0/kit0/adpars/VoicePar5/Enabled"
                // old_end:                     ^
                // buffer_with_port: "VoicePar#8/Enabled::T:F"  // will be overwritten

                size_t nargs_runtime_cur =
                    helpers::get_value_from_runtime(runtime, *p,
                                                    buffersize, loc, old_end,
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
                rtosc_av_arr_len_set(arg_vals_runtime, nargs_runtime-1);
                rtosc_av_arr_type_set(arg_vals_runtime, arg_vals_runtime[1].type);
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

                    // if default and runtime are 2 arrays with common suffix,
                    // do not print the suffix
                    nargs_runtime = first_equal_index(arg_vals_default, arg_vals_runtime,
                                                      nargs_default, nargs_runtime);

                    *res += port_buffer;
                    rtosc_print_arg_vals(arg_vals_runtime, nargs_runtime,
                                         cur_value_pretty + 1, buffersize - 1,
                                         NULL, strlen(port_buffer) + 1);
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
                            return a->type == 'a' ? rtosc_av_arr_len(a) : 1; };
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

    walk_ports(&ports, port_buffer, buffersize, &data, on_reach_port, false,
               runtime);

    if(data.res.length()) // remove trailing newline
        data.res.resize(data.res.length()-1);

    std::swap(data.written, alreadyWritten);

    return data.res;
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

int savefile_dispatcher_t::default_response(size_t nargs)
{
    return nargs;
}

int savefile_dispatcher_t::on_dispatch(size_t, char *,
                                       size_t, size_t nargs,
                                       rtosc_arg_val_t *)
{
    return default_response(nargs);
}

int dispatch_printed_messages(const char* messages,
                              const Ports& ports, void* runtime,
                              savefile_dispatcher_t* dispatcher)
{
    constexpr std::size_t buffersize = 8192;
    char messagebuf[buffersize];
    int rd, rd_total = 0;
    int nargs;
    int msgs_read = 0;
    bool ok = true;

    savefile_dispatcher_t dummy_dispatcher;
    if(!dispatcher)
        dispatcher = &dummy_dispatcher;
    dispatcher->ports = &ports;
    dispatcher->runtime = runtime;

    struct message_t
    {
        std::string portname;
        std::vector<rtosc_arg_val_t> arg_vals;
        std::vector<std::size_t> dependees;
        std::vector<char> strbuf;
    };
    std::vector<message_t> message_v;
    std::map<std::string, message_t*> message_map;

    {
        msgs_read = 0;
        rd_total = 0;
        const char* msg_ptr = messages;
        while(*msg_ptr && ok)
        {
            nargs = rtosc_count_printed_arg_vals_of_msg(msg_ptr);
            if(nargs >= 0)
            {
                message_t m;
                m.arg_vals.resize(nargs);
                std::vector<char> portname_v;
                portname_v.resize(buffersize);

                m.strbuf.resize(buffersize);
                rd = rtosc_scan_message(msg_ptr, portname_v.data(), buffersize,
                                        m.arg_vals.data(), nargs, m.strbuf.data(), buffersize);

                m.portname = portname_v.data();
                rd_total += rd;
                message_v.emplace_back(std::move(m));
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

    for(message_t& msg : message_v)
    {
        message_map.emplace(msg.portname, &msg);
    }

    auto rel2abs=[](const char* relative_path, const std::string& base) -> std::string
    {
        std::string abs = base;
        std::string::size_type last_slash = abs.find_last_of('/');
        assert(last_slash != std::string::npos);
        abs.resize(last_slash+1);
        abs.append(relative_path);
        return abs;
    };

    // add "rEnabledBy", "rDepends" and "rDefaultDepends" edges
    for(std::pair<const std::string, message_t*>& pr : message_map)
    {
        std::string portname = pr.first;
        assert(portname[0] == '/');
        // this port and all parent ports can be enabled by another port, so check them all
        for(std::string::size_type last_slash;
            portname.size() && (last_slash = portname.find_last_of('/')) != std::string::npos;
              portname.resize(last_slash))
        {
            const Port* port = ports.apropos(portname.c_str());
            if(port)
            {
                //printf("dependency check: %s\n",portname.c_str());
                const char* dep_types[3] = { "enabled by", "depends", "default depends" };
                for(const char* dep_type : dep_types)
                {
                    const char* enabled_by = port->meta()[dep_type];
                    if(enabled_by)
                    {
                        auto itr = message_map.find(rel2abs(enabled_by, portname));
                        if(itr != message_map.end())
                            itr->second->dependees.push_back(std::distance(message_v.data(), pr.second));
                    }
                }
            }
        }
    }

    // topologic sort
    std::vector<bool> already_done(message_v.size(), false);
    std::vector<std::size_t> order;
    order.reserve(message_v.size());

    // kahn's algorithm

    std::vector<std::size_t> n_input_edges(message_v.size(), 0);
    for(const message_t& m : message_v)
        for(std::size_t dep : m.dependees)
            ++n_input_edges[dep];

    std::queue<std::size_t> no_incoming_edge;
    for(std::size_t i = 0; i < n_input_edges.size(); ++i)
        if(n_input_edges[i] == 0)
            no_incoming_edge.push(i);

    while(!no_incoming_edge.empty())
    {
        std::size_t m_id = no_incoming_edge.front();
        no_incoming_edge.pop();
        order.push_back(m_id);
        for(std::size_t dependee : message_v[m_id].dependees)
            if(--n_input_edges[dependee] == 0)
                no_incoming_edge.push(dependee);
    }

    // check result
    assert(order.size() == message_v.size());
    for(std::size_t i = 0; i < n_input_edges.size(); ++i)
        assert(n_input_edges[i] == 0);  // cyclic rDepends/rEnables/... in the metadata?
    std::vector<std::size_t> order_copy = order;
    std::sort(order_copy.begin(), order_copy.end());
    for(std::size_t i = 0; i < order_copy.size(); ++i)
    {
        assert(order_copy[i] == i);
    }

//#define DUMP_SAVEFILE_GRAPH
#ifdef DUMP_SAVEFILE_GRAPH
    {
        const char* fname = "/tmp/savefile-graph.dot";
        FILE* dot = fopen(fname, "w");
        if(dot)
        {
            fputs("digraph D {\n\n", dot);
            for(std::size_t i = 0; i < message_v.size(); ++i)
                fprintf(dot, "%lu [label=\"%s\"];\n", i, message_v[i].portname.c_str());
            fputs("\n", dot);
            /*for(const message_t& m : message_v)
                for(std::size_t dep : m.dependees)
                    fprintf(dot, "%s -> %s\n", m.portname.c_str(), message_v[dep].portname.c_str());*/
            for(std::size_t i = 0; i < message_v.size(); ++i)
                for(std::size_t dep : message_v[i].dependees)
                    fprintf(dot, "%lu -> %lu\n", i, dep);
            fputs("\n\n}\n", dot);
            fclose(dot);
        }
        else
        {
            fprintf(stderr, "Saving the graph to %s\n. Not terminating.", fname);
        }
    }
#endif

    // finally, handling messages - in correct order
    for(std::size_t order_id : order)
    {
        message_t& message = message_v[order_id];
        char portname[buffersize];
        fast_strcpy(portname, message.portname.c_str(), buffersize);

        // let the user modify the message and the args
        // the argument number may have changed, or the user
        // wants to discard the message or abort the savefile loading
        nargs = message.arg_vals.size();
        // nargs << 1 is usually too much, but it allows the user to use
        // these values (using on_dispatch())
        const size_t maxargs = std::max(nargs << 1, 16);
        message.arg_vals.resize(maxargs); // allow the user to modify arguments
        nargs = dispatcher->on_dispatch(buffersize, portname,
                                        maxargs, nargs, message.arg_vals.data());

        if(nargs == savefile_dispatcher_t::abort)
        {
            ok = false;
        }
        else
        {
            if(nargs != savefile_dispatcher_t::discard)
            {
                const rtosc_arg_val_t* arg_val_ptr;
                bool is_array;
                if(nargs && message.arg_vals[0].type == 'a')
                {
                    is_array = true;
                    // arrays of arrays are not yet supported -
                    // neither by rtosc_*message, nor by the inner for
                    // loop below.
                    // arrays will probably have an 'a' (or #)
                    assert(rtosc_av_arr_type(message.arg_vals.data()) != 'a' &&
                           rtosc_av_arr_type(message.arg_vals.data()) != '#');
                    // we won't read the array arg val anymore
                    --nargs;
                    arg_val_ptr = message.arg_vals.data() + 1;
                }
                else {
                    is_array = false;
                    arg_val_ptr = message.arg_vals.data();
                }

                char* portname_end = portname + message.portname.length();

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
                    STACKALLOC(rtosc_arg_t, vals, val_max);
                    STACKALLOC(char, argstr, val_max+1);

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

                    rtosc_amessage(messagebuf, buffersize, portname,
                                   argstr, vals);

                    ok = (*dispatcher)(messagebuf);
                    //printf("%s, %s, %d -> %s\n", messagebuf, portname, nargs, ok ? "yes": "no");
                }
            }
        }

        if (!ok)
            break;
    }
    return ok ? msgs_read : -rd_total-1;
}

std::string save_to_file(const Ports &ports, void *runtime,
                         const char *appname, rtosc_version appver,
                         std::set<std::string>& alreadyWritten,
                         const std::vector<std::string> &propsToExclude,
                         std::string file_str)
{
    char rtosc_vbuf[12], app_vbuf[12];

    if(file_str.empty())
    {
        {
            rtosc_version rtoscver = rtosc_current_version();
            rtosc_version_print_to_12byte_str(&rtoscver, rtosc_vbuf);
            rtosc_version_print_to_12byte_str(&appver, app_vbuf);
        }

        file_str += "% RT OSC v"; file_str += rtosc_vbuf; file_str += " savefile\n"
               "% "; file_str += appname; file_str += " v"; file_str += app_vbuf; file_str += "\n";
    }
    else
    {
        // append mode - no header
    }
    file_str += get_changed_values(ports, runtime, alreadyWritten, propsToExclude);

    return file_str;
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

