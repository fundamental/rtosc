#include "Oscil.h"
#include "util.h"
#include <rtosc/ports.h>
using namespace rtosc;


static Ports<1,Oscillator> _ports{{{
    Port<Oscillator>("freq:f","10^,0.01:,000",
            parf<Oscillator>(&Oscillator::freq))
}}};

mPorts *Oscillator::ports = &_ports;


void Oscillator::dispatch(msg_t m)
{
    _ports.dispatch(m,this);
}
