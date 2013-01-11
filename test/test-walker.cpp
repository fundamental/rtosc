#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
#include <string>
#include <iostream>
using namespace rtosc;

std::string resultA;
int resultB = 0;

void null_fn(const char*,void*){}

Ports subtree = {
    {"port", "", 0, null_fn}
};

Ports ports = {
    {"setstring:s", "", 0,        null_fn},
    {"setint:i",    "", 0,        null_fn},
    {"subtree/",    "", &subtree, null_fn},
    {"echo:ss",     "", 0,        null_fn},
};

int main()
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    walk_ports(&ports, 
            buffer, 1024,
            [](const Port*, const char *name) {
            puts(name);
            });

    return 0;
}
