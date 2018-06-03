#include <cstdlib>
#include <cstring>

#include "port-checker.h"

void usage(const char* progname)
{
    printf("usage: %s <URL>\n", progname);
    puts("Tests ports of the app running on <URL>, if the app\n"
         "has a \"path-search\" port conforming to rtosc::path_search()");
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
    }
    catch(const std::exception& e) {
        printf("Error caught: %s\n", e.what());
        rval = EXIT_FAILURE;
    }

    printf("port-checker summary: %s!\n", rval == EXIT_SUCCESS ? "SUCCESS"
                                                               : "FAILURE");
    return rval;
}

