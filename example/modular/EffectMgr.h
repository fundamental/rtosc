#pragma once
namespace rtosc{class mPorts;};

struct EffectMgr
{
    union {
        struct Effect *eff;
        struct Echo   *echo;
        struct LFO    *lfo;
    };
    EffectMgr(void);
    ~EffectMgr(void);
    void dispatch(const char *m);
    static class rtosc::mPorts *ports;
};
