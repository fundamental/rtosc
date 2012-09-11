#include "../../include/rtosc.h"
#include <string.h>

float Fs   = 0.0f;
float freq = 0.0f;
bool  gate = false;

struct Adsr
{
    float time;
    float relval;
    bool  pgate;
    float length[4];
    float value[4];

    float operator()(bool gate);
};

static Adsr amp_env, frq_env;

//sawtooth generator
float oscil(float freq)
{
    static float phase = 0.0f;
    phase += freq/Fs;
    if(phase > 1.0f)
        phase -= 1.0f;

    return phase;
}

float sample(void)
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
        if(length[0] > reltime) //Attack
            return interp(value[0], value[1], reltime/length[0]);
        reltime -= length[0];
        if(length[1] > reltime) //Decay
            return interp(value[1], value[2], reltime/length[1]);
        return value[2];        //Sustain
    }
    if(length[3] > reltime)     //Release
        return interp(relval, value[3], reltime/length[3]);

    return value[3];
}


struct Dispatch
{
    const char *path;
    float *data;
    float min, max;
    const char *fn;
};

//Dispatch information
Dispatch d_table[] = 
{
    {"/amp/env/av", amp_env.value,   -1.0,1.0,"1"},
    {"/amp/env/dv", amp_env.value+1, -1.0,1.0,"1"},
    {"/amp/env/sv", amp_env.value+2, -1.0,1.0,"1"},
    {"/amp/env/rv", amp_env.value+3, -1.0,1.0,"1"},
    {"/amp/env/at", amp_env.length,   0.001,10.0,"10^"},
    {"/amp/env/dt", amp_env.length+1, 0.001,10.0,"10^"},
    {"/amp/env/rt", amp_env.length+3, 0.001,10.0,"10^"},
    {"/frq/env/av", frq_env.value,   -1.0,1.0,"1"},
    {"/frq/env/dv", frq_env.value+1, -1.0,1.0,"1"},
    {"/frq/env/sv", frq_env.value+2, -1.0,1.0,"1"},
    {"/frq/env/rv", frq_env.value+3, -1.0,1.0,"1"},
    {"/frq/env/at", frq_env.length,   0.001,10.0,"10^"},
    {"/frq/env/dt", frq_env.length+1, 0.001,10.0,"10^"},
    {"/frq/env/rt", frq_env.length+3, 0.001,10.0,"10^"},
    {"/freq",       &freq,            10, 1000, "10^"},
    {0}
};

void dsp_dispatch(const char *msg)
{
    Dispatch *itr = d_table;
    while(itr->path && strcmp(msg, itr->path))
        ++itr;
    if(itr->path)
        *itr->data = argument(msg, 0).f;
    if(!strcmp(msg,"/gate"))
        gate = argument(msg,0).T;
}
