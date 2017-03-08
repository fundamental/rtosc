#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>

#include "common.h"

using namespace rtosc;

static const Ports ports = {
    {"A::i", rDefault(64) rDoc("..."), NULL, NULL },
    {"B#2::i", rOpt(-2, Master) rOpt(-1, Off)
        rOptions(Part1, Part2) rDefault(Off), NULL, NULL },
    {"C::T", rDefault(false), NULL, NULL},
    {"D::f", rDefault(1.0), NULL, NULL},
    {"E::i", "", NULL, NULL}
};

void simple_default_values()
{
    assert_str_eq(get_default_value("A", ports, NULL), "64",
                  "get integer default value", __LINE__);
    assert_str_eq(get_default_value("B0", ports, NULL), "Off",
                  "get integer array default value with options", __LINE__);
    assert_str_eq(get_default_value("B1", ports, NULL), "Off",
                  "get integer array default value with options", __LINE__);
    assert_str_eq(get_default_value("C", ports, NULL), "false",
                  "get boolean default value", __LINE__);
    assert_str_eq(get_default_value("D", ports, NULL), "1.0",
                  "get float default value", __LINE__);
    assert_null(get_default_value("E", ports, NULL),
                "get default value where there's none", __LINE__);
}

struct Envelope
{
    static const rtosc::Ports& ports;
    std::size_t volume, attack_rate, env_type;
};

// preset 0: volume low (30), attack slow (40)
// preset 1: all knobs high
static const Ports envelope_ports = {
    {"volume::i", rDefaultDepends(env_type) rMap(default 0, 30) rMap(default 1, 127), NULL,
        [](const char*, RtData& d) {
            d.reply("volume", "i", (static_cast<Envelope*>(d.obj))->volume); }},
    {"attack_rate::i", rDefaultDepends(env_type) rMap(default 0, 40) rMap(default 1, 127), NULL,
        [](const char*, RtData& d) {
            d.reply("attack_rate", "i", (static_cast<Envelope*>(d.obj))->attack_rate); }},
    {"env_type::i", rDefault(0), NULL, [](const char*, RtData& d) {
        d.reply("env_type", "i", (static_cast<Envelope*>(d.obj))->env_type); } }
};

const rtosc::Ports& Envelope::ports = envelope_ports;

void envelope_types()
{
    // by default, the envelope has preset 0, i.e. volume low (30), attack slow (40)
    assert_str_eq("30", get_default_value("volume", envelope_ports, NULL),
                  "get default value without runtime (1)", __LINE__);
    assert_str_eq("40", get_default_value("attack_rate", envelope_ports, NULL),
                  "get default value without runtime (2)", __LINE__);

    Envelope e1, e2;

    e1.volume       =  40; // != envelope 0's default
    e1.attack_rate  =  40; // = envelope 0's default
    e1.env_type     =   0; // = default
    e2.volume       =   0; // != envelope 1's default
    e2.attack_rate  = 127; // = envelope 1's default
    e2.env_type     =   1; // != default

    assert_str_eq("30", get_default_value("volume", envelope_ports, &e1),
                  "get default value with runtime (1)", __LINE__);
    assert_str_eq("40", get_default_value("attack_rate", envelope_ports, &e1),
                  "get default value with runtime (2)", __LINE__);

    assert_str_eq("127", get_default_value("volume", envelope_ports, &e2),
                  "get default value with runtime (3)", __LINE__);
    assert_str_eq("127", get_default_value("attack_rate", envelope_ports, &e2 ),
                  "get default value with runtime (4)", __LINE__);

    assert_str_eq("/volume 40", get_changed_values(envelope_ports, &e1).c_str(),
                  "get changed values where none are changed", __LINE__);
    assert_str_eq("/volume 0\n/env_type 1", get_changed_values(envelope_ports, &e2).c_str(),
                  "get changed values where one is changed", __LINE__);
}

void presets()
{
    // for presets, it would be exactly the same,
    // with only the env_type port being named as "preset" port.
}

int main()
{
    simple_default_values();
    envelope_types();
    presets();
    return test_summary();
}
