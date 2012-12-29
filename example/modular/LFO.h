#include "Effect.h"

struct LFO : public Effect
{
    float freq;//Hz
    void dispatch(const char *m);
    static class rtosc::mPorts *ports;
};
