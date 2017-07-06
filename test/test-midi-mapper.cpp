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
    printf("#   Message to realtime '%s'\n", msg);
    if(strcmp("/midi-learn/midi-bind", msg) == 0) {
        delete storage;
        storage = *(decltype(storage)*)rtosc_argument(msg, 0).b.data;
    }
};

void rt_to_non_rt(const char *msg) {
    printf("#   Message to non-realtime '%s'\n", msg);
};
void rt_to_rt(const char *msg) {
    printf("#   Message to realtime '%s'\n", msg);
};

void test_basic(void)
{
    printf("#Test Basic\n");
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
    storage = NULL;
}

void test_relearn(void)
{
    printf("#Test Relearn\n");
    rtosc::MidiMapperRT rt;
    rtosc::MidiMappernRT non_rt;

    non_rt.base_ports = &p;
    non_rt.rt_cb = non_rt_to_rt;
    rt.setFrontendCb(rt_to_non_rt);
    rt.setBackendCb(rt_to_rt);

    assert_int_eq(0, non_rt.getMidiMappingStrings().size(),
            "No mappings are available on initialization", __LINE__);

    printf("#\n#Setup watch\n");
    non_rt.map("/foo", true);
    rt.addWatch();
    printf("#\n#Learn binding\n");
    rt.handleCC(5, 2);
    non_rt.useFreeID(5);
    assert_int_eq(1, non_rt.getMidiMappingStrings().size(),
            "One mapping is available after learning", __LINE__);

    printf("#\n#unlearn binding\n");
    non_rt.unMap("/foo", false);
    non_rt.unMap("/foo", true);
    assert_int_eq(0, non_rt.getMidiMappingStrings().size(),
            "No mappings are available after unlearning", __LINE__);

    //printf("#\n#Setup watch2\n");
    //non_rt.map("/foo", true);
    //rt.addWatch();
    //printf("#\n#Relearn binding\n");
    //rt.handleCC(5, 2);
    //non_rt.useFreeID(5);
    delete storage;
    storage = NULL;
    return;
}

int main()
{
    test_basic();
    test_relearn();
    return test_summary();
}
