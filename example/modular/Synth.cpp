#include "Synth.h"
#include "EffectMgr.h"
#include "util.h"
#include <rtosc/ports.h>
using namespace rtosc;

static Ports<3,Synth> _ports{{{
    Port<Synth>("volume:f","10^:0.001:1", parf<Synth>(&Synth::volume)),
    Port<Synth>("oscil/", Oscillator::ports, recur<Synth,Oscillator>(&Synth::oscil)),
    Port<Synth>("effect#8/", EffectMgr::ports, recur_array<Synth, EffectMgr>(&Synth::effects))

}}};

mPorts *Synth::ports = &_ports;

void Synth::dispatch(msg_t m)
{
    _ports.dispatch(m,this);
}
