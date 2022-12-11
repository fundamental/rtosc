#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
using namespace rtosc;

#include "common.h"

void null_fn(const char*,RtData){}

Ports subtree1 = {
    {"port", "", 0, null_fn}
};

Ports ports = {
    {"setstring:s",            "", 0,         null_fn},
    {"setint:i",               "", 0,         null_fn},
    {"subtree1/",              "", &subtree1, null_fn},
    {"subtree2/x/",            "", &subtree1, null_fn},
    {"no/subtree/",            "", 0,         null_fn},
    {"VoicePar#8/",            "", &subtree1, null_fn},
    {"VoicePar#8/Enabled:T:F", "", 0,         null_fn},
    {"echo:ss",                "", 0,         null_fn},
};

int main()
{
    const Port *p;
    p = ports.apropos("/setstring");
    assert_ptr_eq(&ports.ports[0], p, "no subtree", __LINE__);
    p = ports.apropos("/subtree1/port");
    assert_ptr_eq(&ports.ports[2].ports->ports[0], p, "normal subtree", __LINE__);
    p = ports.apropos("/subtree2/x/port");
    assert_ptr_eq(&ports.ports[3].ports->ports[0], p, "subtree with 2 slashes", __LINE__);
    // this is not good style, but it still shall not cause memory errors:
    p = ports.apropos("/no/subtree/");
    assert_ptr_eq(&ports.ports[4], p, "trailing slash without subtree", __LINE__);
    p = ports.apropos("/VoicePar1/Enabled");
    // this is a bug: it should match "&ports.ports[6]", but currently it fails,
    // because it dives into ports.ports[5] and then aborts
    assert_ptr_eq(nullptr, p, "array", __LINE__);

    return test_summary();
}
