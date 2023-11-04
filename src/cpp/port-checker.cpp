#include <cassert>
#include <vector>
#include <set>
#include <cstring>
#include <string>
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <iomanip>

#include "util.h"
#include <rtosc/ports.h>
#include <rtosc/pretty-format.h>
#include <rtosc/arg-ext.h>
#include <rtosc/arg-val.h>
#include <rtosc/arg-val-math.h>
#include <rtosc/port-checker.h>

// enable this if you need to find bugs in port-checker
// #define DEBUG_PORT_CHECKER

namespace rtosc {

static constexpr const issue_t _m_issue_types_arr[(int)issue::number] =
{
/*{
    issue::trailing_slash_without_subports,
    "ports with trailing slashes but no subports",
    "ports have trailing slashes, but no subports",
    severity::hint
},*/
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
    issue::enumeration_port_not_si,
    "enumerated port not \"S\" or \"i\"",
    "parameters using \"enumerated\" property do not accept \"i\" or \"S\"",
    severity::error
},{
    issue::roptions_port_not_si,
    "port with \"rOptions\" not \"S\" or \"i\"",
    "parameters using \"rOptions\" property do not accept \"i\" or \"S\"",
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
    issue::rpreset_without_rdefaultdepends,
    "rPreset without rDefaultDepends",
    "ports have rPreset(s), but no rDefaultDepends",
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

void port_checker::server::handle_recv(int argc, const char *types, const std::vector<const char*>* exp_strs)
{
    last_args->resize(argc);
    bool still_ok = true;
    for(std::size_t i = 0; i < (std::size_t)argc; ++i)
    {
        (*last_args)[i].val = rtosc_argument(last_buffer->data(), i);
        (*last_args)[i].type = types[i];
        if(still_ok && (*last_args)[i].type == 's' && i+1 < exp_strs->size())
        {
            still_ok = !strcmp((*last_args)[i].val.s, (*exp_strs)[i+1]);
        }
    }

    if(still_ok) { waiting = false; }
}

void port_checker::server::init(const char *target_url)
{
    vinit(target_url);

    rtosc_arg_val_t hi[3];
    hi[0].type = hi[1].type = 's';
    hi[0].val.s = hi[1].val.s = "";
    hi[2].type = 'T';
    hi[2].val.T = 1;
    send_msg("/path-search", 3, hi);
    std::vector<rtosc_arg_val_t> reply;
    std::vector<char> buf;
    bool ok = wait_for_reply(&buf, &reply, "/paths", hi[0].val.s, hi[1].val.s);
    if(!ok) {
        throw std::runtime_error("OSC app does not reply at all");
    }
}

bool port_checker::send_msg(const char* address,
                            size_t nargs, const rtosc_arg_val_t* args)
{
    return sender->send_msg(address, nargs, args);
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
        std::string enable_port = loc;
        enable_port += enable_port_rel;

        const char* collapsed_loc = Ports::collapsePath(&enable_port[0]);
        send_msg(collapsed_loc, 0, nullptr);

        std::vector<rtosc_arg_val_t> args;
        std::vector<char> strbuf;
        if(sender->wait_for_reply(&strbuf, &args, collapsed_loc)) {
            if(args.size() == 1 &&
               (args[0].type == 'T' || args[0].type == 'F' || args[0].type == 'i')) {
                rval = args[0].type == 'T' || (args[0].type == 'i' && args[0].val.i != 0);
            }
            else
                m_issues.emplace(issue::enabled_port_bad_reply,
                                 std::string(loc) + port);
        } else
            m_issues.emplace(issue::enabled_port_not_replied,
                             std::string(loc) + port);
    }
    //std::cout << "port \"" << loc << "\" enabled? "
    //          << (rval ? "yes" : "no") << std::endl;
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
        case 'b': /* we don't know how to proper alternate the buffer */ break;
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
    std::string full_path = loc; full_path += portname;
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

        bool do_checks = true;
        rtosc::Port::MetaContainer meta(metadata);
        if(meta_len && metadata && *metadata)
        {
            bool is_parameter = false,
                 is_enumerated = false,
                 default_depends = false;
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
                else if(!strcmp(x.title, "no port checker"))
                    do_checks = false;
                else if(!strcmp(x.title, "default")) {
                    // x.value;
                    ++n_default_vals;
                    default_values.push_back(x.value);
                }
                else if(!strcmp(x.title, "default depends")) {
                    default_depends = true;
                }
                else if(!strncmp(x.title, "default ", strlen("default "))) {
                    ++presets[x.title + strlen("default ")];
                    default_values.push_back(x.value);
                }
                else if(!strncmp(x.title, "map ", 4)) {
                    ++mappings[atoi(x.title + 4)];
                    ++mapping_values[x.value];
                }
            }

            auto raise = [this, loc, portname](issue issue_type) {
                m_issues.emplace(issue_type, std::string(loc) + portname);
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
                    STACKALLOC(rtosc_arg_val_t, avs, nargs);
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
                                    ptr - (avs+1) < rtosc_av_arr_len(avs);
                                    ptr += incsize, arrsize += cur)
                                {
                                    switch(ptr->type)
                                    {
                                        case '-':
                                            cur = rtosc_av_rep_num(ptr);
                                            incsize = 2 + rtosc_av_rep_has_delta(ptr);
                                            if(!cur) infinite = true;
                                            break;
                                        case 'a':
                                            cur = 1;
                                            incsize = rtosc_av_arr_len(ptr);
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
            if(do_checks)
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

                if(presets.size() && !default_depends)
                {
                    raise(issue::rpreset_without_rdefaultdepends);
                }

                if(!strstr(portname, ":i") || !strstr(portname, ":S"))
                {
                    if(is_enumerated)
                        raise(issue::enumeration_port_not_si);
                    if(mappings.size())
                        raise(issue::roptions_port_not_si);
                }

                // send and reply...
                // first, get some useful values
                send_msg(full_path.c_str(), 0, nullptr);
                std::vector<rtosc_arg_val_t> args1;
                std::vector<char> strbuf1;
                int res = sender->wait_for_reply(&strbuf1, &args1,
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
                    res = sender->wait_for_reply(&strbuf1, &args1,
                                                 full_path.c_str(), nullptr,
                                                 "/undo_change", nullptr);

                    if(!res)
                        raise(issue::parameter_not_replied);
                    else {
                        if(sender->replied_path() == 1 /* i.e. undo_change */) {
                            // some apps may reply with undo_change, some may
                            // already catch those... if we get one: retry
                            res = sender->wait_for_reply(&strbuf1, &args1,
                                                         full_path.c_str());
                        }

                        if(res)
                        {
                            res = other->wait_for_reply(&strbuf1, &args1,
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
    for(const std::pair<const issue, std::string>& p : m_issues)
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
    for(const std::pair<const issue, std::string>& p : m_issues)
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
#ifdef DEBUG_PORT_CHECKER
    std::cout << "Checking Ports: \"" << loc << "\"..." << std::endl;
#endif

    rtosc_arg_val_t query_args[3];
    query_args[0].type = query_args[1].type = 's';
    query_args[0].val.s = loc;
    query_args[1].val.s = "";
    query_args[2].type = 'T';
    query_args[2].val.T = 1;
    send_msg("/path-search", 3, query_args);
    std::vector<rtosc_arg_val_t> args;
    std::vector<char> strbuf;

    int res = sender->wait_for_reply(&strbuf, &args, "/paths",
                                     query_args[0].val.s, query_args[1].val.s);
    if(!res)
        throw port_error("no reply from path-search", loc);
    if(args.size() % 2)
        throw port_error("bad reply from path-search", loc);

    bool self_disabled = false; // disabled by an rSelf() ?
    size_t port_max = args.size() >> 1;
#ifdef DEBUG_PORT_CHECKER
    std::cout << "found " << port_max-1 << " subports" << std::endl;
#endif
    for(size_t port_no = 1; port_no < port_max; ++port_no)
    {
        if(args[port_no << 1].type != 's' ||
                args[(port_no << 1) + 1].type != 'b')
            throw port_error("Invalid \"paths\" reply: bad types", loc);

        if(!strncmp(args[port_no << 1].val.s, "self:", 5)) {
            rtosc_blob_t& blob = args[(port_no << 1) + 1].val.b;
            const char* metadata = (const char*)blob.data;
            if(!port_is_enabled(loc, args[port_no << 1].val.s, metadata))
                self_disabled = true;
        }
    }
    if(self_disabled)
        m_skipped.insert(loc);
    else
        ++ports_checked;

    std::map<std::string, unsigned> port_count;

    if(!self_disabled)
    for(size_t port_no = 1; port_no < port_max; ++port_no)
    {
        const char* portname = args[port_no << 1].val.s;
        const int portlen = strlen(portname);
        const rtosc_blob_t& blob = args[(port_no << 1) + 1].val.b;
        const char* metadata = (const char*)blob.data;
        const int32_t meta_len = blob.len;
        const bool has_meta = meta_len && metadata && *metadata;
        bool has_subports = portname[portlen-1] == '/';
        if(!has_meta)
           metadata = "";
#ifdef DEBUG_PORT_CHECKER
        std::cout << "port " << loc << ", replied: " << portname
                  << " (" << port_no << "/" << (port_max-1)
                  << "), has subports: " << std::boolalpha << has_subports
                  << std::endl;
#endif
        if(!has_meta || port_is_enabled(loc, portname, metadata))
        {
            Port::MetaContainer meta(metadata);

            if(has_meta && meta.find("parameter") != meta.end())
            {
                std::string portname_noargs = portname;
                portname_noargs.resize(portname_noargs.find(':'));
                // count portname to detect duplicate parameter ports
                ++port_count[portname_noargs];
            }

            if(rtosc_match_path(portname, loc, nullptr))
            {
                // the returned path has equal length
                // -> it's a single port without subports
                if(has_subports)
                {
/*                  m_issues.emplace(issue::trailing_slash_without_subports,
                                     loc);*/
                    has_subports = false;
                }
            }
            else
            {
                if(has_subports)
                {
                    // statistics: port may still be disabled, see above
                    if(loc_size > portlen)
                    {
                        // TODO: it could still be a port without subports tht
                        //       did not pass rtosc_match_path
                        // it can not come from the root now,
                        // so it's relative to the current port
                        strcpy(old_loc, portname);
                        // TODO: the portname could be relative (i.e. rtosc_match_path
                        //       failed), but relative to a parent port
                        //       => only copy last part ("basename") of portname?
                    }
                    else
                        throw port_error("portname too long", loc);
                }
            }

            if(has_subports)
            {
                // statistics: port may still be disabled, see above
                if(loc_size > portlen)
                {
                    char* hashsign = strchr(old_loc, '#');
                    if(hashsign)
                    {
                        // #16\0 => 0\0
                        *hashsign = '0';
                        *++hashsign = '/';
                        *++hashsign = 0;
                    }

                    bool next_check_defaults =
                       !has_meta || (meta.find("no defaults") == meta.end());
                    do_checks(loc, loc_size - portlen, next_check_defaults);
                }
            }
            else {
                ++ports_checked;
                check_port(loc, portname, metadata, meta_len, check_defaults);
            }
        }
        else
            m_skipped.insert(std::string(loc) + portname);
        *old_loc = 0;
    }

    for(const auto& pr : port_count)
    {
        if(pr.second > 1)
            m_issues.emplace(issue::duplicate_parameter,
                             std::string(loc) + pr.first);
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

    sender->init(sendtourl.c_str());
    other->init(sendtourl.c_str());

    char loc_buffer[4096] = { '/', 0 };
    do_checks(loc_buffer, sizeof(loc_buffer));

    finish_time = time(NULL);
    return true;
}

port_error::port_error(const char *errmsg, const char *port)
    : runtime_error(errmsg)
{
    strncpy(m_port, port, sizeof(m_port) - 1);
    m_port[sizeof(m_port) - 1] = 0;
}

}

