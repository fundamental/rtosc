#include <cassert>
#include <cstdlib>

#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>

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
    assert(!strcmp(get_default_value("A", ports, NULL), "64"));
    assert(!strcmp(get_default_value("B0", ports, NULL), "Off"));
    assert(!strcmp(get_default_value("B1", ports, NULL), "Off"));
    assert(!strcmp(get_default_value("C", ports, NULL), "false"));
    assert(!strcmp(get_default_value("D", ports, NULL), "1.0"));
    assert(get_default_value("E", ports, NULL) == NULL);
}

struct Envelope
{
    static const rtosc::Ports& ports;
    std::size_t env_type;
};

// preset 0: volume low (30), attack slow (40)
// preset 1: all knobs high
static const Ports envelope_ports = {
    {"volume::i", rDefaultDepends(env_type) rMap(default 0, 30) rMap(default 1, 127), NULL, NULL },
    {"attack_rate::i", rDefaultDepends(env_type) rMap(default 0, 40) rMap(default 1, 127), NULL, NULL },
    {"env_type::i", rDefault(0), NULL, [](const char*, RtData& d) {
         d.reply("env_type", "i", (static_cast<Envelope*>(d.obj))->env_type); } }
};

const rtosc::Ports& Envelope::ports = envelope_ports;

void envelope_types()
{
    // by default, the envelope has preset 0, i.e. volume low (30), attack slow (40)
    assert(!strcmp(get_default_value("volume", envelope_ports, NULL), "30"));
    assert(!strcmp(get_default_value("attack_rate", envelope_ports, NULL), "40"));

    Envelope e1, e2, e3;

    e1.env_type = 0;
    e2.env_type = 1;
    e3.env_type = 2; // fake envelope type - will raise an error later

    assert(!strcmp(get_default_value("volume", envelope_ports, &e1), "30"));
    assert(!strcmp(get_default_value("attack_rate", envelope_ports, &e1), "40"));

    assert(!strcmp(get_default_value("volume", envelope_ports, &e2), "127"));
    assert(!strcmp(get_default_value("attack_rate", envelope_ports, &e2 ), "127"));

    bool error_caught = false;
    try {
        get_default_value("volume", envelope_ports, &e3);
    }
    catch(std::logic_error e) {
        error_caught = true;
        assert(!strcmp(e.what(), "get_default_value(): "
                                 "value depends on a variable which is "
                                 "out of range of the port's values "));
    }
    assert(error_caught);
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
    return EXIT_SUCCESS;
}
