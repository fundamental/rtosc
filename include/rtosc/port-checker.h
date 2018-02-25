/*
 * Copyright (c) 2017 Johannes Lorenz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @file port-checker.h
 * Header providing the port_checker class
 *
 * @test port-checker.c
 */

#include <string>
#include <vector>
#include <map>
#include <rtosc/rtosc.h>
#include <lo/lo_types.h>

#ifndef RTOSC_PORT_CHECKER_H
#define RTOSC_PORT_CHECKER_H

namespace rtosc {

enum class port_issue
{
    // callbacks:
    parameter_not_replied,
    parameter_not_broadcast,
    rapply_missing_in_callback,
    // port metadata:
    option_port_not_ics,
    // default values:
    rdefault_missing,
    multiple_rdefault,
    rdefault_without_rparameter,
    bundle_size_not_matching_rdefault,
    rdefault_not_infinite
};

class port_checker
{
        typedef int (*cb_t)(const char*, const char*,
                            lo_arg**, int, lo_message, void*);

        std::multimap<port_issue, std::string> issues;
        std::string sendtourl;
public: // TODO: static friend function instead
        class server
        {
            volatile bool waiting = true;

            lo_server srv;
            lo_address target;

            const char* exp_path;

            // variables from the last expected reply
            std::vector<char>* last_buffer;
            std::string* last_path; //!< unused yet
            std::vector<rtosc_arg_val_t>* last_args;
        public:
            std::size_t get_last_args(rtosc_arg_val_t *&av);
            void init(const char *target_url);
            bool send_msg(const char *address, size_t nargs, const rtosc_arg_val_t *args);
            bool wait_for_reply(const char *_exp_path, std::vector<char> *buffer, std::vector<rtosc_arg_val_t>*args);
            void on_recv(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
        };
private:
        static constexpr const std::size_t max_args = 4096;
        server sender, other;
public:
        port_checker();
        bool operator()(const char* url);
        int handle_sender(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
        int handle_other(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
private:
        //! send a message via the sender
        bool send_msg(const char *address, size_t nargs, const rtosc_arg_val_t *args);
        /**
         * Execute all checks recursively under a given OSC path
         * @param loc the OSC root path for the recursive checks
         * @param loc_size size of the buffer located at @p loc
         */
        void do_checks(char *loc, int loc_size);
        void check_port(const char *loc, const char *portname, const char *metadata, int meta_len);
};

}

#endif // RTOSC_PORT_CHECKER
