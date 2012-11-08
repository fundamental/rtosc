#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Dial.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/Fl_Tree.H>
#include <FL/fl_draw.H>
#include <thread-link.h>

#include <string>
#include <cstring>
#include <cmath>
#include <sstream>
#include <map>

#include "synth.h"

using std::string;
using std::map;

extern ThreadLink<1024,1024> bToU;
extern ThreadLink<1024,1024> uToB;

struct Fl_Knob;
map<string, Fl_Knob*> gui_map;

struct Fl_Grid : public Fl_Tile
{
    Fl_Grid(int x, int y, int w, int h, const char *L=0)
        :Fl_Tile(x,y,w,h,L){};
    void resize(int x, int y, int w, int h)
    {
        Fl_Group::resize(x, y, w, h);
    };
};

template<typename T>
T max(const T &a, const T &b)
{
    return a>b?a:b;
}

template<typename T>
T min(const T &a, const T &b)
{
    return b>a?a:b;
}

template<typename A, typename B>
B string_cast(const A &a)
{
    std::stringstream s;
    s.precision(3);
    B b;
    s << a;
    s >> b;
    return b;
}


struct Fl_Padding : public Fl_Group
{
    Fl_Padding(int x, int y, int w, int h, int _wp, int _hp, Fl_Widget *_w)
        :Fl_Group(x,y,w,h,NULL), hp(_hp), wp(_wp), widget(_w)
    {
        widget->resize(x+wp,y+hp,max(w-2*wp,0),max(h-2*hp,0));
        end();
    }

    void resize(int x, int y, int w, int h)
    {
        widget->resize(x+wp,y+hp,max(w-2*wp,0),max(h-2*hp,0));
    }

    int hp, wp;

    Fl_Widget *widget;
};

void gui_message(const char *path, const char *type, ...);

struct Fl_Knob : public Fl_Dial
{
    Fl_Knob(int x, int y, int w, int h, string _path, const _Port *port)
        :Fl_Dial(x,y,w,h,NULL)

    {
        std::stringstream path_ss(_path+port->name);
        getline(path_ss,path, ':');
        uToB.write(path.c_str(),"");

        std::stringstream scale_ss(port->metadata);
        getline(scale_ss,fn, ':');
        char delim;
        scale_ss >> min >> delim >> max;
        type = "s";

        callback(_cb);

        gui_map[path] = this;
    };

    static float translate(const char *fn, float min, float max, float x)
    {
        if(!strcmp(fn, "10^")) {
            const float b = log(min)/log(10);
            const float a = log(max)/log(10)-b;
            return powf(10.0f, a*x+b);
        } else if(!strcmp(fn, "1")) {
            const float b = min;
            const float a = max-min;
            return a*x+b;
        }
        return 0;
    }

    void setVal(float v)
    {
        float val = 0.0;
        if(fn == "10^") {
            const float b = log(min)/log(10);
            const float a = log(max)/log(10)-b;
            val = (log(v)/log(10)-b)/a;
        } else if(fn == "1") {
            const float b = min;
            const float a = max-min;
            val = (v-b)/a;
        }
        this->value(val);
        label_str = string_cast<float,string>(v) + type;
        label("                ");
        label(label_str.c_str());
    }

    void cb(void)
    {
        puts("callback...");
        const float val = translate(fn.c_str(),min,max,value());
        uToB.write(path.c_str(), "f", val);
        //printf("%s f %f\n", path.c_str(), val);

        label_str = string_cast<float,string>(val) + type;
        label("                ");
        label(label_str.c_str());
    }
    static void _cb(Fl_Widget *w, void *)
    {
        Fl_Knob *k= static_cast<Fl_Knob*>(w);
        k->cb();
    }

    string label_str;
    string path;
    string fn;
    string type;
    float min, max;
};

struct Fl_Center_Knob : public Fl_Knob
{
    Fl_Center_Knob(int x, int y, int w, int h, string path, const _Port *port)
        :Fl_Knob(x,y,w,h,path,port)
    {
    };

    void draw(void)
    {
        Fl_Knob::draw();
        fl_color(0,100,0);
        fl_polygon(x()+w()/2-w()/8, y(), x()+w()/2+w()/8, y(), x()+w()/2, y()-h()/8);
    }
};

struct Fl_Click_Knob : public Fl_Knob
{
    Fl_Click_Knob(int x, int y, int w, int h, string path, const _Port *port)
        :Fl_Knob(x,y,w,h,path, port)
    {
    };

    void draw(void)
    {
        Fl_Knob::draw();
        fl_circle(x(),y()+7*h()/8,h()/8);
    }
};

template<typename T>
struct Fl_Square : public Fl_Group
{
    Fl_Square<T>(int x, int y, int w, int h, int _pad, string name, const _Port *port)
        :Fl_Group(x,y,w,h,NULL), pad(_pad)
    {
        const int l = min(max(w-2*pad,0),max(h-2*pad,0));
        t = new T(x+(w-l)/2,y+(h-l)/2,l,l, name.c_str(), port);
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


struct ADSR_Pane : public Fl_Group
{
    ADSR_Pane(int x, int y, int w, int h, string N)
        :Fl_Group(x,y,w,h,NULL)
    {
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

        const char ports[2][4][3] = {
            {"av","dv","sv",  "rv"},
            {"at","dt","\0\0","rt"}
        };

        //Make controls
        for(int i=0; i<2; ++i)
            for(int j=0; j<4; ++j)
                if(ports[i][j][0])
                    new Fl_Square<Fl_Center_Knob>(x+(j+1)*W,y+20+i*H,W,H,15,
                            N, Adsr::ports[ports[i][j]]);
        end();
        resizable(new Fl_Box(x+W,y+20,4*W,2*H));
    }
};


void traverse_tree(const _Ports *p, std::string prefix="/")
{
    for(unsigned i=0; i<p->nports(); ++i) {
        const _Port &port = p->port(i);
        if(index(port.name,'/'))
            traverse_tree(port.ports, prefix+port.name);
        else
            printf("%s\n", (prefix+port.name).c_str());
    }
}

extern _Ports *root_ports;
const _Ports *subtree_lookup(const _Ports *p, std::string s)
{
    if(s=="")
        return p;

    for(unsigned i = 0; i < p->nports(); ++i) {
        const _Port &port = p->port(i);
        const char *name  = port.name;
        if(index(name,'/') && s.substr(0,(index(name,'/')-name)+1) == std::string(name).substr(0,strlen(name)-1)){
            return subtree_lookup(port.ports, s.substr(index(name,'/')-name));
        }
    }

    //TODO else case
    return p;
}

void audio_init(void);
int main()
{
    audio_init();
    Fl::scheme("plastic");
    Fl_Window *win = new Fl_Double_Window(400, 600, "Oscillation");
    ADSR_Pane *ampl_env = new ADSR_Pane(0,0,400,200, "/amp-env/");
    ADSR_Pane *freq_env = new ADSR_Pane(0,200,400,200, "/frq-env/");
    ampl_env->box(FL_DOWN_BOX);
    freq_env->box(FL_DOWN_BOX);

    //Trigger
    Fl_Button *b=new Fl_Toggle_Button(0,400,400,50,"Gate");
    Fl_Hor_Slider *s = new Fl_Hor_Slider(0, 460, 400, 50, "Frequency");
    win->end();
    s->callback([](Fl_Widget*w,void*){uToB.write("/freq", "f", Fl_Knob::translate("10^", 10, 1000, static_cast<Fl_Slider*>(w)->value()));});
    b->callback([](Fl_Widget*w,void*){uToB.write("/gate", static_cast<Fl_Button*>(w)->value()? "T" : "F");}, NULL);

    win->resizable(new Fl_Box(0,0,400,400));
    win->show();

    Fl_Window *midi_win = new Fl_Double_Window(400, 400, "Midi connections");
    Fl_Tree *tree = new Fl_Tree(0,0,400,400);
    tree->root_label("");
    tree->add("nil");
    tree->add("/nil/nil");
    tree->close(tree->first());
    tree->callback([](Fl_Widget*w,void*){
            Fl_Tree *t=(Fl_Tree*)w;
            int reason = t->callback_reason();
            printf("%d\n", reason);
            char pathname[1024];
            t->item_pathname(pathname, sizeof(pathname), t->callback_item());
            printf("%s\n", pathname);
            if(reason==1) {
                char *colon = index(pathname, ':');
                if(colon) {
                    *colon = 0;
                    puts(pathname);
                    uToB.write("/learn", "s", pathname);
                }
            }

            if(reason==3) //Populate fields
            {
                const _Ports *p=subtree_lookup(root_ports,pathname+1);
                if(auto *i = t->find_item((std::string(pathname)+"/"+"nil").c_str()))
                    t->remove(i);
                for(unsigned i=0; i<p->nports(); ++i) {
                    if(index(p->port(i).name,'/'))
                        t->add((std::string(pathname)+"/"+p->port(i).name+"/"+"nil").c_str());
                    t->close(t->add((std::string(pathname)+"/"+p->port(i).name).c_str()));
                }

            }
            },NULL);
    midi_win->show();
    //Traverse possible ports
    //puts("<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    //traverse_tree(root_ports,"/");
    //puts(">>>>>>>>>>>>>>>>>>>>>>>>>>>>");


    while(win->visible())
    {
        while(bToU.hasNext()) {
            const char *msg = bToU.read();
            //Only floats to known GUI controls are currently received
            gui_map[msg]->setVal(rtosc_argument(msg,0).f);
        }
        Fl::wait(0.01f);
    }
}
