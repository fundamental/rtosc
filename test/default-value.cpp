#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
#include <rtosc/default-value.h>
#include <rtosc/savefile.h>
#include <rtosc/port-sugar.h>

#include "common.h"

using namespace rtosc;

void port_sugar()
{
    const char* presets_str = rPresetsAt(1, 3, 2);
    assert_str_eq(":default 1", presets_str,
                  "Port sugar for presets (1)", __LINE__);
    assert_str_eq("=3", presets_str+11,
                  "Port sugar for presets (2)", __LINE__);
    assert_str_eq(":default 2", presets_str+14,
                  "Port sugar for presets (3)", __LINE__);
    assert_str_eq("=2", presets_str+25,
                  "Port sugar for presets (4)", __LINE__);

    const char* multi_str = rPresetAtMulti(false, 0, 2);
    assert_str_eq(":default 0", multi_str,
                  "Port sugar for presets (1)", __LINE__);
    assert_str_eq("=false", multi_str+11,
                  "Port sugar for presets (2)", __LINE__);
    assert_str_eq(":default 2", multi_str+18,
                  "Port sugar for presets (3)", __LINE__);
    assert_str_eq("=false", multi_str+29,
                  "Port sugar for presets (4)", __LINE__);
}

static const Ports ports = {
    {"A::i", rDefault(64) rDoc("..."), NULL, NULL },
    {"B#2::i", rOpt(-2, Master) rOpt(-1, Off)
        rOptions(Part1, Part2) rDefault(Off), NULL, NULL },
    {"C::T", rDefault(false), NULL, NULL},
    {"D::f", rDefault(1.0), NULL, NULL},
    {"E::i", "", NULL, NULL}
};

void canonical_values()
{
    const Ports ports = {
        { "A::ii:cc:SS", rOpt(1, one) rOpt(2, two) rOpt(3, three), NULL, NULL }
    };

    rtosc_arg_val_t av[3];
    av[0].type = 'S';
    av[0].val.s = "two";
    av[1].type = 'i';
    av[1].val.i = 42;
    av[2].type = 'S';
    av[2].val.s = "three";

    const Port* p = ports.apropos("A");
    const char* port_args = strchr(p->name, ':');

    assert_int_eq(1, canonicalize_arg_vals(av, 3, port_args, p->meta()),
                  "One argument was not converted", __LINE__);

    assert_char_eq(av[0].type, 'i',
                   "value was converted to correct canonical type", __LINE__);
    assert_int_eq(av[0].val.i, 2,
                  "value was converted to correct canonical type", __LINE__);
    assert_char_eq(av[1].type, 'i',
                   "value was not converted to canonical since "
                   "it was already canonical", __LINE__);
    assert_char_eq(av[2].type, 'S',
                   "value was not converted to canonical since "
                   "there was no recipe left in the port args", __LINE__);

}

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

    bool read_or_store(const char* m, RtData& d, size_t& ref)
    {
        if(*rtosc_argument_string(m)) {
            ref = rtosc_argument(m, 0).i;
            return true;
        }
        else
        {
            d.reply(d.loc, "i", (int)ref);
            return false;
        }
    }

    std::size_t sustain, attack_rate;
    int scale_type; //!< 0=log, 1=lin
    size_t env_type;
    int array[4];

    // constructor; sets all values to default
    Envelope() : scale_type(1), env_type(0)
    {
        update_env_type_dependencies();
    }

    void update_env_type_dependencies()
    {
        switch(env_type)
        {
            case 0:
                sustain = 30;
                attack_rate = 40;
                memset(array, 0, 4*sizeof(int));
                break;
            case 1:
                sustain = 127;
                attack_rate = 127;
                for(std::size_t i = 0; i < 4; ++i)
                    array[i] = i;
                break;
        }
    }
};

#define rObject Envelope
// preset 0: sustain value low (30), attack slow (40)
// preset 1: all knobs high
static const Ports envelope_ports = {
    /*
     * usual parameters
     */
    {"sustain::i", rProp(parameter) rDefaultDepends(env_type)
     rMap(default 0, 30) rMap(default 1, 127), NULL,
        [](const char* m, RtData& d) {
            Envelope* obj = static_cast<Envelope*>(d.obj);
            obj->read_or_store(m, d, obj->sustain); }},
    {"attack_rate::i", rProp(parameter) rDefaultDepends(env_type)
     rMap(default 0, 40) rDefault(127), NULL,
        [](const char* m, RtData& d) {
            Envelope* obj = static_cast<Envelope*>(d.obj);
            obj->read_or_store(m, d, obj->attack_rate); }},
    rOption(scale_type, rProp(parameter) rOpt(0, logarithmic), rOpt(1, linear),
            rDefault(linear), "scale type"),
    // port without rDefault macros
    // must not be noted for the savefile
    {"paste:b", rProp(parameter), NULL,
        [](const char* , RtData& ) { assert(false); } },
    /*
     * envelope type (~preset)
     */
    {"env_type::i", rProp(parameter) rDefault(0), NULL,
    [](const char* m, RtData& d) {
        Envelope* obj = static_cast<Envelope*>(d.obj);
        if(obj->read_or_store(m, d, obj->env_type)) {
            obj->update_env_type_dependencies();
        }}}
};
#undef rObject

const rtosc::Ports& Envelope::ports = envelope_ports;

void envelope_types()
{
    // by default, the envelope has preset 0, i.e.
    // sustain value low (30), attack slow (40)
    assert_str_eq("30", get_default_value("sustain::i", envelope_ports, NULL),
                  "get default value without runtime (1)", __LINE__);
    assert_str_eq("40", get_default_value("attack_rate::i", envelope_ports,
                                          NULL),
                  "get default value without runtime (2)", __LINE__);

    Envelope e1, e2;

    e1.sustain         =  40; // != envelope 0's default
    e2.sustain         =   0; // != envelope 1's default
    e2.attack_rate     = 127; //  = envelope 1's default
    e2.env_type        =   1; // != default
    e2.scale_type      =   0; // != default

    assert_str_eq("30", get_default_value("sustain::i", envelope_ports, &e1),
                  "get default value with runtime (1)", __LINE__);
    assert_str_eq("40", get_default_value("attack_rate::i", envelope_ports,
                                          &e1),
                  "get default value with runtime (2)", __LINE__);

    assert_str_eq("127", get_default_value("sustain::i", envelope_ports, &e2),
                  "get default value with runtime (3)", __LINE__);
    assert_str_eq("127",
                  get_default_value("attack_rate::i", envelope_ports, &e2 ),
                  "get default value with runtime (4)", __LINE__);

    assert_str_eq("/sustain 40",
                  get_changed_values(envelope_ports, &e1).c_str(),
                  "get changed values where none are changed", __LINE__);
    const char* changed_e2 = "/sustain 0\n/scale_type logarithmic\n"
                             "/env_type 1";
    assert_str_eq(changed_e2,
                  get_changed_values(envelope_ports, &e2).c_str(),
                  "get changed values where three are changed", __LINE__);

    // restore values to envelope from a savefile
    // this is indirectly related to default values
    Envelope e3;
    // note: setting the envelope type does overwrite all other values
    //       this means that "/sustain 60" has to be executed after
    //       "/env_type 1", even though the order in the savefile is different
    //       dispatch_printed_messages() executes these messages in the
    //       correct order.
    int num = dispatch_printed_messages("%this is a savefile\n"
                                        "/sustain 60\n /env_type 1\n"
                                        "/attack_rate 10 % a useless comment\n"
                                        "/scale_type 0",
                                        envelope_ports, &e3);
    assert_int_eq(4, num,
                  "restore values from savefile - correct number", __LINE__);
    assert_int_eq(60, e3.sustain, "restore values from savefile (1)", __LINE__);
    assert_int_eq(1, e3.env_type, "restore values from savefile (2)", __LINE__);
    assert_int_eq(10, e3.attack_rate,
                  "restore values from savefile (3)", __LINE__);
    assert_int_eq(0, e3.scale_type,
                  "restore values from savefile (4)", __LINE__);

    num = dispatch_printed_messages("/sustain 60 /env_type $1",
                                    envelope_ports, &e3);
    assert_int_eq(-13, num,
                  "restore values from a corrupt savefile (3)", __LINE__);

    std::string appname = "default-values-test";
    std::string savefile = save_to_file(envelope_ports, &e2,
                                        appname.c_str());
    std::string exp_savefile = changed_e2;

    assert_str_eq(exp_savefile.c_str(), savefile.c_str(),
                  "save testfile", __LINE__);

    Envelope e2_restored; // e2 will be loaded from the savefile

    int rval = load_from_file(exp_savefile.c_str(),
                              envelope_ports, &e2_restored,
                              appname.c_str());

    assert_int_eq(3, rval,
                  "load savefile from file, 4 messages read", __LINE__);

    auto check_restored = [](int i1, int i2, const char* member) {
        std::string tcs = "restore "; tcs += member; tcs += " from file";
        assert_int_eq(i1, i2, tcs.c_str(), __LINE__);
    };

    check_restored(e2.sustain, e2_restored.sustain, "sustain value");
    check_restored(e2.attack_rate, e2_restored.attack_rate, "attack rate");
    check_restored(e2.scale_type, e2_restored.scale_type, "scale type");
    check_restored(e2.env_type, e2_restored.env_type, "envelope type");
}

void presets()
{
    // for presets, it would be exactly the same,
    // with only the env_type port being named as "preset" port.
}

struct SavefileTest
{
    static const rtosc::Ports& ports;

    int new_param;
    bool very_old_version;
    int further_param;
};

#define rObject SavefileTest
static const Ports savefile_test_ports = {
    {"new_param:i", "", NULL,
        [](const char* m, RtData& d) {
            SavefileTest* obj = static_cast<SavefileTest*>(d.obj);
            obj->new_param = rtosc_argument(m, 0).i; }},
    {"very_old_version:T:F", "", NULL,
        [](const char* m, RtData& d) {
            SavefileTest* obj = static_cast<SavefileTest*>(d.obj);
            obj->very_old_version = rtosc_argument(m, 0).T; }},
    {"further_param:i", "", NULL,
        [](const char* m, RtData& d) {
            SavefileTest* obj = static_cast<SavefileTest*>(d.obj);
            obj->further_param = rtosc_argument(m, 0).i; }}
};
#undef rObject

const rtosc::Ports& SavefileTest::ports = savefile_test_ports;

int main()
{
    port_sugar();

    canonical_values();
    simple_default_values();
    envelope_types();
    presets();

    return test_summary();
}
