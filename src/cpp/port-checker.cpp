#include <cassert>
#include <vector>
#include <set>
#include <cstring>
#include <string>
#include <cstdarg>

#include <rtosc/ports.h>
#include <rtosc/port-checker.h>
#include <rtosc/pretty-format.h>
#include <lo/lo.h>
#include <lo/lo_lowlevel.h>

namespace rtosc {

port_checker::port_checker()
{

}

static void liblo_error_cb(int i, const char *m, const char *loc)
{
    fprintf(stderr, "liblo :-( %d-%s@%s\n",i,m,loc);
}

static int handle_st(const char *path, const char *types, lo_arg **argv,
                     int argc, lo_message msg, void *data)
{
    static_cast<port_checker::server*>(data)->on_recv(path, types,
                                                      argv, argc, msg);
    return 0;
}

void port_checker::server::on_recv(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
    printf("on_recv: %s, %d, %s\n", path, waiting,
           exp_path);
    if(waiting && exp_path)
    {
        if(!strcmp(exp_path, path))
        {
            size_t len = lo_message_length(msg, path);
            *last_buffer = std::vector<char>(len);
            size_t written;
            lo_message_serialise(msg, path, last_buffer->data(), &written);
            if(written > last_buffer->size()) // ouch...
                throw std::runtime_error("can not happen, lo_message_length has been used");

            last_args->resize(argc);
            if((size_t)argc > max_args)
                throw std::runtime_error("too many arguments returned");
            for(int i = 0; i < argc; ++i)
            {
                (*last_args)[i].val = rtosc_argument(last_buffer->data(), i);
                (*last_args)[i].type = types[i];
            }

            waiting = false;
        }
    }
}

void port_checker::server::init(const char* target_url)
{
    target = lo_address_new_from_url(target_url);
    if(!target || !lo_address_get_url(target) ||
       !lo_address_get_port(target) ||
        lo_address_get_protocol(target) != LO_UDP)
        throw std::runtime_error("invalid address");

    srv = lo_server_new_with_proto(NULL, LO_UDP, liblo_error_cb);
    if(srv == nullptr)
        throw std::runtime_error("Could not create lo server");
    lo_server_add_method(srv, NULL, NULL, handle_st, this);
    rtosc_arg_val_t hi;
    hi.val.s = "hi"; hi.type = 's';
    std::vector<rtosc_arg_val_t> reply;
    std::vector<char> buf;
    send_msg("/echo", 1, &hi);
    wait_for_reply("/echo", &buf, &reply);
}

bool port_checker::send_msg(const char* address, size_t nargs, const rtosc_arg_val_t* args)
{ // TODO: function useless?
    return sender.send_msg(address, nargs, args);
}

bool port_checker::server::send_msg(const char* address, size_t nargs, const rtosc_arg_val_t* args)
{
    char buffer[2048];
    int len = rtosc_avmessage(buffer, sizeof(buffer), address, nargs, args);
    int res = 0;
    lo_message msg  = lo_message_deserialise(buffer, len, &res);
    if(msg == nullptr)
        throw std::runtime_error("could not deserialize message");
    printf("address: %s.\n", address);
    lo_message_pp(msg);

    res = lo_send_message_from(target, srv, buffer, msg);
    if(res == -1)
        throw std::runtime_error("Could not send message");
    return true;
}

bool port_checker::server::wait_for_reply(const char* _exp_path, std::vector<char>* buffer, std::vector<rtosc_arg_val_t> * args)
{
    exp_path = _exp_path;
    last_args = args;
    last_buffer = buffer;

    int tries_left = 100;
    while(tries_left-->1 && waiting)
    {
        lo_server_recv_noblock(srv, 10 /* 0,01 seconds */);
        // message will be dispatched to the server's callback
    }
    printf("tries left: %d\n", tries_left);
    waiting = true; // prepare for next round

    return tries_left; // TODO
}

void port_checker::check_port(const char* loc, const char* portname,
                              const char* metadata, int meta_len)
{
    std::string full_path = "/"; full_path += loc; full_path += portname;
    full_path.resize(full_path.find(':'));
    printf("fp: %s\n", full_path.c_str());
    rtosc::Port::MetaContainer meta(metadata);
    if(*metadata && meta_len)
    {
        bool is_parameter = false;
        const char* default_val = nullptr, * default_0 = nullptr;
        for(const auto x : meta)
        {
            if(!strcmp(x.title, "parameter"))
                is_parameter = true;
            else if(!strcmp(x.title, "default"))
                default_val = x.value;
            else if(!strcmp(x.title, "default 0"))
                default_0 = x.value;
        }

        if(is_parameter)
        {
            const char* args_to_send = portname - 1;
            do
            {
                args_to_send = strchr(args_to_send + 1, ':');

            } while(args_to_send && args_to_send[1] == ':');

            if(args_to_send)
            {
                const char* def_str = default_val
                                        ? default_val
                                        : default_0
                                                ? default_0
                                                : nullptr;

                if(def_str)
                {
                    int nargs = rtosc_count_printed_arg_vals(def_str);
                    if(nargs <= 0)
                    {
                        // TODO: add issue
                        assert(nargs > 0); // parse error => error in the metadata?
                    }
                    else
                    {
                        char strbuf[4096];
                        rtosc_arg_val_t query_args[nargs];
                        rtosc_scan_arg_vals(def_str, query_args, nargs, strbuf, sizeof(strbuf));
                        send_msg(full_path.c_str(), nargs, query_args);

                        std::vector<rtosc_arg_val_t> args;
                        std::vector<char> strbuf2;

                        int res = sender.wait_for_reply("/paths", &strbuf2, &args);
/*                        if(!res)
                            throw std::runtime_error("no reply from path-search");*/
                        if(res && args.size() % 2)
                            throw std::runtime_error("bad reply from path-search");

                    }
                }
            }
        }
    }
}

void port_checker::do_checks(char* loc, int loc_size)
{
    char* old_loc = loc + strlen(loc);
    printf("Checking: \"%s\"...\n", loc);

    rtosc_arg_val_t query_args[2];
    query_args[0].type = query_args[1].type = 's';
    query_args[0].val.s = loc;
    query_args[1].val.s = "";
    send_msg("/path-search", 2, query_args);
    std::vector<rtosc_arg_val_t> args;
    std::vector<char> strbuf;

    int res = sender.wait_for_reply("/paths", &strbuf, &args);
    if(!res)
        throw std::runtime_error("no reply from path-search");
    if(args.size() % 2)
        throw std::runtime_error("bad reply from path-search");

    size_t port_max = args.size() >> 1;
    //printf("found %lu subports\n", port_max);
    for(size_t port_no = 0; port_no < port_max; ++port_no)
    {
        if(args[port_no << 1].type != 's' || args[(port_no << 1) + 1].type != 'b')
            throw std::runtime_error("Invalid paths reply: bad types");
        const char* portname = args[port_no << 1].val.s;
        int portlen = strlen(portname);
        rtosc_blob_t& blob = args[(port_no << 1) + 1].val.b;
        const char* metadata = (const char*)blob.data;
        int32_t meta_len = blob.len;
        if(!metadata)
            metadata = "";
        bool has_subports = portname[portlen-1] == '/';
        //printf("port %s%s (%lu/%lu), has subports: %d\n", loc, portname, port_no, port_max, has_subports);

        if(has_subports)
        {
            if(loc_size > portlen)
            {
                strcpy(old_loc, portname);
                char* hashsign = strchr(old_loc, '#');
                if(hashsign)
                {
                    // #16 => 000
                    for(; *hashsign && *hashsign != '/'; ++hashsign)
                        *hashsign = '0';
                }
                do_checks(loc, loc_size - portlen);
            }
            else
                throw std::runtime_error("portname too long");
        }
        else
        {
            // TODO: not implemented yet
            // check_port(loc, portname, metadata, meta_len);
            (void) meta_len;
        }
        *old_loc = 0;
    }
}

bool port_checker::operator()(const char* url)
{
    sendtourl = url;

    sender.init(url);
    other.init(url);

    char loc_buffer[4096] = { 0 };
    do_checks(loc_buffer, sizeof(loc_buffer));

    return true;
}

}

