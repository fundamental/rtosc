#include <rtosc/ports.h>
#include "LFO.h"
#include "util.h"

using namespace rtosc;

static Ports<1,LFO> _ports{{{
    Port<LFO>("freq:f","log,0.001,10", parf(&LFO::freq)),

}}};

mPorts *LFO::ports = &_ports;

void LFO::dispatch(msg_t m)
{
    _ports.dispatch(m,this);
}
