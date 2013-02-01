#include "Synth.h"
#include "Echo.h"
#include <stdio.h>
#include <rtosc/rtosc.h>
#include <rtosc/ports.h>

int main()
{
    Synth synth;
    synth.oscil.freq = 10.0;

    synth.effects[2].eff = new Echo();


    char buffer[100];
    //simple event
    rtosc_message(buffer,100,"volume","f", 0.37);
    Synth::ports.dispatch(NULL, 0, buffer, &synth);

    //simple nested event
    rtosc_message(buffer,100,"oscil/freq","f", 1002.3);
    Synth::ports.dispatch(NULL, 0, buffer, &synth);

    //received event
    rtosc_message(buffer,100,"effect2/echo/time", "f", 8.0);
    Synth::ports.dispatch(NULL, 0, buffer, &synth);

    //discarded event
    rtosc_message(buffer,100,"effect2/lfo/freq", "f", 2.0);
    Synth::ports.dispatch(NULL, 0, buffer, &synth);

    //discarded event
    rtosc_message(buffer,100,"effect0/lfo/freq", "f", 2.0);
    Synth::ports.dispatch(NULL, 0, buffer, &synth);


    printf("synth.oscil.freq            = %f\n", synth.oscil.freq);
    printf("synth.volume                = %f\n", synth.volume);
    printf("synth.effects[2].echo->time = %f\n",synth.effects[2].echo->time);

    return 0;
}
