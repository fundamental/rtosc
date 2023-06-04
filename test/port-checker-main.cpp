#include <cstdlib>
#include <cstring>
#include <iostream>
#include <getopt.h>

#include <rtosc/port-checker.h>

void usage(const char* progname)
{
    std::cout << "Usage: " << progname << " [options] <URL>" << std::endl;
    std::cout << "Tests ports of the app running on <URL>, if the app"
              << std::endl
              << "has a \"path-search\" port conforming to rtosc::path_search()"
              << std::endl
              << "The URL must be in format osc.udp://xxx.xxx.xxx.xxx:ppppp/,"
              << std::endl
              << "or just ppppp (which means osc.udp://127.0.0.1:ppppp/)"
              << std::endl << std::endl;
    std::cout << "Options: --timeout <time in msecs>" << std::endl;
}

struct port_checker_options
{
    int timeout_msecs = 50;
};

int run_port_checker(const char* url,
                     const port_checker_options& checker_opts)
{
    int rval = EXIT_SUCCESS;
    try {
        rtosc::liblo_server sender(checker_opts.timeout_msecs), other(checker_opts.timeout_msecs);
        rtosc::port_checker checker(&sender, &other);
        checker(url);

        if(!checker.print_sanity_checks())
            rval = EXIT_FAILURE;
        checker.print_evaluation();
        if(checker.errors_found())
            rval = EXIT_FAILURE;
        checker.print_not_affected();
        checker.print_skipped();
        checker.print_statistics();
    }
    catch(const rtosc::port_error& e) {
        std::cout << "**Error caught**: port \"" << e.m_port << "\": "
                  << e.what() << std::endl;
        rval = EXIT_FAILURE;
    }
    catch(const std::exception& e) {
        std::cout << "**Error caught**: " << e.what() << std::endl;
        rval = EXIT_FAILURE;
    }

    std::cout << "# Port checker test summary" << std::endl << std::endl;
    std::cout << (rval == EXIT_SUCCESS ? "**SUCCESS!**" : "**FAILURE!**")
              << std::endl;
    std::cout << std::endl;

    return rval;
}

int main(int argc, char** argv)
{
    int rval = EXIT_SUCCESS;
    const char* errmsg = nullptr;
    bool exit_with_help = false;
    auto mk_err = [&errmsg, &rval](const char* msg) {
        errmsg = msg; rval = EXIT_FAILURE; };

    // getopt stuff
    struct option opts[] = {
        // options with single char equivalents
        { "help", 0, nullptr, 'h' },
        { "timeout", 1, nullptr, 't' },
        { nullptr, 0, nullptr, 0 }
    };
    opterr = 0;
    int option_index = 0, opt;

    // port checker args
    port_checker_options checker_opts;
    const char* url;

    // parse options
    while(1)
    {
        opt = getopt_long(argc, argv, "ht:", opts, &option_index);
        if(opt == -1)
            break;

        switch(opt)
        {
            case 'h':
                exit_with_help = true;
                break;
            case 't':
                checker_opts.timeout_msecs = atoi(optarg);
                if(!checker_opts.timeout_msecs) {
                    mk_err("Timeout must be a parameter >0");
                }
                break;
            case '?':
                mk_err("Bad option or parameter (use --help)");
        }
    }

    if(!exit_with_help && rval == EXIT_SUCCESS)
    {
        if (optind == argc - 1) { url = argv[optind]; }
        else mk_err("expecting exactly 1 non-option argument - the URL");
    }

    if(exit_with_help || rval == EXIT_FAILURE)
    {
        if(errmsg) std::cerr << "ERROR: " << errmsg << std::endl << std::endl;
        usage(*argv);
    }
    else rval = run_port_checker(url, checker_opts);

    return rval;
}

