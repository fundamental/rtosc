#include <cstdlib>
#include <cstring>
#include <iostream>

#include "common.h"
#include "port-checker.h"

//! @file port-checker-tester.cpp
//! This tester tests port checker
//! It's being connected to port-checker-testapp

using issue = rtosc::issue;

std::multimap<issue, std::string> get_exp()
{
    // test expectations
    // note: if you add new ports, please try to only let each new port
    //       occur in at most one category

    std::multimap<issue, std::string> exp;

    exp.emplace(issue::duplicate_parameter, "/duplicate_param");

    exp.emplace(issue::parameter_not_queryable, "/no_query::i");
    exp.emplace(issue::parameter_not_replied, "/no_reply_A::i");
    exp.emplace(issue::parameter_not_replied, "/no_reply_B::i");
    exp.emplace(issue::parameter_not_broadcast, "/no_broadcast::i");

    exp.emplace(issue::option_port_not_si, "/roption_without_ics::i:c");
    exp.emplace(issue::duplicate_mapping, "/duplicate_mapping::i:S");
    exp.emplace(issue::enabled_port_not_replied,
                "/enabled_port_not_existing::i");
    exp.emplace(issue::enabled_port_bad_reply, "/enabled_port_bad_reply::i");

    exp.emplace(issue::rdefault_missing, "/no_rdefault::i");
    exp.emplace(issue::rdefault_multiple, "/double_rdefault::i");
    exp.emplace(issue::rpreset_multiple, "/double_rpreset::i");
    exp.emplace(issue::rdefault_without_rparameter,
                "/rdefault_without_rparameter_A::i");
    exp.emplace(issue::rdefault_without_rparameter,
                "/rdefault_without_rparameter_B::i");
    exp.emplace(issue::invalid_default_format, "/invalid_rdefault::i");
    exp.emplace(issue::bundle_size_not_matching_rdefault,
                "/bundle_size#16::i");
    exp.emplace(issue::rdefault_not_infinite, "/bundle_size#16::i");
    exp.emplace(issue::default_cannot_canonicalize,
                "/rpreset_not_in_roptions::i:c:S");

    return exp;
}


void usage(const char* progname)
{
    std::cout << "usage: " << progname << " <URL>" << std::endl;
    std::cout << "Starts a port checker and tests it"
              << std::endl
              << "Must be used together with port-checker-testapp"
              << std::endl
              << "Don't start directly, use test-port-checker.rb"
              << std::endl;
}

int main(int argc, char** argv)
{
    if(argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(*argv);
        return EXIT_SUCCESS;
    }

    bool exceptions_thrown = false;

    try {
        rtosc::port_checker checker;
        checker(argv[1]);

        assert_true(checker.sanity_checks(), "Port checker sanity", __LINE__);
        // we keep it clean, but if you ever need help:
        // checker.print_evaluation();
        assert_true(checker.coverage(), "All issues have test ports", __LINE__);

        std::multimap<issue, std::string> exp = get_exp(); // see above
        const std::multimap<issue, std::string>& res = checker.issues();

        assert_int_eq(exp.size(), res.size(),
                      "Expected number of issues is correct", __LINE__);
        assert_true(exp == res,
                    "Issues are as expected", __LINE__);

        std::set<std::string> exp_skipped;
        exp_skipped.insert("/invisible_param::i");
        assert_true(exp_skipped == checker.skipped(), "Skipped port are as"
                                                      "expected", __LINE__);
    }
    catch(const std::exception& e) {
        // std::cout << "**Error caught**: " << e.what() << std::endl;
        exceptions_thrown = true;
    }

    assert_true(!exceptions_thrown, "No exceptions thrown", __LINE__);

    return test_summary();
}

