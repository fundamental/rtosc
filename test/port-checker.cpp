#include <cassert>
#include <vector>
#include <set>
#include <cstring>
#include <string>
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <iomanip>

#include <rtosc/ports.h>
#include <rtosc/pretty-format.h>
#include <rtosc/arg-val-math.h>
#include <lo/lo.h>
#include <lo/lo_lowlevel.h>

#include "port-checker.h"

namespace rtosc {

static constexpr const issue_t _m_issue_types_arr[(int)issue::number] =
{
{
    issue::duplicate_parameter,
    "multiple parameters with same name",
    "parameters are duplicates",
    severity::hint
},{
    issue::parameter_not_queryable,
    "parameter not queryable",
    "parameters can not be queried",
    severity::error
},{
    issue::parameter_not_replied,
    "parameter not replied",
    "parameters are not replied",
    severity::error
},{
    issue::parameter_not_broadcast,
    "parameter not broadcast",
    "parameters are not broadcast",
    severity::warning
},{
    issue::option_port_not_si,
    "option port not \"S\" or \"i\"",
    "parameter using \"enumerated\" property do not accept \"i\" or \"S\"",
    severity::error
},{
    issue::rdefault_missing,
    "rDefault missing",
    "parameters are missing default values",
    severity::warning,
},{
    issue::rdefault_multiple,
    "multiple rDefault",
    "parameters have multiple mappings for rDefault",
    severity::error,
},{
    issue::rpreset_multiple,
    "multiple rPreset",
    "parameters have multiple rPreset with the same number",
    severity::error,
},{
    issue::rdefault_without_rparameter,
    "rDefault without rparameter",
    "ports have rDefault, but not rParameter",
    severity::hint,
},{
    issue::invalid_default_format,
    "invalid default format",
    "parameters have default values which could not be parsed",
    severity::error
},{
    issue::bundle_size_not_matching_rdefault,
    "bundle size not matching rDefault",
    "bundle parameters have rDefault array with different size",
    severity::error
},{
    issue::rdefault_not_infinite,
    "rDefault not infinite",
    "parameters should use ellipsis instead of fixed sizes",
    severity::hint
},{
    issue::default_cannot_canonicalize,
    "cannot canoncicalize defaults",
    "parameters have default values with non-existing enumeration values",
    severity::error
},{
    issue::duplicate_mapping,
    "duplicate mapping",
    "ports have the same mapping twice (rOptions()?)",
    severity::error
},{
    issue::enabled_port_not_replied,
    "enabled-port did not reply",
    "ports have enabled-ports which do not reply",
    severity::error
},{
    issue::enabled_port_bad_reply,
    "bad reply from enabled-port",
    "ports have enabled-ports which did not reply \"T\" or \"F\"",
    severity::error
}
};

static void liblo_error_cb(int i, const char *m, const char *loc)
{
    std::cerr << "liblo :-( " << i << "-" << m << "@" << loc << std::endl;
}

int handle_st(const char *path, const char *types, lo_arg **argv,
                     int argc, lo_message msg, void *data)
{
    static_cast<port_checker::server*>(data)->on_recv(path, types,
                                                      argv, argc, msg);
    return 0;
}

void port_checker::server::on_recv(const char *path, const char *types,
                                   lo_arg **argv, int argc, lo_message msg)
{
    (void)argv;
//  std::cout << "on_recv: " << path << ", " << waiting << std::endl;
//  for(const char** exp_path = exp_paths; *exp_path;
//      ++exp_path, ++_replied_path)
//      std::cout << " - exp: " << *exp_path << std::endl;
    if(waiting && exp_paths && exp_paths[0])
    {
        _replied_path = 0;
        for(const char** exp_path = exp_paths; *exp_path;
            ++exp_path, ++_replied_path)
        if(!strcmp(*exp_path, path))
        {
            size_t len = lo_message_length(msg, path);
            *last_buffer = std::vector<char>(len);
            size_t written;
            lo_message_serialise(msg, path, last_buffer->data(), &written);
            if(written > last_buffer->size()) // ouch...
                throw std::runtime_error("can not happen, "
                                         "lo_message_length has been used");

            last_args->resize(argc);
            for(int i = 0; i < argc; ++i)
            {
                (*last_args)[i].val = rtosc_argument(last_buffer->data(), i);
                (*last_args)[i].type = types[i];
            }

            waiting = false;
            break;
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

    srv = lo_server_new_with_proto(nullptr, LO_UDP, liblo_error_cb);
    if(srv == nullptr)
        throw std::runtime_error("Could not create lo server");
    lo_server_add_method(srv, nullptr, nullptr, handle_st, this);

    rtosc_arg_val_t hi[2];
    hi[0].type = hi[1].type = 's';
    hi[0].val.s = hi[1].val.s = "";
    send_msg("/path-search", 2, hi);
    std::vector<rtosc_arg_val_t> reply;
    std::vector<char> buf;
    wait_for_reply(&buf, &reply, "/paths");
}

bool port_checker::send_msg(const char* address,
                            size_t nargs, const rtosc_arg_val_t* args)
{
    return sender.send_msg(address, nargs, args);
}

bool port_checker::server::send_msg(const char* address,
                                    size_t nargs, const rtosc_arg_val_t* args)
{
    char buffer[2048];
    int len = rtosc_avmessage(buffer, sizeof(buffer), address, nargs, args);
    int res = 0;
    lo_message msg  = lo_message_deserialise(buffer, len, &res);
    if(msg == nullptr) {
        std::cout << "liblo error code: " << res << std::endl;
        throw std::runtime_error("could not deserialize message");
    }
    // std::cout << "send message: " << address << ", args:" << std::endl;
    // lo_message_pp(msg);

    res = lo_send_message_from(target, srv, buffer, msg);
    if(res == -1)
        throw std::runtime_error("Could not send message");
    return true;
}

bool port_checker::server::_wait_for_reply(std::vector<char>* buffer,
                                           std::vector<rtosc_arg_val_t> * args,
                                           int unused)
{
    (void)unused;
    last_args = args;
    last_buffer = buffer;

    // allow up to 1000 ports = 1s. never wait more than 0.05 seconds
    const int timeout_initial = 50;
    int tries_left = 1000, timeout = timeout_initial;
    while(tries_left-->1 && timeout-->1 && waiting)
    {
        int n = lo_server_recv_noblock(srv, 1 /* 0,001 seconds */);
        if(n)
            timeout = timeout_initial;
        // message will be dispatched to the server's callback
    }
    waiting = true; // prepare for next round

    return tries_left && timeout;
}

// loc: path in which that port is, must not end on '/'
// port: only for printing issues
bool port_checker::port_is_enabled(const char* loc, const char* port,
                                   const char* metadata)
{
    Port::MetaContainer meta(metadata);
    const char* enable_port_rel = meta["enabled by"];
    bool rval = true;
    if(enable_port_rel)
    {
        std::string enable_port = "/";
        enable_port += loc;
        enable_port += enable_port_rel;

        const char* collapsed_loc = Ports::collapsePath(&enable_port[0]);
        send_msg(collapsed_loc, 0, nullptr);

        std::vector<rtosc_arg_val_t> args;
        std::vector<char> strbuf;
        if(sender.wait_for_reply(&strbuf, &args, collapsed_loc)) {
            if(args.size() == 1 &&
               (args[0].type == 'T' || args[0].type == 'F')) {
                rval = (args[0].type == 'T');
            }
            else
                m_issues.emplace(issue::enabled_port_bad_reply,
                               "/" + std::string(loc) + port);
        } else
            m_issues.emplace(issue::enabled_port_not_replied,
                           "/" + std::string(loc) + port);
    }
    return rval;
}

void alternate_arg_val(rtosc_arg_val_t& a)
{
    switch(a.type)
    {
        case 'h': a.val.h = a.val.h ? 0 : 1; break;
        case 't': a.val.t = a.val.t + 1; break;
        case 'd': a.val.d = a.val.d ? 0.0 : 1.0; break;
        case 'c': a.val.i = a.val.i ? 0 : 'x'; break;
        case 'i':
        case 'r': a.val.i = a.val.i ? 0 : 1; break;
        case 'm': a.val.m[3] = a.val.m[3] ? 0 : 1; break;
        case 'S':
        case 's': a.val.s = a.val.s ? "" : "non-empty"; break;
        case 'b': assert(a.val.b.len);
            a.val.b.data[0] = a.val.b.data[0] ? 0 : 1; break;
        case 'f': a.val.f = a.val.f ? 0.0f : 1.0f; break;
        case 'T': a.val.T = 0; a.type = 'F'; break;
        case 'F': a.val.T = 1; a.type = 'T'; break;
    }
}

void port_checker::check_port(const char* loc, const char* portname,
                              const char* metadata, int meta_len,
                              bool check_defaults)
{
    const char* port_args = strchr(portname, ':');
    if(!port_args)
        port_args = portname + strlen(portname);
    std::string full_path = "/"; full_path += loc; full_path += portname;
    std::string::size_type arg_pos = full_path.find(':');
    if(arg_pos != std::string::npos)
    {
        full_path.resize(arg_pos);

        std::string::size_type hash_pos = full_path.find('#');
        int bundle_size = 0;
        if(hash_pos != std::string::npos)
        {
            bundle_size = atoi(full_path.data() + hash_pos + 1);

            // .../port#16... -> .../port0
            full_path[hash_pos] = '0';
            std::string::size_type slash = full_path.find('/', hash_pos + 1);
            full_path.erase(hash_pos + 1,
                (slash == std::string::npos)
                ? std::string::npos // cut string after '#0'
                : slash - (hash_pos + 1) // cut anything between '#0' and '/'
            );
        }

        rtosc::Port::MetaContainer meta(metadata);
        if(*metadata && meta_len)
        {
            bool is_parameter = false, is_enumerated = false;
            int n_default_vals = 0;

            std::map<std::string, int> presets;
            std::map<int, int> mappings; // for rOptions()
            std::map<std::string, int> mapping_values;
            std::vector<std::string> default_values;

            for(const auto x : meta)
            {
                if(!strcmp(x.title, "parameter"))
                    is_parameter = true;
                if(!strcmp(x.title, "enumerated"))
                    is_enumerated = true;
                if(!strcmp(x.title, "no defaults"))
                    check_defaults = false;
                else if(!strcmp(x.title, "default")) {
                    // x.value;
                    ++n_default_vals;
                    default_values.push_back(x.value);
                }
                else if(!strncmp(x.title, "default ", strlen("default ")) &&
                         strcmp(x.title + strlen("default "), "depends")) {
                    ++presets[x.title + strlen("default ")];
                    default_values.push_back(x.value);
                }
                else if(!strncmp(x.title, "map ", 4)) {
                    ++mappings[atoi(x.title + 4)];
                    ++mapping_values[x.value];
                }
            }

            auto raise = [this, loc, portname](issue issue_type) {
                m_issues.emplace(issue_type, "/" + std::string(loc) + portname);
            };

            for(auto pr : mapping_values)
            if(pr.second > 1) {
                raise(issue::duplicate_mapping);
                break;
            }

            for(std::string pretty : default_values)
            {
                int nargs = rtosc_count_printed_arg_vals(pretty.c_str());
                if(nargs <= 0)
                    raise(issue::invalid_default_format);
                else
                {
                    char pretty_strbuf[8096];
                    rtosc_arg_val_t avs[nargs];
                    rtosc_scan_arg_vals(pretty.c_str(), avs, nargs,
                                        pretty_strbuf, sizeof(pretty_strbuf));

                    {
                        int errs_found = canonicalize_arg_vals(avs,
                                                               nargs,
                                                               port_args,
                                                               meta);
                        if(errs_found)
                            raise(issue::default_cannot_canonicalize);
                        else
                        {
                            if(avs[0].type == 'a')
                            {
                                // How many elements are in the array?
                                int arrsize = 0;
                                int cur = 0;
                                int incsize = 0;
                                bool infinite = false;
                                for(rtosc_arg_val_t* ptr = avs + 1;
                                    ptr - (avs+1) < avs[0].val.a.len;
                                    ptr += incsize, arrsize += cur)
                                {
                                    switch(ptr->type)
                                    {
                                        case '-':
                                            cur = ptr->val.r.num;
                                            incsize = 2 + ptr->val.r.has_delta;
                                            if(!cur) infinite = true;
                                            break;
                                        case 'a':
                                            cur = 1;
                                            incsize = ptr->val.a.len;
                                            break;
                                        default:
                                            cur = 1;
                                            incsize = 1;
                                            break;
                                    }
                                }
                                if(!infinite) {
                                    raise(issue::rdefault_not_infinite);
                                    if(arrsize != bundle_size)
                                        raise(issue::bundle_size_not_matching_rdefault);
                                }
                            }
                        }
                    }
                }
            }

            if(is_parameter)
            {
                // check metadata
                if(check_defaults && !n_default_vals && presets.empty())
                    raise(issue::rdefault_missing);
                else {
                    if(n_default_vals > 1)
                        raise(issue::rdefault_multiple);

                    for(auto pr : presets)
                    if(pr.second > 1) {
                            raise(issue::rpreset_multiple);
                            break;
                    }
                }

                if(is_enumerated &&
                   (!strstr(portname, ":i") || !strstr(portname, ":S"))) {
                    raise(issue::option_port_not_si);
                }

                // send and reply...
                // first, get some useful values
                send_msg(full_path.c_str(), 0, nullptr);
                std::vector<rtosc_arg_val_t> args1;
                std::vector<char> strbuf1;
                int res = sender.wait_for_reply(&strbuf1, &args1,
                                                full_path.c_str());

                if(res)
                {
                    // alternate the values...
                    for(rtosc_arg_val_t& a : args1)
                        alternate_arg_val(a);
                    // ... and send them back
                    send_msg(full_path.c_str(), args1.size(), args1.data());
                    args1.clear();
                    strbuf1.clear();
                    res = sender.wait_for_reply(&strbuf1, &args1,
                                                full_path.c_str(),
                                                "/undo_change");

                    if(!res)
                        raise(issue::parameter_not_replied);
                    else {
                        if(sender.replied_path() == 1 /* i.e. undo_change */) {
                            // some apps may reply with undo_change, some may
                            // already catch those... if we get one: retry
                            res = sender.wait_for_reply(&strbuf1, &args1,
                                                        full_path.c_str());
                        }

                        if(res)
                        {
                            res = other.wait_for_reply(&strbuf1, &args1,
                                                       full_path.c_str());
                            if(!res)
                                raise(issue::parameter_not_broadcast);
                        }
                        else raise(issue::parameter_not_replied);
                    }
                }
                else {
                    raise(issue::parameter_not_queryable);
                }
            }
            else {
                if(default_values.size())
                    raise(issue::rdefault_without_rparameter);
            }
        }
    }
}

void port_checker::print_evaluation() const
{
    auto sev_str = [](severity s) -> const char* {
        return s == severity::error ? "**ERROR**"
                                    : s == severity::warning ? "**WARNING**"
                                                             : "**HINT**";
    };
    std::cout << "# Port evaluation" << std::endl;

    issue last_issue = issue::number;
    for(const std::pair<issue, std::string>& p : m_issues)
    {
        if(last_issue != p.first)
        {
            std::cout << std::endl;
            const issue_t& it = m_issue_types.at(p.first);
            std::cout << sev_str(it.sev) << ":" << std::endl
                      << "The following " << it.msg << ":" << std::endl;
            last_issue = p.first;
        }
        std::cout << "* " << p.second << std::endl;
    }
    std::cout << std::endl;
}

std::set<issue> port_checker::issues_not_covered() const
{
    std::set<issue> not_covered;
    for(const auto& t : m_issue_types)
        not_covered.insert(t.second.issue_id);
    for(const auto& p : m_issues)
        not_covered.erase(p.first);
    return not_covered;
}

bool port_checker::coverage() const
{
    return issues_not_covered().empty();
}

void port_checker::print_coverage(bool coverage_report) const
{
    if(coverage_report) {
        std::cout << "# Issue Coverage" << std::endl << std::endl;
        std::cout << "Checking for issues not covered..." << std::endl;
    }
    else {
        std::cout << "# Issues that did not occur" << std::endl << std::endl;
    }
    std::set<issue> not_covered;
    for(const auto& t : m_issue_types)
        not_covered.insert(t.second.issue_id);
    for(const auto& p : m_issues)
        not_covered.erase(p.first);
    if(not_covered.empty())
        std::cout << (coverage_report ? "Tests found for all issue types"
                                      : "None")
                  << std::endl << std::endl;
    else
    {
        std::cout << (coverage_report ? "The following issue types have not "
                                        "been tested"
                                      : "No ports are affected by")
                  << std::endl;
        for(const issue& i : not_covered)
            std::cout << "* " << m_issue_types.at(i).shortmsg << std::endl;
        std::cout << std::endl;
    }
}

void port_checker::print_not_affected() const
{
    print_coverage(false);
}

const std::set<std::string> &port_checker::skipped() const
{
    return m_skipped;
}

void port_checker::print_skipped() const
{
    std::cout << "# Skipped ports" << std::endl << std::endl;
    std::cout << "The following ports have been skipped" << std::endl;
    std::cout << "This usually means they have been disabled" << std::endl;
    for(const std::string& s : skipped())
        std::cout << "* " << s << std::endl;
    std::cout << std::endl;
}

void port_checker::print_statistics() const
{
    double time = finish_time - start_time;

    std::cout << "# Statistics" << std::endl << std::endl;
    std::cout << "Ports:" << std::endl;
    std::cout << "* checked (and not disabled): " << ports_checked << std::endl;
    std::cout << "* disabled: " << m_skipped.size() << std::endl
              << std::endl;

    std::cout << "Time (incl. IO wait):"  << std::endl;
    std::cout << "* total: " << time << "s" << std::endl;
    std::cout << std::setprecision(3)
              << "* per port (checked or disabled): "
              << (time / (ports_checked + m_skipped.size()))*1000.0 << "ms"
              << std::endl
              << std::endl;
}

const std::map<issue, issue_t> &port_checker::issue_types() const {
    return m_issue_types; }

const std::multimap<issue, std::string> &port_checker::issues() const {
    return m_issues; }

int port_checker::errors_found() const
{
    int errors = 0;
    for(const std::pair<issue, std::string>& p : m_issues)
        errors += (m_issue_types.at(p.first).sev == severity::error);
    return errors;
}

void port_checker::m_sanity_checks(std::vector<int>& issue_type_missing) const
{
    for(int i = 0; i < (int)issue::number; ++i)
    {
        bool found = false;
        for(const auto& it : _m_issue_types_arr)
        if((int)it.issue_id == i)
            found = true;
        if(!found)
            issue_type_missing.push_back(i);
    }
}

bool port_checker::sanity_checks() const
{
    std::vector<int> issue_type_missing;
    m_sanity_checks(issue_type_missing);
    return issue_type_missing.empty();
}

bool port_checker::print_sanity_checks() const
{
    std::cout << "# Sanity checks" << std::endl << std::endl;
    std::cout << "Checking port-checker itself for issues..." << std::endl;
    std::vector<int> issue_type_missing;
    m_sanity_checks(issue_type_missing);
    if(issue_type_missing.size())
    {
        std::cout << "* Issue types missing description:" << std::endl;
        for(int i : issue_type_missing)
            std::cout << "  - Enumerated issue with number " << i << std::endl;
    }
    else
        std::cout << "* All issue types have descriptions" << std::endl;

    std::cout << std::endl;
    return issue_type_missing.empty();
}

void port_checker::do_checks(char* loc, int loc_size, bool check_defaults)
{
    char* old_loc = loc + strlen(loc);
//  std::cout << "Checking Ports: \"" << loc << "\"..." << std::endl;


    rtosc_arg_val_t query_args[2];
    query_args[0].type = query_args[1].type = 's';
    query_args[0].val.s = loc;
    query_args[1].val.s = "";
    send_msg("/path-search", 2, query_args);
    std::vector<rtosc_arg_val_t> args;
    std::vector<char> strbuf;

    int res = sender.wait_for_reply(&strbuf, &args, "/paths");
    if(!res)
        throw std::runtime_error("no reply from path-search");
    if(args.size() % 2)
        throw std::runtime_error("bad reply from path-search");

    bool self_disabled = false; // disabled by an rSelf() ?
    size_t port_max = args.size() >> 1;
    // std::cout << "found " << port_max << " subports" << std::endl;
    for(size_t port_no = 0; port_no < port_max; ++port_no)
    {
        if(args[port_no << 1].type != 's' ||
                args[(port_no << 1) + 1].type != 'b')
            throw std::runtime_error("Invalid \"paths\" reply: bad types");

        if(!strncmp(args[port_no << 1].val.s, "self:", 5)) {
            rtosc_blob_t& blob = args[(port_no << 1) + 1].val.b;
            const char* metadata = (const char*)blob.data;
            if(!port_is_enabled(loc, args[port_no << 1].val.s, metadata))
                self_disabled = true;
        }
    }
    if(self_disabled)
        m_skipped.insert("/" + std::string(loc));
    else
        ++ports_checked;

    std::map<std::string, unsigned> port_count;

    if(!self_disabled)
    for(size_t port_no = 0; port_no < port_max; ++port_no)
    {
        const char* portname = args[port_no << 1].val.s;
        int portlen = strlen(portname);
        rtosc_blob_t& blob = args[(port_no << 1) + 1].val.b;
        const char* metadata = (const char*)blob.data;
        int32_t meta_len = blob.len;
        if(!metadata)
            metadata = "";
        bool has_subports = portname[portlen-1] == '/';
/*      std::cout << "port /" << loc << portname
                  << " (" << port_no << "/" << port_max
                  << "), has subports: " << std::boolalpha << has_subports
                  << std::endl;*/

        if(port_is_enabled(loc, portname, metadata))
        {
            Port::MetaContainer meta(metadata);

            if(meta.find("parameter") != meta.end())
            {
                std::string portname_noargs = portname;
                portname_noargs.resize(portname_noargs.find(':'));
                ++port_count[portname_noargs];
            }

            if(has_subports)
            {
                // statistics: port may still be disabled, see above
                if(loc_size > portlen)
                {
                    strcpy(old_loc, portname);
                    char* hashsign = strchr(old_loc, '#');
                    if(hashsign)
                    {
                        // #16\0 => 0\0
                        *hashsign = '0';
                        *++hashsign = '/';
                        *++hashsign = 0;
                    }

                    bool next_check_defaults =
                       (meta.find("no defaults") == meta.end());

                    do_checks(loc, loc_size - portlen, next_check_defaults);
                }
                else
                    throw std::runtime_error("portname too long");
            }
            else {
                ++ports_checked;
                check_port(loc, portname, metadata, meta_len, check_defaults);
            }
        }
        else
            m_skipped.insert("/" + std::string(loc) + portname);
        *old_loc = 0;
    }

    for(const auto& pr : port_count)
    {
        if(pr.second > 1)
            m_issues.emplace(issue::duplicate_parameter,
                             "/" + std::string(loc) + pr.first);
    }
}

bool port_checker::operator()(const char* url)
{
    for(const issue_t& it : _m_issue_types_arr)
        m_issue_types.emplace(it.issue_id, it);

    start_time = time(NULL);

    unsigned i = 0;
    for(; i < strlen(url); ++i)
        if(!isdigit(url[i]))
            break;

    if(i == strlen(url))
    {
        sendtourl = "osc.udp://127.0.0.1:";
        sendtourl += url;
        sendtourl += "/";
    }
    else
        sendtourl = url;

    sender.init(sendtourl.c_str());
    other.init(sendtourl.c_str());

    char loc_buffer[4096] = { 0 };
    do_checks(loc_buffer, sizeof(loc_buffer));

    finish_time = time(NULL);
    return true;
}

}

