#include "Oscil.h"
#include "EffectMgr.h"

struct Synth
{
    float volume;
    Oscillator oscil;
    EffectMgr effects[8];
    void dispatch(const char *m);
    static rtosc::mPorts *ports;
};
