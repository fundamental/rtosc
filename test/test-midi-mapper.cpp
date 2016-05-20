#include <rtosc/miditable.h>
#include <rtosc/port-sugar.h>
#include "common.h"

struct Dummy {
    char foo;
    char bar;
};

#define rObject Dummy
rtosc::Ports p = {
    rParam(foo),
    rParam(bar),
};


rtosc::MidiMapperStorage *storage = NULL;

void non_rt_to_rt(const char *msg)
{
    delete storage;
    storage = *(decltype(storage)*)rtosc_argument(msg, 0).b.data;
};
void rt_to_non_rt(const char *){};
void rt_to_rt(const char *) {};

int main()
{
    rtosc::MidiMapperRT rt;
    rtosc::MidiMappernRT non_rt;

    non_rt.rt_cb = non_rt_to_rt;
    rt.setFrontendCb(rt_to_non_rt);
    rt.setBackendCb(rt_to_rt);

    non_rt.addNewMapper(0, p.ports[0], "/foo");
    non_rt.addNewMapper(1, p.ports[1], "/bar");
    auto &map = non_rt.inv_map;

    assert_int_eq(1, map.count("/bar"),
            "Mapping for 'bar' is learned", __LINE__);
    assert_int_eq(1, map.count("/foo"),
                    "Mapping for 'foo' is learned", __LINE__);

    non_rt.clear();
    assert_int_eq(0, non_rt.inv_map.size(), "Mapping can be cleared", __LINE__);

    delete storage;
    return test_summary();
}
