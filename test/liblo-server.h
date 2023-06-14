#ifndef LIBLO_SERVER_H
#define LIBLO_SERVER_H

#include <lo/lo_types.h>
#include <vector>

#include <rtosc/port-checker.h>

namespace rtosc {

//! Liblo implementation of port_checker::server
class liblo_server : public port_checker::server
{
    lo_server srv;
    lo_address target;

    friend int handle_st(const char *path, const char *types, lo_arg **argv,
                         int argc, lo_message msg, void *data);

public:
    void on_recv(const char *path, const char *types,
                 lo_arg **argv, int argc, lo_message msg);

    bool send_msg(const char* address,
                  size_t nargs, const rtosc_arg_val_t* args) override;

    bool _wait_for_reply(std::vector<char>* buffer,
                         std::vector<rtosc_arg_val_t> * args,
                         int n0, int n1) override;

    void vinit(const char* target_url) override;

    using server::server;
};

}

#endif // LIBLO_SERVER_H
