#include <iostream>
#include <cstring>

#include <lo/lo.h>
#include <lo/lo_lowlevel.h>
#include <rtosc/arg-val.h>

#include "liblo-server.h"

namespace rtosc {

static void liblo_error_cb(int i, const char *m, const char *loc)
{
    std::cerr << "liblo :-( " << i << "-" << m << "@" << loc << std::endl;
}

int handle_st(const char *path, const char *types, lo_arg **argv,
                     int argc, lo_message msg, void *data)
{
    static_cast<liblo_server*>(data)->on_recv(path, types,
                                              argv, argc, msg);
    return 0;
}

void liblo_server::on_recv(const char *path, const char *types,
                           lo_arg **argv, int argc, lo_message msg)
{
    (void)argv;
#ifdef DEBUG_PORT_CHECKER
    std::cout << "on_recv: " << path << ", " << waiting << std::endl;
    for(std::vector<const char*>* exp_strs = exp_paths_n_args; exp_strs->size();
        ++exp_strs, ++_replied_path)
    {
        std::cout << " - exp. path: " << (*exp_strs)[0] << std::endl;
        for(std::size_t i = 1; i < exp_strs->size(); ++i)
            std::cout << "   - exp. arg " << i-1 << ": " << (*exp_strs)[i] << std::endl;
    }
#endif
    if(waiting && exp_paths_n_args[0].size())
    {
        _replied_path = 0;
        for(std::vector<const char*>* exp_strs = exp_paths_n_args; exp_strs->size();
            ++exp_strs, ++_replied_path)
        if(!strcmp((*exp_strs)[0], path))
        {
#ifdef DEBUG_PORT_CHECKER
            std::cout << "on_recv: match:" << std::endl;
            lo_message_pp(msg);
#endif
            size_t len = lo_message_length(msg, path);
            *last_buffer = std::vector<char>(len);
            size_t written;
            lo_message_serialise(msg, path, last_buffer->data(), &written);
            if(written > last_buffer->size()) // ouch...
                throw std::runtime_error("can not happen, "
                                         "lo_message_length has been used");

            server::handle_recv(argc, types, exp_strs);
            break;
        }
    }
}

void liblo_server::vinit(const char* target_url)
{
    target = lo_address_new_from_url(target_url);
    if(!target || !lo_address_get_url(target) ||
       !lo_address_get_port(target) ||
        lo_address_get_protocol(target) != LO_UDP)
        throw std::runtime_error("invalid address");

    srv = lo_server_new_with_proto(nullptr, LO_UDP, liblo_error_cb);
    if(srv == nullptr)
        throw std::runtime_error("Could not create lo server");
    lo_server_add_method(srv, nullptr, nullptr, handle_st, this);
}

bool liblo_server::send_msg(const char* address,
                            size_t nargs, const rtosc_arg_val_t* args)
{
    char buffer[8192];
    int len = rtosc_avmessage(buffer, sizeof(buffer), address, nargs, args);
    int res = 0;
    lo_message msg  = lo_message_deserialise(buffer, len, &res);
    if(msg == nullptr) {
        std::cout << "liblo error code: " << res << std::endl;
        throw std::runtime_error("could not deserialize message");
    }
#ifdef DEBUG_PORT_CHECKER
    std::cout << "send message: " << address << ", args:" << std::endl;
    lo_message_pp(msg);
#endif
    res = lo_send_message_from(target, srv, buffer, msg);
    if(res == -1)
        throw std::runtime_error("Could not send message");
    return true;
}

bool liblo_server::_wait_for_reply(std::vector<char>* buffer,
                                   std::vector<rtosc_arg_val_t> * args,
                                   int n0, int n1)
{
    (void)n0;
    exp_paths_n_args[n1].clear();

    last_args = args;
    last_buffer = buffer;

    // allow up to 1000 recv calls = 1s
    // if there's no reply at all after 0.5 seconds, abort
    const int timeout_initial = timeout_msecs;
    int tries_left = 1000, timeout = timeout_initial;
    while(tries_left-->1 && timeout-->1 && waiting) // waiting is set in "on_recv"
    {
        int n = lo_server_recv_noblock(srv, 1 /* 0,001 second = 1 milli second */);
        if(n)
            timeout = timeout_initial;
        // message will be dispatched to the server's callback
    }
    waiting = true; // prepare for next round

    return tries_left && timeout;
}

}

