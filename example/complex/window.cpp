#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include "Fl_Osc_Dial.H"
#include "Fl_Osc_Button.H"
#include "Fl_Osc_Slider.H"
#include "Fl_Osc_Pane.H"
#include "Fl_Osc_Tree.H"
#include "Fl_Osc_Interface.H"

#include <rtosc/thread-link.h>

#include <string>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iostream>
#include <map>

#include "synth.h"
#include "util.h"

using std::string;
using std::map;
using namespace rtosc;

extern ThreadLink bToU;
extern ThreadLink uToB;

struct Fl_Knob;
map<string, Fl_Osc_Widget*> gui_map;

class Osc_Interface : public Fl_Osc_Interface
{
    void createLink(string path, class Fl_Osc_Widget *dial)
    {
        gui_map[path] = dial;
    }

    void requestValue(string path)
    {
        uToB.write(path.c_str(), "");
        printf("request: %s\n", path.c_str());
    }

    void writeValue(string path, float val)
    {
        uToB.write(path.c_str(), "f", val);
        printf("write float: %s\n", path.c_str());
    }

    void writeValue(string path, string val)
    {
        uToB.write(path.c_str(), "s", val.c_str());
        printf("write string: %s\n", path.c_str());
    }

    void writeValue(string path, bool val)
    {
        uToB.write(path.c_str(), val ? "T" : "F");
        printf("write bool: %s\n", path.c_str());
    }

} OSC_API;

template<typename T> T max(const T &a, const T &b) { return a>b?a:b; }
template<typename T> T min(const T &a, const T &b) { return b>a?a:b; } 

struct Fl_Center_Knob : public Fl_Osc_Dial
{
    Fl_Center_Knob(int x, int y, int w, int h, const Port *port)
        :Fl_Osc_Dial(x,y,w,h,port->name,port->metadata)
    {
    };

    void draw(void)
    {
        Fl_Osc_Dial::draw();
        fl_color(0,100,0);
        fl_polygon(x()+w()/2-w()/8, y(), x()+w()/2+w()/8, y(), x()+w()/2, y()-h()/8);
    }
};

template<typename T>
struct Fl_Square : public Fl_Osc_Group
{
    Fl_Square<T>(int x, int y, int w, int h, int _pad, const Port *port)
        :Fl_Osc_Group(x,y,w,h,NULL), pad(_pad)
    {
        const int l = min(max(w-2*pad,0),max(h-2*pad,0));
        t = new T(x+(w-l)/2,y+(h-l)/2,l,l, port);
        end();
    }

    void resize(int x, int y, int w, int h)
    {
        Fl_Group::resize(x,y,w,h);
        const int l = min(max(w-2*pad,0),max(h-2*pad,0));
        t->resize(x+(w-l)/2,y+(h-l)/2,l,l);
    }

    const int pad;

    T *t;
};


struct ADSR_Pane : public Fl_Osc_Group
{
    ADSR_Pane(int x, int y, int w, int h, string N)
        :Fl_Osc_Group(x,y,w,h,NULL)
    {
        pane_name += N;
        const int W = w/5;
        const int H = (h-50)/2;

        begin();
        //Make labels
        new Fl_Box(x+1*W,y+10,W,10,"A");
        new Fl_Box(x+2*W,y+10,W,10,"D");
        new Fl_Box(x+3*W,y+10,W,10,"S");
        new Fl_Box(x+4*W,y+10,W,10,"R");

        new Fl_Box(x,y+20,W,H,"Val");
        new Fl_Box(x,y+20+H,W,H,"Time");

        begin();
        const char ports[2][4][3] = {
            {"av","dv","sv",  "rv"},
            {"at","dt","\0\0","rt"}
        };

        //Make controls
        for(int i=0; i<2; ++i)
            for(int j=0; j<4; ++j)
                if(ports[i][j][0])
                    new Fl_Square<Fl_Center_Knob>(x+(j+1)*W,y+20+i*H,W,H,15,
                            Adsr::ports[ports[i][j]]);

        end();
        resizable(new Fl_Box(x+W,y+20,4*W,2*H));
    }
};


void traverse_tree(const Ports *p)
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    walk_ports(p, buffer, sizeof(buffer),
            [](const Port *, const char *name) {
                printf("%s\n", name);});
}

extern Ports *root_ports;

float translate(float x, const char *meta);

struct Synth_Window : public Fl_Double_Window, public Fl_Osc_Pane
{
    Synth_Window(void)
        :Fl_Double_Window(400,600, "RT-OSC Test Synth")
    {
        osc       = &OSC_API;
        pane_name = "/";

        ADSR_Pane *ampl_env = new ADSR_Pane(0,0,400,200, "amp-env/");
        ADSR_Pane *freq_env = new ADSR_Pane(0,200,400,200, "frq-env/");
        ampl_env->box(FL_DOWN_BOX);
        freq_env->box(FL_DOWN_BOX);

        //Trigger
        Fl_Button *b=new Fl_Osc_Button(0,400,400,50,"gate","");
        b->type(FL_TOGGLE_BUTTON);
        b->label("Gate Switch");
        Fl_Osc_Slider *s = new Fl_Osc_Slider(0, 460, 400, 50,
                "freq", "log,10,1000::Frequency");
        s->type(FL_HOR_SLIDER);

        end();

        resizable(new Fl_Box(0,0,400,400));
    }
    ~Synth_Window(void)
    {}
};


void audio_init(void);
int main()
{
    audio_init();
    //Fl::scheme("plastic");
    Fl_Window *win = new Synth_Window();
    win->show();

    Fl_Window *midi_win = new Fl_Double_Window(400, 400, "Midi connections");
    Fl_Osc_Tree *tree   = new Fl_Osc_Tree(0,0,400,400);
    tree->root_ports    = root_ports;
    tree->osc           = &OSC_API;
    midi_win->show();
    //Traverse possible ports
    puts("<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    traverse_tree(root_ports);
    puts(">>>>>>>>>>>>>>>>>>>>>>>>>>>>");


    while(win->shown())
    {
        while(bToU.hasNext()) {
            const char *msg = bToU.read();
            if(gui_map.find(msg) != gui_map.end()) {
                if(rtosc_narguments(msg) != 1)
                    continue;
                switch(rtosc_type(msg,0)) {
                    case 'f':
                        //Only floats to known GUI controls are currently received
                        gui_map[msg]->OSC_value((float)rtosc_argument(msg,0).f);
                        break;
                }
            }
        }
        Fl::wait(0.01f);
    }
}
