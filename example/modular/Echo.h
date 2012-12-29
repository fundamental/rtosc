#include "Effect.h"

struct Echo : public Effect
{
    float time;//sec
    void dispatch(const char *m);
    static class rtosc::mPorts *ports;
};
