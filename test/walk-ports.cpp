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

static const rtosc::Ports ports = {
    {"a/", 0, &a_ports, null_fn},
    {"a/b/c/", 0, &c_ports, null_fn},
};

static const rtosc::Ports numeric_ports = {
    {"a#3/b#2/c", 0, &d_ports, null_fn},
};

static const rtosc::Ports numeric_ports_integer = {
    {"a#3/b#2/c/::i", 0, &d_ports, null_fn},
};

static const rtosc::Ports numeric_port = {
    {"a#3/b#2/c::i", 0, 0, null_fn},
};

static const rtosc::Ports multiple_ports = {
    {"c/d/e:", 0, 0, null_fn},
    {"a/x:", 0, 0, null_fn},
    {"c/d:", 0, 0, null_fn},
    {"a:", 0, 0, null_fn},
    {"b:", 0, 0, null_fn},
};

void append_str(const rtosc::Port*, const char *name, const char*,
                const rtosc::Ports&, void *resVoid, void*)
{
    std::string* res = (std::string*)resVoid;
    *res += name;
    *res += ";";
}

void check_all_subports(const rtosc::Ports& root, const char* exp,
                        const char* testcase, int line)
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    std::string res;
    rtosc::walk_ports(&root, buffer, 1024, &res, append_str);
    assert_str_eq(exp, res.c_str(), testcase, line);
}

int main()
{
    check_all_subports(ports, "/a/b/c/d/e;/a/b/c/d/e;",
                       "walk_ports with \"/a/b/c\"", __LINE__);

    const char* numeric_exp = "/a0/b0/c/e;/a0/b1/c/e;"
                              "/a1/b0/c/e;/a1/b1/c/e;"
                              "/a2/b0/c/e;/a2/b1/c/e;";
    check_all_subports(numeric_ports, numeric_exp,
                       "walk_ports with \"a#3/b#2/c\"", __LINE__);

    check_all_subports(numeric_ports_integer, numeric_exp,
                       "walk_ports with \"a#3/b#2/c/::i\"", __LINE__);

#if 0
    check_all_subports(numeric_port, numeric_exp,
                       "walk_ports with \"a#3/b#2/c::i\"", __LINE__);
    // still failing: bundle-foreach.h needs a recursion like walk_ports
    // has it for subports with multiple hashes
#endif

    check_all_subports(multiple_ports, "a;b;c/d",
                       "walk_ports with multiple common prefixes", __LINE__);

    return test_summary();
}

