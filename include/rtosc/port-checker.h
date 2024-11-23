/*
 * Copyright (c) 2017-2023 Johannes Lorenz
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
 * @test port-checker-tester.cpp
 */

#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <rtosc/rtosc.h>

#ifndef RTOSC_PORT_CHECKER_H
#define RTOSC_PORT_CHECKER_H

namespace rtosc {

enum class issue
{
    // general:
    //trailing_slash_without_subports, // not working currently
    duplicate_parameter,
    // callbacks:
    parameter_not_queryable,
    parameter_not_replied,
    parameter_not_broadcast,
    // port metadata:
    enumeration_port_not_si,
    roptions_port_not_si,
    duplicate_mapping,
    enabled_port_not_replied,
    enabled_port_bad_reply,
    // default values:
    rdefault_missing,
    rdefault_multiple,
    rpreset_multiple,
    rpreset_without_rdefaultdepends,
    rdefault_without_rparameter,
    invalid_default_format,
    bundle_size_not_matching_rdefault,
    rdefault_not_infinite,
    default_cannot_canonicalize,

    number
};

enum class severity
{
    error, //!< Wrong and must be fixed
    warning, //!< Wrong and could be fixed
    hint //!< Maybe wrong
};

struct issue_t
{
    issue issue_id;
    const char* shortmsg;
    const char* msg;
    severity sev;
};

class port_error : public std::runtime_error
{
public:
    char m_port[256];
    port_error(const char* errmsg, const char* port);
};

//! Class to check all ports of a remote (UDP) controlled app
class port_checker
{
    // statistical data
    time_t start_time, finish_time;
    unsigned ports_checked = 0; //!< ports checked and not disabled

    //! issue types
    std::map<issue, issue_t> m_issue_types;
    //! issues found: type and port each
    std::multimap<issue, std::string> m_issues;
    //! ports that have been skipped, usually because the have been disabled
    std::set<std::string> m_skipped;

    //! URL of the app
    std::string sendtourl;

public:
    class server
    {
    protected:
        volatile bool waiting = true;
        int timeout_msecs;

        constexpr static int max_exp_paths = 15;
        /**
            max_exp_paths string vectors
            each first string is the expected paths
            all further strings must match the respective arguments, if they
            are of type 's'
        */
        std::vector<const char*> exp_paths_n_args[max_exp_paths+1];

        // variables from the last expected reply
        int _replied_path;
        std::vector<char>* last_buffer;
        std::vector<rtosc_arg_val_t>* last_args;

        virtual bool _wait_for_reply(std::vector<char>* buffer,
                                     std::vector<rtosc_arg_val_t> * args,
                                     int n0, int n1) = 0;
        template<class ...Args>
        bool _wait_for_reply(std::vector<char> *buffer,
                             std::vector<rtosc_arg_val_t>* args,
                             int n0, int n1, const char* path0, Args ...more_paths) {
            if(path0)
            {
                n1 = n0 + 1; // do not clear this vector anymore, it's in use
                exp_paths_n_args[n0].push_back(path0);
            }
            else
            {
                ++n0; // i.e. n0 == n1
                exp_paths_n_args[n0].clear();
            }
            return _wait_for_reply(buffer, args, n0, n1, more_paths...);
        }

        void handle_recv(int argc, const char *types, const std::vector<const char *> *exp_strs);

        virtual void vinit(const char *target_url) = 0;

    public:
        server(int t) : timeout_msecs(t) {}
        virtual ~server() {}

        //! Return which of the expected paths from wait_for_reply() has
        //! has been received
        int replied_path() const { return _replied_path; }

        void init(const char *target_url);

        virtual bool send_msg(const char* address,
              size_t nargs, const rtosc_arg_val_t* args) = 0;

        /**
            Wait for a reply matching any of the C strings from
            "exp_paths_n_args...". Any other received messages are discarded.
            @param buffer Pointer to vector where the address will be
                written
            @param args Pointer to vector where args will be put
            @param exp_paths Matches that are considered. Those must be
                separated with nullptrs. Each match begins with the allowed
                path, followed by optional string matches for the first
                arguments (only type 's' is being compared)
        */
        template<class ...Args>
        bool wait_for_reply(std::vector<char>* buffer,
                            std::vector<rtosc_arg_val_t>* args,
                            Args ...exp_paths) {
            exp_paths_n_args[0].clear();
            return _wait_for_reply(buffer, args, 0, 0, exp_paths...);
        }
    };

private:
    server* const sender; //!< send and check replies
    server* const other; //!< check broadcasts

    //! send a message via the sender
    bool send_msg(const char *address,
                  size_t nargs, const rtosc_arg_val_t *args);
    /**
     * Execute all checks recursively under a given OSC path
     * @param loc the OSC root path for the recursive checks
     * @param loc_size size of the buffer located at @p loc
     * @param check_defaults Whether default values should be checked in this
     *        port and all subports
     */
    void do_checks(char *loc, int loc_size, bool check_defaults = true);
    //! Check a port which has no subports
    void check_port(const char *loc, const char *portname,
                    const char *metadata, int meta_len, bool check_defaults);
    bool port_is_enabled(const char *loc, const char *port,
                         const char *metadata);
    void m_sanity_checks(std::vector<int> &issue_type_missing) const;
    std::set<issue> issues_not_covered() const;

public:
    port_checker(server* sender, server* other) :
        sender(sender), other(other) {}

    //! Let the port checker connect to url and find issues for all ports
    //! @param url URL in format osc.udp://xxx.xxx.xxx.xxx:ppppp/, or just
    //!     ppppp (which means osc.udp://127.0.0.1:ppppp/)
    bool operator()(const char* url);

    //! Return issue types
    const std::map<issue, issue_t>& issue_types() const;
    //! Return issues found: type and port each
    const std::multimap<issue, std::string>& issues() const;
    //! Print a listing of all ports with issues in Markdown format
    void print_evaluation() const;
    //! Return the number of serious issues found
    int errors_found() const;

    //! Check if the port checker itself has issues
    bool sanity_checks() const;
    //! Print if the port checker itself has issues
    bool print_sanity_checks() const;

    //! Return if port checker has at least one port failing for each issue type
    bool coverage() const;
    //! Print if port checker has at least one port failing for each issue type
    //! @note Only used for testing the port checker itself inside rtosc!
    void print_coverage(bool coverage_report = true) const;
    //! Print a list of error types that were OK for all checked ports
    void print_not_affected() const;

    //! Return ports that have been skipped,
    //! usually because the have been disabled
    const std::set<std::string> &skipped() const;
    //! Print ports that have been skipped,
    //! usually because the have been disabled
    void print_skipped() const;

    //! Print statistics like number of ports and time consumed
    void print_statistics() const;
};

}

#endif // RTOSC_PORT_CHECKER
