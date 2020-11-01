#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
using namespace rtosc;

#include "common.h"

void null_fn(const char*,RtData){}

Ports subtree1 = {
    {"port", "", 0, null_fn}
};

Ports ports = {
    {"setstring:s",     "", 0,         null_fn},
    {"setint:i",        "", 0,         null_fn},
    {"subtree1/",       "", &subtree1, null_fn},
    {"subtree2/x/",     "", &subtree1, null_fn},
    {"no/subtree/",     "", 0,         null_fn},
    {"echo:ss",         "", 0,         null_fn},
};

int main()
{
    const Port *p;
    p = ports.apropos("/setstring");
    assert_ptr_eq(p, &ports.ports[0], "no subtree", __LINE__);
    p = ports.apropos("/subtree1/port");
    assert_ptr_eq(p, &ports.ports[2].ports->ports[0], "normal subtree", __LINE__);
    p = ports.apropos("/subtree2/x/port");
    assert_ptr_eq(p, &ports.ports[3].ports->ports[0], "subtree with 2 slashes", __LINE__);
    // this is not good style, but it still shall not cause memory errors:
    p = ports.apropos("/no/subtree/");
    assert_ptr_eq(p, &ports.ports[4], "trailing slash without subtree", __LINE__);

    return test_summary();
}
