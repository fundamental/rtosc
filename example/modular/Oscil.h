#pragma once
namespace rtosc{class mPorts;};

struct Oscillator
{
    float freq;
    void dispatch(const char *m);
    static rtosc::mPorts *ports;
};
