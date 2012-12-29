#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
#include <string>
#include <iostream>
using namespace rtosc;

std::string resultA;
int resultB = 0;

void null_fn(const char*,void*){}

Ports<1,void*> subtree{{{
    Port<void*>("port", "", null_fn)
}}};

Ports<4,void*> ports{{{
    Port<void*>("setstring:s", "",       null_fn),
    Port<void*>("setint:i",    "",       null_fn),
    Port<void*>("subtree/",    &subtree, null_fn),
    Port<void*>("echo:ss",     "",       null_fn),
}}};

int main()
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    walk_ports(&ports, 
            buffer, 1024,
            [](const mPort*, const char *name) {
                puts(name);
            });

    return 0;
}
