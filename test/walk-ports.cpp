#include <set>
#include <string>
#include <iostream>

#include "common.h"

#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>

void null_fn(const char*,rtosc::RtData){}

static const rtosc::Ports d_ports = {
    {"e", 0, 0, null_fn},
};

static const rtosc::Ports c_ports = {
    {"d/", 0, &d_ports, null_fn},
};

static const rtosc::Ports a_ports = {
    {"b/c/", 0, &c_ports, null_fn},
};

#define rObject walk_ports_tester
static const rtosc::Ports ports = {
    {"a/", 0, &a_ports, null_fn},
    {"a/b/c/", 0, &c_ports, null_fn},
};
#undef rObject

static const rtosc::Ports numeric_ports = {
    {"a#3/b#2/c", 0, 0, null_fn},
};

void append_str(const rtosc::Port*, const char *name, const char*,
                const rtosc::Ports&, void *resVoid, void*)
{
    std::string* res = (std::string*)resVoid;
    *res += name;
    *res += ";";
}

int main()
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    std::string res;
    rtosc::walk_ports(&ports, buffer, 1024, &res, append_str);
    assert_str_eq("/a/b/c/d/e;/a/b/c/d/e;", res.c_str(), "walk_ports from root", __LINE__);
    res.clear();
    rtosc::walk_ports(&numeric_ports, buffer, 1024, &res, append_str);
    printf("str: %s\n",res.c_str());
    // yet wrong, but at least half way right:
    assert_str_eq("/a0/b#2/c;/a1/b#2/c;/a2/b#2/c;", res.c_str(), "walk_ports from root", __LINE__);
    return test_summary();
}

