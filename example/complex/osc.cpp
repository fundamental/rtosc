#include <stdarg.h>
#include <stdio.h>
#include "../../include/rtosc.h"
#include <jack/ringbuffer.h>
#define OSC_SIZE (2048)

//Message Queues
jack_ringbuffer_t *to_gui;
jack_ringbuffer_t *to_dsp;

//Static buffers
static char gui_osc_buf[OSC_SIZE];
static char dsp_osc_buf[OSC_SIZE];

void gui_message(const char *path, const char *type, ...)
{
    va_list ap;
    va_start(ap, type);
    vsosc(gui_osc_buf, OSC_SIZE, path, type, ap);
    if(jack_ringbuffer_write_space(to_dsp) >= OSC_SIZE)
        jack_ringbuffer_write(to_dsp, gui_osc_buf, OSC_SIZE);
    else
        puts("to_dsp ringbuffer full...");
}

void dsp_message(const char *path, const char *type, ...)
{
    va_list ap;
    va_start(ap, type);
    vsosc(dsp_osc_buf, OSC_SIZE, path, type, ap);
    if(jack_ringbuffer_write_space(to_gui) >= OSC_SIZE)
        jack_ringbuffer_write(to_gui, dsp_osc_buf, OSC_SIZE);
}

const char *gui_read(void)
{
    if(jack_ringbuffer_read_space(to_gui) >= OSC_SIZE) {
        jack_ringbuffer_read(to_gui, gui_osc_buf, OSC_SIZE);
        return gui_osc_buf;
    }
    return NULL;
}

const char *dsp_read(void)
{
    if(jack_ringbuffer_read_space(to_dsp) >= OSC_SIZE) {
        jack_ringbuffer_read(to_dsp, dsp_osc_buf, OSC_SIZE);
        return dsp_osc_buf;
    }
    return NULL;
}

void osc_init(void)
{
    //Setup Ringbuffers
    to_gui = jack_ringbuffer_create(OSC_SIZE*8);
    to_dsp = jack_ringbuffer_create(OSC_SIZE*32);
    jack_ringbuffer_mlock(to_gui);
    jack_ringbuffer_mlock(to_dsp);
}
