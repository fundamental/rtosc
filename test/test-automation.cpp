#include <rtosc/ports.h>
#include <rtosc/automations.h>
#include <rtosc/port-sugar.h>
#include "common.h"

struct Dummy {
    float foo;
    float bar;
};

#define rObject Dummy
rtosc::Ports p = {
    rParamF(foo, rLinear(-1,10), "no documentation"),
    rParamF(bar, rLinear(0, 100.2), "no doc"),
};

void suite(const char *s)
{ 
    printf("\n\n#SUITE: %s\n", s);
}


//rtosc::MidiMapperStorage *storage = NULL;
//
//void non_rt_to_rt(const char *msg)
//{
//    printf("#   Message to realtime '%s'\n", msg);
//    if(strcmp("/midi-learn/midi-bind", msg) == 0) {
//        delete storage;
//        storage = *(decltype(storage)*)rtosc_argument(msg, 0).b.data;
//    }
//};
//
//void rt_to_non_rt(const char *msg) {
//    printf("#   Message to non-realtime '%s'\n", msg);
//};
//void rt_to_rt(const char *msg) {
//    printf("#   Message to realtime '%s'\n", msg);
//};

void test_basic(void)
{
    printf("#Test Basic\n");
//    rtosc::MidiMapperRT rt;
//    rtosc::MidiMappernRT non_rt;
//
//    non_rt.rt_cb = non_rt_to_rt;
//    rt.setFrontendCb(rt_to_non_rt);
//    rt.setBackendCb(rt_to_rt);
//
//    non_rt.addNewMapper(0, p.ports[0], "/foo");
//    non_rt.addNewMapper(1, p.ports[1], "/bar");
//    auto &map = non_rt.inv_map;
//
//    assert_int_eq(1, map.count("/bar"),
//            "Mapping for 'bar' is learned", __LINE__);
//    assert_int_eq(1, map.count("/foo"),
//                    "Mapping for 'foo' is learned", __LINE__);
//
//    non_rt.clear();
//    assert_int_eq(0, non_rt.inv_map.size(), "Mapping can be cleared", __LINE__);
//
//    delete storage;
//    storage = NULL;
}

void test_relearn(void)
{
    printf("#Test Relearn\n");
//    rtosc::MidiMapperRT rt;
//    rtosc::MidiMappernRT non_rt;
//
//    non_rt.base_ports = &p;
//    non_rt.rt_cb = non_rt_to_rt;
//    rt.setFrontendCb(rt_to_non_rt);
//    rt.setBackendCb(rt_to_rt);
//
//    assert_int_eq(0, non_rt.getMidiMappingStrings().size(),
//            "No mappings are available on initialization", __LINE__);
//
//    printf("#\n#Setup watch\n");
//    non_rt.map("/foo", true);
//    rt.addWatch();
//    printf("#\n#Learn binding\n");
//    rt.handleCC(5, 2);
//    non_rt.useFreeID(5);
//    assert_int_eq(1, non_rt.getMidiMappingStrings().size(),
//            "One mapping is available after learning", __LINE__);
//
//    printf("#\n#unlearn binding\n");
//    non_rt.unMap("/foo", false);
//    non_rt.unMap("/foo", true);
//    assert_int_eq(0, non_rt.getMidiMappingStrings().size(),
//            "No mappings are available after unlearning", __LINE__);
//
//    //printf("#\n#Setup watch2\n");
//    //non_rt.map("/foo", true);
//    //rt.addWatch();
//    //printf("#\n#Relearn binding\n");
//    //rt.handleCC(5, 2);
//    //non_rt.useFreeID(5);
//    delete storage;
//    storage = NULL;
//    return;
}

void test_basic_learn(void)
{
    suite("test_basic_learn");
    rtosc::AutomationMgr mgr(4, 2, 16);
    Dummy d = {0,0};
    mgr.set_ports(p);
    mgr.set_instance(&d);
    mgr.createBinding(0, "/foo", false);
    mgr.backend = [&d](const char *msg) {
        rtosc::RtData rd;
        char loc[128];
        rd.loc = loc;
        rd.loc_size = sizeof(loc);
        rd.obj = &d; p.dispatch(msg, rd, true);};
    assert_str_eq("/foo", mgr.slots[0].automations[0].param_path, "Parameter is learned", __LINE__);
    assert_flt_eq(0, d.foo, "Learning does not change values", __LINE__);
    mgr.setSlot(0, 1);
    assert_flt_eq(10, d.foo, "Slot to max makes bound parameter max", __LINE__);
    mgr.setSlot(0, 0);
    assert_flt_eq(-1, d.foo, "Slot to min makes bound parameter min", __LINE__);
}

void test_slot_learn(void)
{
    suite("test_slot_learn");
    rtosc::AutomationMgr mgr(4, 2, 16);
    Dummy d = {0,0};
    mgr.set_ports(p);
    mgr.set_instance(&d);
    mgr.createBinding(0, "/foo", false);
    mgr.backend = [&d](const char *msg) {
        rtosc::RtData rd;
        char loc[128];
        rd.loc = loc;
        rd.loc_size = sizeof(loc);
        rd.obj = &d; p.dispatch(msg, rd, true);};
    assert_str_eq("/foo", mgr.slots[0].automations[0].param_path, "Parameter is learned", __LINE__);
    assert_flt_eq(0, d.foo, "Learning does not change values", __LINE__);
    mgr.setSlot(0, 1);
    assert_flt_eq(10, d.foo, "Slot to max makes bound parameter max", __LINE__);
    mgr.setSlot(0, 0);
    assert_flt_eq(-1, d.foo, "Slot to min makes bound parameter min", __LINE__);
    mgr.createBinding(1, "/bar", false);
    assert_str_eq("/bar", mgr.slots[1].automations[0].param_path, "Parameter is learned", __LINE__);
    assert_flt_eq(0, d.bar, "Learning does not change values", __LINE__);
    mgr.setSlot(1, 1);
    assert_flt_eq(100.2, d.bar, "Slot to max makes bound parameter max", __LINE__);
    mgr.setSlot(1, 0);
    assert_flt_eq(0.0, d.bar, "Slot to min makes bound parameter min", __LINE__);
}

void test_midi_learn(void)
{
    suite("test_midi_learn");
    rtosc::AutomationMgr mgr(4, 2, 16);
    Dummy d = {0,0};
    mgr.set_ports(p);
    mgr.set_instance(&d);
    mgr.backend = [&d](const char *msg) {
        rtosc::RtData rd;
        char loc[128];
        rd.loc = loc;
        rd.loc_size = sizeof(loc);
        rd.obj = &d; p.dispatch(msg, rd, true);};

    assert_int_eq(-1, mgr.slots[0].learning,
            "Parameter starts inactive", __LINE__);
    mgr.createBinding(0, "/foo", true);
    assert_str_eq("/foo", mgr.slots[0].automations[0].param_path,
            "Parameter is bound quickly", __LINE__);
    assert_int_eq(1, mgr.slots[0].learning,
            "Parameter is in learning state", __LINE__);
    mgr.handleMidi(0, 12, 0);
    assert_int_eq(-1, mgr.slots[0].learning, "Learning stops when MIDI CC is grabbed", __LINE__);
    assert_int_eq(12, mgr.slots[0].midi_cc, "MIDI CC is captured", __LINE__);

    mgr.handleMidi(0, 12, 127);
    assert_flt_eq(10, d.foo, "Slot to max makes bound parameter max", __LINE__);
    mgr.handleMidi(0, 12, 0);
    assert_flt_eq(-1, d.foo, "Slot to min makes bound parameter min", __LINE__);
}

void test_macro_learn(void)
{
    suite("test_macro_learn");
    rtosc::AutomationMgr mgr(4, 2, 16);
    Dummy d = {0,0};
    mgr.set_ports(p);
    mgr.set_instance(&d);
    mgr.backend = [&d](const char *msg) {
        rtosc::RtData rd;
        char loc[128];
        rd.loc = loc;
        rd.loc_size = sizeof(loc);
        rd.obj = &d; p.dispatch(msg, rd, true);};

    mgr.createBinding(0, "/foo", true);
    assert_str_eq("/foo", mgr.slots[0].automations[0].param_path, "Parameter is learned", __LINE__);
    assert_flt_eq(0, d.foo, "Learning does not change values", __LINE__);
    mgr.setSlot(0, 1);
    assert_flt_eq(10, d.foo, "Slot to max makes bound parameter max", __LINE__);
    mgr.setSlot(0, 0);
    assert_flt_eq(-1, d.foo, "Slot to min makes bound parameter min", __LINE__);
    mgr.createBinding(0, "/bar", true);
    assert_str_eq("/bar", mgr.slots[0].automations[1].param_path, "Parameter is learned", __LINE__);
    assert_flt_eq(0, d.bar, "Learning does not change values", __LINE__);
    mgr.setSlot(0, 1);
    assert_flt_eq(100.2, d.bar, "Slot to max makes bound parameter max", __LINE__);
    mgr.setSlot(0, 0);
    assert_flt_eq(0.0, d.bar, "Slot to min makes bound parameter min", __LINE__);
}

void test_curve_lerp(void)
{
    suite("test_curve_lerp");
    rtosc::AutomationMgr mgr(4, 2, 16);
    Dummy d = {0,0};
    mgr.set_ports(p);
    mgr.set_instance(&d);
    mgr.backend = [&d](const char *msg) {
        rtosc::RtData rd;
        char loc[128];
        rd.loc = loc;
        rd.loc_size = sizeof(loc);
        rd.obj = &d; p.dispatch(msg, rd, true);};

    mgr.createBinding(0, "/foo", true);
    assert_str_eq("/foo", mgr.slots[0].automations[0].param_path, "Parameter is learned", __LINE__);
    assert_int_eq(2, mgr.slots[0].automations[0].map.upoints, "Map is ready", __LINE__);


    printf("\n#Setting up high (+) slope\n");
    mgr.simpleSlope(0, 0, 80.0, 0.25);
    assert_flt_eq(0.0, d.foo,  "Setup does nothing", __LINE__);
    mgr.setSlot(0, 0.25);
    assert_flt_eq(-1, d.foo, "Minimum is correct", __LINE__);
    mgr.setSlot(0, 0.75);
    assert_flt_eq(10, d.foo, "Maximum is correct", __LINE__);


    printf("\n#Setting up low  (+) slope\n");
    mgr.simpleSlope(0, 0, 2.0, +3);
    mgr.setSlot(0, 0);
    assert_flt_eq(2.0, d.foo, "Minimum is correct", __LINE__);
    mgr.setSlot(0, 1.0);
    assert_flt_eq(4.0, d.foo, "Maximum is correct", __LINE__);

    printf("\n#Setting up low  (-) slope\n");
    mgr.simpleSlope(0, 0, -2.0, +3);
    mgr.setSlot(0, 0);
    assert_flt_eq(4.0, d.foo, "Minimum is correct", __LINE__);
    mgr.setSlot(0, 1.0);
    assert_flt_eq(2.0, d.foo, "Maximum is correct", __LINE__);

    printf("\n#Setting up high (-) slope\n");
    mgr.simpleSlope(0, 0, -80.0, 0.25);
    mgr.setSlot(0, 0.25);
    assert_flt_eq(10, d.foo, "Maximum is correct", __LINE__);
    mgr.setSlot(0, 0.75);
    assert_flt_eq(-1, d.foo, "Minimum is correct", __LINE__);
}

void test_curve_piecewise(void)
{
    rtosc::AutomationMgr mgr(4, 2, 16);
    mgr.set_ports(p);
}

void test_learn_many(void)
{
    rtosc::AutomationMgr mgr(4, 2, 16);
    mgr.set_ports(p);
    mgr.createBinding(0, "/foo", true);
    mgr.createBinding(0, "/bar", true);
    mgr.createBinding(0, "/foo", true);
    mgr.createBinding(0, "/bar", true);
    mgr.createBinding(0, "/foo", true);
    mgr.createBinding(0, "/bar", true);
    mgr.createBinding(0, "/foo", true);
    mgr.createBinding(0, "/bar", true);
}

int main()
{
    test_basic_learn();
    test_slot_learn();
    test_midi_learn();
    test_macro_learn();
    test_learn_many();
    return test_summary();
}
