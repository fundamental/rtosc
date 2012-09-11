#include <math.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include "osc.h"

extern float Fs;
extern float freq;
extern bool  gate;
float sample(void);
int current_note = 0;

void dsp_dispatch(const char *msg);
//void dsp_dispatch(const char *msg)
//{
//    dispatch_t *itr = dispatch_table;
//    while(itr->label && strcmp(msg, itr->label))
//        ++itr;
//    if(itr->label)
//        itr->fn(msg, itr->data);
//}


//JACK stuff
jack_port_t   *port, *iport;
jack_client_t *client;
int process(unsigned nframes, void *args);
void audio_init(void)
{
    client = jack_client_open("rtosc-demo2", JackNullOption, NULL, NULL);
    if(!client)
        errx(1, "jack_client_open() failure");

    jack_set_process_callback(client, process, NULL);

    port = jack_port_register(client, "output",
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput | JackPortIsTerminal, 0);

    //Make midi port
    iport = jack_port_register(client, "input",
            JACK_DEFAULT_MIDI_TYPE, JackPortIsInput | JackPortIsTerminal, 0);

    if(!port)
        errx(1, "jack_port_register() failure");

    if(jack_activate(client))
        errx(1, "jack_activate() failure");
}

void stop_synth(void)
{
    jack_client_close(client);
}

int process(unsigned nframes, void *args)
{
    Fs = jack_get_sample_rate(client);

    //Handle midi first
    void *midi_buf = jack_port_get_buffer(iport, nframes);
    jack_midi_event_t ev;
    jack_nframes_t event_idx = 0;
    while(jack_midi_event_get(&ev, midi_buf, event_idx++) == 0) {
        switch(ev.buffer[0]&0xf0) {
            case 0x90: //Note On
                freq = 440.0f * powf(2.0f, (ev.buffer[1]-69.0f)/12.0f);
                current_note = ev.buffer[1];
                gate = 1;
                break;
            case 0x80: //Note Off
                if(current_note == ev.buffer[1])
                    current_note = gate =0;
                break;
            case 0xB0: //Controller
                printf("Control %d %d\n", ev.buffer[1], ev.buffer[2]);
                break;
        }
    }
    
    //Handle OSC Queue
    while(const char *msg = dsp_read())
        dsp_dispatch(msg);

    //Get all samples
    float *smps = (float*) jack_port_get_buffer(port, nframes);
    for(int i=0; i<nframes; ++i)
        smps[i] = sample();

    return 0;
}
