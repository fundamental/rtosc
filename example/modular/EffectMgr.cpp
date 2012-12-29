#include "EffectMgr.h"
#include <rtosc/ports.h>
#include "Echo.h"
#include "LFO.h"
#include "util.h"

using namespace rtosc;

static Ports<3,EffectMgr> _ports{{{
    Port<EffectMgr>("echo/", ":o:", Echo::ports, cast<EffectMgr,Echo,Effect*>(&EffectMgr::eff)),
    Port<EffectMgr>("lfo/",  ":o:", LFO::ports,  cast<EffectMgr,LFO,Effect*>(&EffectMgr::eff)),
    Port<EffectMgr>("type",  "", [](msg_t, EffectMgr*){})
}}};

mPorts *EffectMgr::ports = &_ports;

EffectMgr::EffectMgr(void)
    :eff(NULL){}
EffectMgr::~EffectMgr(void)
{
    delete eff;
}

void EffectMgr::dispatch(msg_t m)
{
    _ports.dispatch(m,this);
}
