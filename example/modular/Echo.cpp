#include <rtosc/ports.h>
#include "Echo.h"
#include "util.h"

using namespace rtosc;

static Ports<1,Echo> _ports{{{
    Port<Echo>("time:f","log,0.001,10", parf(&Echo::time)),

}}};

mPorts *Echo::ports = &_ports;

void Echo::dispatch(msg_t m)
{
    _ports.dispatch(m,this);
}
