//Test to verify dispatch performance of rtosc is good enough

#include <ctime>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <string>

#ifdef HAVE_LIBLO
#include <lo/lo_lowlevel.h>
#endif

#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
using namespace rtosc;

void do_nothing(const char *data, RtData &d)
{
    (void) data;
    (void) d;
}

#ifdef HAVE_LIBLO
int liblo_do_nothing(const char *path, const char *types,
                     lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    (void) path;
    (void) types;
    (void) argv;
    (void) argc;
    (void) msg;
    (void) user_data;
    return 0; /* = message has been handled */
}

int liblo_count(const char *path, const char *types,
                lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    ++*((int*)user_data);
    return liblo_do_nothing(path, types, argv, argc, msg, user_data);
}
#endif

#define dummy(x)  {#x, NULL, NULL, do_nothing}
#define dummyT(x) {#x"::T:F", NULL, NULL, do_nothing}
#define dummyC(x) {#x"::c", NULL, NULL, do_nothing}
#define dummyI(x) {#x"::i", NULL, NULL, do_nothing}
#define dummyO(x) {#x"::s:c:i", NULL, NULL, do_nothing}


//TODO Consider performance penalty of argument specifier
//chains
//55 elements long (assuming I can count properly)
Ports port_table = {
    dummy(oscil/),
    dummy(mod-oscil/),
    dummy(FreqLfo/),
    dummy(AmpLfo/),
    dummy(FilterLfo/),
    dummy(FreqEnvelope/),
    dummy(AmpEnvelope/),
    dummy(FilterEnvelope/),
    dummy(FMFreqEnvelope/),
    dummy(FMAmpEnvelope/),
    dummy(VoiceFilter/),
    dummyT(Enabled),//section
    dummyC(Unison_size),
    dummyC(Unison_frequency_spread),
    dummyC(Unison_stereo_spread),
    dummyC(Unison_vibratto),
    dummyC(Unison_vibratto_speed),
    dummyO(Unison_invert_phase),
    dummyO(Type),
    dummyC(PDelay),
    dummyT(Presonance),
    dummyC(Pextoscil),
    dummyC(PextFMoscil),
    dummyC(Poscilphase),
    dummyC(PFMoscilphase),
    dummyT(Pfilterbypass),
    dummyT(Pfixedfreq),//Freq Stuff
    dummyC(PfixedfreqET),
    dummyI(PDetune),
    dummyI(PCoarseDetune),
    dummyC(PDetuneType),
    dummyT(PFreqEnvelopeEnabled),
    dummyT(PFreqLfoEnabled),
    dummyC(PPanning),//Amplitude Stuff
    dummyC(PVolume),
    dummyT(PVolumeminus),
    dummyC(PAmpVelocityScaleFunction),
    dummyT(PAmpEnvelopeEnabled),
    dummyT(PAmpLfoEnabled),
    dummyT(PFilterEnabled),//Filter Stuff
    dummyT(PFilterEnvelopeEnabled),
    dummyT(PFilterLfoEnabled),
    dummyC(PFMEnabled),//Modulator Stuff
    dummyI(PFMVoice),
    dummyC(PFMVolume),
    dummyC(PFMVolumeDamp),
    dummyC(PFMVelocityScaleFunction),
    dummyI(PFMDetune),
    dummyI(PFMCoarseDetune),
    dummyC(PFMDetuneType),
    dummyT(PFMFreqEnvelopeEnabled),
    dummyT(PFMAmpEnvelopeEnabled),
    dummy(detunevalue),
    dummy(octave),
    dummy(coarsedetune),
};

char events[20][1024];
char loc_buffer[1024];

static void liblo_error_cb(int i, const char *m, const char *loc)
{
    fprintf(stderr, "liblo :-( %d-%s@%s\n",i,m,loc);
}

void print_results(const char* libname,
                   clock_t t_on, clock_t t_off, int repeats)
{
    double seconds = (t_off - t_on) * 1.0 / CLOCKS_PER_SEC;
    double ns_per_dispatch = seconds*1e9/(repeats*20.0);
    assert(ns_per_dispatch < 100000.00); // fit the field width

    printf("%s Performance: %8.2f seconds for the test\n", libname, seconds);
    printf("%s Performace:  %8.2f ns per dispatch\n", libname, ns_per_dispatch);
}

int main()
{
    /*
     * create all the messages
     */
    rtosc_message(events[0],  1024, "PFMDetune", "i", 23);
    rtosc_message(events[1],  1024, "oscil/blam", "c", 23);
    rtosc_message(events[2],  1024, "PFilterEnabled", "T");
    rtosc_message(events[3],  1024, "PVolume", "c", 23);
    rtosc_message(events[4],  1024, "Enabled", "T");
    rtosc_message(events[5],  1024, "Unison_size", "c", 1);
    rtosc_message(events[6],  1024, "Unison_frequency_spread", "c", 2);
    rtosc_message(events[7],  1024, "Unison_stereo_spread", "c", 3);
    rtosc_message(events[8],  1024, "Unison_vibratto", "c", 4);
    rtosc_message(events[9],  1024, "Unison_vibratto_speed", "c", 5);
    rtosc_message(events[10], 1024, "Unison_invert_phase", "");
    rtosc_message(events[11], 1024, "FilterLfo/another/few/layers", "");
    rtosc_message(events[12], 1024, "FreqEnvelope/blam", "");
    rtosc_message(events[13], 1024, "PINVALID_RANDOM_STRING", "ics", 23, 23, "23");
    rtosc_message(events[14], 1024, "PFMVelocityScaleFunction", "i", 23);
    rtosc_message(events[15], 1024, "PFMDetune", "i", 230);
    rtosc_message(events[16], 1024, "Pfixedfreq", "F");
    rtosc_message(events[17], 1024, "detunevalue", "");
    rtosc_message(events[18], 1024, "PfixedfreqET", "c", 10);
    rtosc_message(events[19], 1024, "PfixedfreqET", "");
    RtData d;
    d.loc_size = 1024;
    d.obj = d.loc = loc_buffer;
    d.matches = 0;

    /*
     * run RTOSC
     */
    int repeats = 200000;
    clock_t t_on = clock(); // timer before calling func
    for(int j=0; j<200000; ++j) {
        for(int i=0; i<20; ++i){
            port_table.dispatch(events[i], d);
        }
    }
    //printf("Matches: %d\n", d.matches);
    assert(d.matches == 3600000);
    int t_off = clock(); // timer when func returns
    print_results("RTOSC", t_on, t_off, repeats);

#ifdef HAVE_LIBLO
    /*
     * prepare LIBLO message data
     */
    struct liblo_message_prepared
    {
        char* memory;
        size_t exact_size;
    };
    std::vector<liblo_message_prepared> lo_messages;

    int max = sizeof(events)/sizeof(events[0]);
    lo_messages.resize(max);
    for(int i = 0; i < max; ++i)
    {
        lo_messages[i].memory = events[i];
        lo_messages[i].exact_size = rtosc_message_length(events[i], 1024);
    }

    /*
     * prepare LIBLO port info
     */
    struct liblo_port_info_t
    {
        std::string name;
        std::vector<std::string> typespecs;
        bool accept_all = false;
    };
    std::vector<liblo_port_info_t> liblo_port_info;

    int port_size = port_table.ports.size();
    liblo_port_info.resize(port_size);
    std::vector<liblo_port_info_t>::iterator pinf =
        liblo_port_info.begin();
    for(const Port& port : port_table)
    {
        for(const char* p = port.name; *p && *p != ':'; ++p)
            pinf->name.push_back(*p);

        // version of strchr() which will return end of string on miss
        auto m_strchr = [&](const char* s, char c) -> const char*
        {
            for(; *s && *s != c; ++s) ;
            return s;
        };

        const char* itr = m_strchr(port.name, ':'), *next;
        if(!*itr)
            pinf->accept_all = true;
        else for(; *itr; itr = next)
        {
            next = m_strchr(itr+1, ':');
            if(itr[1])
                pinf->typespecs.emplace_back(itr+1, next - (itr+1));
        }
        ++pinf;
    }

    // liblo does not support trees, so messages like 'oscil/blam' won't
    // dispatch at ports like 'oscil/'. we need to help them a bit...
    for(const liblo_message_prepared& lmp : lo_messages)
    if(strchr(lmp.memory, '/'))
    {
        liblo_port_info_t pinf;
        pinf.name = lmp.memory;
        pinf.typespecs.push_back("");
        pinf.accept_all = true;
        liblo_port_info.push_back(pinf);
    }

    auto add_methods = [&](const std::vector<liblo_port_info_t>& port_info,
                           lo_server server, lo_method_handler handler,
                           void* user_data = nullptr)
    {
        for(const liblo_port_info_t& pinf : port_info)
        {
            if(pinf.accept_all && pinf.name[pinf.name.length()-1] != '/')
                lo_server_add_method(server, pinf.name.c_str(), nullptr,
                                     handler, user_data);
            else for(const std::string& spec : pinf.typespecs)
                lo_server_add_method(server, pinf.name.c_str(), spec.c_str(),
                                     handler, user_data);
        }
    };

    /*
     * test LIBLO
     */
    int counter = 0;
    lo_server test_server = lo_server_new(nullptr, liblo_error_cb);
    add_methods(liblo_port_info, test_server, liblo_count, &counter);

    for(int i=0; i<20; ++i)
    {
        int ok = lo_server_dispatch_data(test_server, lo_messages[i].memory,
                                         lo_messages[i].exact_size);
        assert(ok);
        (void)ok;
    }
    assert(counter == 18);

    /*
     * run LIBLO
     */
    lo_server server = lo_server_new(nullptr, liblo_error_cb);
    add_methods(liblo_port_info, server, liblo_do_nothing);

    t_on = clock(); // timer before calling func
    for(int j=0; j<200000; ++j) {
        for(int i=0; i<20; ++i){
            lo_server_dispatch_data(server, lo_messages[i].memory,
                                            lo_messages[i].exact_size);
        }
    }
    t_off = clock(); // timer when func returns
    print_results("LIBLO", t_on, t_off, repeats);
#endif

    return EXIT_SUCCESS;
}
