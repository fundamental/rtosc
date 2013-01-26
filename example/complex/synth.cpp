#include <rtosc/rtosc.h>
#include <rtosc/thread-link.h>
#include <rtosc/ports.h>
#include <rtosc/miditable.h>
#include <string.h>
#include <cmath>
#include "synth.h"
#include "util.h"

using namespace rtosc;

float Fs = 0.0f;

ThreadLink bToU(1024,1024);
ThreadLink uToB(1024,1024);

Ports Adsr::ports = {
    PARAM(Adsr, av, av, lin, -1.0, 1.0, "attack  value"),
    PARAM(Adsr, dv, dv, lin, -1.0, 1.0, "decay   value"),
    PARAM(Adsr, sv, sv, lin, -1.0, 1.0, "sustain value"),
    PARAM(Adsr, rv, rv, lin, -1.0, 1.0, "release value"),
    PARAM(Adsr, at, at, log,  1e-3, 10, "attack  time"),
    PARAM(Adsr, dt, dt, log,  1e-3, 10, "decay   time"),
    PARAM(Adsr, rt, rt, log,  1e-3, 10, "release time")
};

//sawtooth generator
float oscil(float freq)
{
    static float phase = 0.0f;
    phase += freq/Fs;
    if(phase > 1.0f)
        phase -= 1.0f;

    return phase;
}

inline float Synth::sample(void)
{
    return oscil(freq*(1+frq_env(gate))/2.0f)*(1+amp_env(gate))/2.0f;
}

float interp(float a, float b, float pos)
{
    return (1.0f-pos)*a+pos*b;
}

//Linear ADSR
float Adsr::operator()(bool gate)
{
    time += 1.0/Fs;

    if(gate == false && pgate == true)
        relval = (*this)(true);
    if(gate != pgate)
        time = 0.0f;
    pgate = gate;

    float reltime = time;
    if(gate) {
        if(at > reltime) //Attack
            return interp(av, dv, reltime/at);
        reltime -= at;
        if(dt > reltime) //Decay
            return interp(dv, sv, reltime/dt);
        return sv;        //Sustain
    }
    if(rt > reltime)     //Release
        return interp(relval, rv, reltime/rt);

    return rv;
}

MidiTable<64,64> midi(Synth::ports);

Synth s;
void process_control(unsigned char control[3]);
rtosc::Ports Synth::ports = {
    RECUR(Synth, Adsr, amp-env, amp_env, "amplitude envelope"),
    RECUR(Synth, Adsr, frq-env, frq_env, "frequency envelope"),
    PARAM(Synth, freq, freq,    log, 1, 1e3, "note frequency"),
    {"gate:T","::", 0, [](msg_t,void*v){((Synth*)v)->gate=true;}},
    {"gate:F","::", 0, [](msg_t,void*v){((Synth*)v)->gate=false;}},
    {"register:iis","::", 0, [](msg_t m,void*){
            //printf("registering element...\n");
            //printf("%d %d\n",argument(m,0).i,argument(m,1).i);
            const char *pos = rtosc_argument(m,2).s;
            while(*pos) putchar(*pos++);
            //printf("%p %p %c(%d)\n", m, argument(m,2).s, *(argument(m,2).s), *(argument(m,2).s));
            //printf("%p\n", argument(m,2).s);
            midi.addElm(rtosc_argument(m,0).i,rtosc_argument(m,1).i,rtosc_argument(m,2).s);
            //printf("adding element %d %d\n",argument(m,0).i,argument(m,1).i);
            //printf("---------path: %s %s\n",argument(m,2).s, Synth::ports.meta_data(argument(m,2).s+1));
            //unsigned char ctl[3] = {2,13,107};
            //process_control(ctl);
            //printf("synth.amp-env.av=%f\n", s.amp_env.av);
            }},
    {"learn:s", "::", 0, [](msg_t m, void*){
            midi.learn(rtosc_argument(m,0).s);
            }}

};

Ports *root_ports = &Synth::ports;


float &freq = s.freq;
bool  &gate = s.gate;

void event_cb(msg_t m)
{
    Synth::ports.dispatch(m+1, &s);
    bToU.raw_write(m);
    puts("event-cb");
    if(rtosc_type(m,0) == 'f')
        printf("%s -> %f\n", m, rtosc_argument(m,0).f);
    if(rtosc_type(m,0) == 'i')
        printf("%s -> %d\n", m, rtosc_argument(m,0).i);
}

#include <err.h>
void synth_init(void)
{
    printf("%p\n", Adsr::ports["dv"]->metadata);
    printf("'%d'\n", Adsr::ports["dv"]->metadata[0]);
    if(strlen(Adsr::ports["dv"]->metadata)<3)
        errx(1,"bad metadata");
    midi.event_cb = event_cb;
}

void process_control(unsigned char ctl[3])
{
    midi.process(ctl[0]&0x0F, ctl[1], ctl[2]);
}

void process_output(float *smps, unsigned nframes)
{
    while(uToB.hasNext())
        Synth::ports.dispatch(uToB.read()+1, &s);

    for(unsigned i=0; i<nframes; ++i)
        smps[i] = s.sample();
}
