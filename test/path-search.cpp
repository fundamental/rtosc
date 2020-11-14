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
    {"b/x/", 0,        0, null_fn}
};

#define rObject walk_ports_tester
static const rtosc::Ports ports = {
    {"a/", 0, &a_ports, null_fn},
//  {"a/b/c/", 0, &c_ports, null_fn},
};
#undef rObject

static const rtosc::Ports multiple_ports = {
    {"c/d/e:", 0, 0, null_fn},
    {"a/x:", 0, 0, null_fn},
    {"a/y:", 0, 0, null_fn},
    {"c/d/", 0, 0, null_fn},
    {"a/", 0, 0, null_fn},
    {"b", 0, 0, null_fn},
    {"b2", 0, 0, null_fn},
};

int main()
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    std::size_t max_ports = 255;
    size_t max_args    = max_ports << 1;
    size_t max_types   = max_args + 1;
    std::vector<char> types(max_types);
    std::vector<rtosc_arg_t> args(max_types);

    assert_ptr_eq(ports.apropos("/a/b/c/d/e"),
                  &ports.ports[0].ports->ports[0].ports->ports[0].ports->ports[0],
                  "apropos(\"/a/b/c/d/e\")", __LINE__);

    assert_ptr_eq(ports.apropos("/a/b/c/d"),
                  &ports.ports[0].ports->ports[0].ports->ports[0],
                  "apropos(\"/a/b/c/d\")", __LINE__);

    assert_ptr_eq(ports.apropos("/a/b/c"),
                  &ports.ports[0].ports->ports[0],
                  "apropos(\"/a/b/c\")", __LINE__);

    assert_ptr_eq(ports.apropos("/a/b"),
                  &ports.ports[0].ports->ports[0], // ???
                  "apropos(\"/a/b\")", __LINE__);

    assert_ptr_eq(ports.apropos("/a"),
                  &ports.ports[0],
                  "apropos(\"/a\")", __LINE__);

    assert_ptr_eq(ports.apropos("/a"),
                  &ports.ports[0],
                  "apropos(\"/a\")", __LINE__);

    assert_ptr_eq(ports.apropos(""),
                  nullptr,
                  "apropos(\"\")", __LINE__);

    assert_ptr_eq(ports.apropos("doesnt-exist"),
                  nullptr,
                  "apropos(\"doesnt-exist\")", __LINE__);

    // all children at "/a/b/c/d/e"? no children - return e directly
    path_search(ports, "/a/b/c/d/e", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sb", types.data(), "/a/b/c/d/e - types", __LINE__);
    assert_str_eq("e", args[0].s, "/a/b/c/d/e - ports", __LINE__);

    // all children at "/a/b/c/d"? return e
    path_search(ports, "/a/b/c/d", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sb", types.data(), "/a/b/c/d - types", __LINE__);
    assert_str_eq("e", args[0].s, "/a/b/c/d - ports", __LINE__);

    path_search(ports, "/a/b/c", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sb", types.data(), "/a/b/c - types", __LINE__);
    assert_str_eq("d/", args[0].s, "/a/b/c - ports", __LINE__);

    path_search(ports, "/a/b", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sb", types.data(), "/a/b - types", __LINE__);
    assert_str_eq("d/", args[0].s, "/a/b - ports", __LINE__); // ???

    path_search(ports, "/a", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sbsb", types.data(), "/a - types", __LINE__);
    assert_str_eq("b/c/", args[0].s, "/a - ports 1", __LINE__);
    assert_str_eq("b/x/", args[2].s, "/a - ports 2", __LINE__);

    path_search(ports, "", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sb", types.data(), "\"\" - types", __LINE__);
    assert_str_eq("a/", args[0].s, "\"\" - ports", __LINE__);

    path_search(ports, "/doesnt-exist", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("", types.data(), "\"/doesnt-exist\" - types", __LINE__);

    // multiple ports

    path_search(multiple_ports, "", "", types.data(), max_types, args.data(), max_args,
                rtosc::path_search_opts::unmodified);
    assert_str_eq("sbsbsbsbsbsbsb", types.data(), "multiple ports, unmodified - types", __LINE__);
    assert_str_eq("c/d/e:", args[0].s, "multiple ports, sorted - ports 1", __LINE__);

    path_search(multiple_ports, "", "", types.data(), max_types, args.data(), max_args,
                rtosc::path_search_opts::sorted);
    assert_str_eq("sbsbsbsbsbsbsb", types.data(), "multiple ports, sorted - types", __LINE__);
    assert_str_eq("a/", args[0].s, "multiple ports, sorted - ports 1", __LINE__);
    assert_str_eq("a/x:", args[2].s, "multiple ports, sorted - ports 2", __LINE__);
    assert_str_eq("a/y:", args[4].s, "multiple ports, sorted - ports 3", __LINE__);
    assert_str_eq("b", args[6].s, "multiple ports, sorted - ports 4", __LINE__);
    assert_str_eq("b2", args[8].s, "multiple ports, sorted - ports 5", __LINE__);
    assert_str_eq("c/d/", args[10].s, "multiple ports, sorted - ports 6", __LINE__);
    assert_str_eq("c/d/e:", args[12].s, "multiple ports, sorted - ports 7", __LINE__);

    path_search(multiple_ports, "", "", types.data(), max_types, args.data(), max_args);
    assert_str_eq("sbsbsbsb", types.data(), "multiple ports, unique prefix - types", __LINE__);
    assert_str_eq("a/", args[0].s, "multiple ports, unique prefix - ports 1", __LINE__);
    assert_str_eq("b", args[2].s, "multiple ports, unique prefix - ports 2", __LINE__);
    assert_str_eq("b2", args[4].s, "multiple ports, unique prefix - ports 3", __LINE__);
    assert_str_eq("c/d/", args[6].s, "multiple ports, unique prefix - ports 4", __LINE__);

    return test_summary();
}

