#include <rtosc.h>
#include <thread-link.h>
#include <string.h>
#include <cmath>
#include "synth.h"

float Fs = 0.0f;

ThreadLink<1024,1024> bToU;
ThreadLink<1024,1024> uToB;

//Defines a port callback for something that can be set and looked up
template<class T>
std::function<void(msg_t,T*)> param(float T::*p)
{
    return [p](msg_t m, T*t)
    {
        //printf("param of %s\n", m);
        if(rtosc_narguments(m)==0) {
            bToU.write(uToB.peak(), "f", (t->*p));
            //printf("just wrote a '%s' %f\n", uToB.peak(), t->*p);
        } else if(rtosc_narguments(m)==1 && rtosc_type(m,0)=='f') {
            (t->*p)=rtosc_argument(m,0).f;
            //printf("just set a '%s' %f\n", uToB.peak(), t->*p);
        }
        //printf("%f\n", t->*p);
    };
}

Ports<7,Adsr> _adsrPorts{{{
    Port<Adsr>("av:f:", "1,-1.0,1.0:v:", param(&Adsr::av)),
    Port<Adsr>("dv:f:", "1,-1.0,1.0:v:", param<Adsr>(&Adsr::dv)),
    Port<Adsr>("sv:f:", "1,-1.0,1.0:v:", param<Adsr>(&Adsr::sv)),
    Port<Adsr>("rv:f:", "1,-1.0,1.0:v:", param<Adsr>(&Adsr::rv)),
    Port<Adsr>("at:f:", "10^,0.001,10.0:v:", param<Adsr>(&Adsr::at)),
    Port<Adsr>("dt:f:", "10^,0.001,10.0:v:", param<Adsr>(&Adsr::dt)),
    Port<Adsr>("rt:f:", "10^,0.001,10.0:v:", param<Adsr>(&Adsr::rt))
}}};

_Ports &Adsr::ports = _adsrPorts;
void Adsr::dispatch(msg_t m)
{
    _adsrPorts.dispatch(m,this);
}


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

const char *snip(const char *m)
{
    while(*m && *m!='/')++m;
    return *m?m+1:m;
}

template<class T, class TT>
std::function<void(msg_t,T*)> recur(TT T::*p)
{
    return [p](msg_t m, T*t){(t->*p).dispatch(snip(m));};
}

MidiTable<64,64> midi;

unsigned char rouge = 255;

Synth s;
void process_control(unsigned char control[3]);
Ports<7,Synth> _synthPorts{{{
    Port<Synth>("amp-env/",&_adsrPorts, recur<Synth,Adsr>(&Synth::amp_env)),
    Port<Synth>("frq-env/",&_adsrPorts, recur<Synth,Adsr>(&Synth::frq_env)),
    Port<Synth>("freq:f:","10^,0.001,10.0:v:", param<Synth>(&Synth::freq)),
    Port<Synth>("gate:T","::", [](msg_t,Synth*s){s->gate=true;}),
    Port<Synth>("gate:F","::", [](msg_t,Synth*s){s->gate=false;}),
    Port<Synth>("register:iis","::",[](msg_t m,Synth*){
            //printf("registering element...\n");
            //printf("%d %d\n",argument(m,0).i,argument(m,1).i);
            const char *pos = rtosc_argument(m,2).s;
            while(*pos) putchar(*pos++);
            //printf("%p %p %c(%d)\n", m, argument(m,2).s, *(argument(m,2).s), *(argument(m,2).s));
            //printf("%p\n", argument(m,2).s);
            midi.addElm(rtosc_argument(m,0).i,rtosc_argument(m,1).i,rtosc_argument(m,2).s,
                Synth::ports.meta_data(rtosc_argument(m,2).s+1));
            //printf("adding element %d %d\n",argument(m,0).i,argument(m,1).i);
            //printf("---------path: %s %s\n",argument(m,2).s, Synth::ports.meta_data(argument(m,2).s+1));
            //unsigned char ctl[3] = {2,13,107};
            //process_control(ctl);
            //printf("synth.amp-env.av=%f\n", s.amp_env.av);
            }),
    Port<Synth>("learn:s", "::",[](msg_t m, Synth*){
            if(rouge != 255)
                midi.addElm(0,rouge,rtosc_argument(m,0).s,Synth::ports.meta_data(rtosc_argument(m,0).s+1));
            rouge = 255;
            })

}}};

_Ports &Synth::ports = _synthPorts;
_Ports *root_ports = &_synthPorts;

void Synth::dispatch(msg_t m)
{
    _synthPorts.dispatch(m,this);
}


float &freq = s.freq;
bool  &gate = s.gate;


float translate(unsigned char val, const char *conversion)
{
    int type = 0;
    if(conversion[0]=='1' && conversion[1]==':')
        type = 1; //linear
    else if(conversion[0]=='1' && conversion[1]=='0' && conversion[2]=='^')
        type = 2; //exponential

    while(*conversion && *conversion!=',') *conversion++;
    float min = atof(conversion);
    while(*conversion && *conversion!=',') *conversion++;
    float max = atof(conversion);

    //Allow for middle value to be set
    float x = val!=64.0 ? val/127.0 : 0.5;

    if(type == 1)
        return x*(max-min)+min;
    else if(type == 2) {
        const float b = log(min)/log(10);
        const float a = log(max)/log(10)-b;
        return powf(10.0f, a*x+b);
    }

    return 0;
}

void process_control(unsigned char control[3])
{
    //puts("process_control...");
    //printf("%d, %d\n",control[0],control[1]);
    //puts("process_control working...");

    const MidiAddr<64> *addr = midi.get(0,control[0]);
    if(addr) {
        char buffer[1024];
        rtosc_message(buffer,1024,addr->path,"f",
                translate(control[1],addr->conversion));
        s.dispatch(buffer+1);
        bToU.raw_write(buffer);
    } else
        rouge = control[0];
}

void process_output(float *smps, unsigned nframes)
{
    //printf("%f\n", s.amp_env.at);
    while(uToB.hasNext())
        s.dispatch(uToB.read()+1);

    for(unsigned i=0; i<nframes; ++i)
        smps[i] = s.sample();
}
