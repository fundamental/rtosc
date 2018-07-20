#include <cstdlib>
#include <cstring>
#include <iostream>

#include "port-checker.h"

void usage(const char* progname)
{
    std::cout << "usage: " << progname << " <URL>" << std::endl;
    std::cout << "Tests ports of the app running on <URL>, if the app"
              << std::endl
              << "has a \"path-search\" port conforming to rtosc::path_search()"
              << std::endl
              << "The URL must be in format osc.udp://xxx.xxx.xxx.xxx:ppppp/,"
              << std::endl
              << "or just ppppp (which means osc.udp://127.0.0.1:ppppp/)"
              << std::endl;
}

int main(int argc, char** argv)
{
    if(argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(*argv);
        return EXIT_SUCCESS;
    }

    int rval = EXIT_SUCCESS;

    try {
        rtosc::port_checker checker;
        checker(argv[1]);

        if(!checker.print_sanity_checks())
            rval = EXIT_FAILURE;
        checker.print_evaluation();
        if(checker.errors_found())
            rval = EXIT_FAILURE;
        checker.print_not_affected();
        checker.print_skipped();
        checker.print_statistics();
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

