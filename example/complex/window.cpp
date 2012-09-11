#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Dial.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/fl_draw.H>

#include <string>
#include <cstring>
#include <cmath>
#include <sstream>
#include <map>

using std::string;
using std::map;

map<string, Fl_Widget*> gui_map;

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
    Fl_Knob(int x, int y, int w, int h, string _path)
        :Fl_Dial(x,y,w,h,NULL)

    {
        std::stringstream strstr(_path);
        getline(strstr,path, ':');
        getline(strstr,fn, ':');
        string tmp;
        strstr >> min;
        getline(strstr,tmp, ':');
        strstr >> max;
        getline(strstr,tmp, ':');
        
        getline(strstr,type, ':');

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

    void cb(void)
    {
        const float val = translate(fn.c_str(),min,max,value());
        gui_message(path.c_str(), "f", val);
        printf("%s f %f\n", path.c_str(), val);

        label_str = string_cast<float,string>(val) + type;
        label("                ");
        label(label_str.c_str());
    }
    static void _cb(Fl_Widget *w, void *v)
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
    Fl_Center_Knob(int x, int y, int w, int h, string path)
        :Fl_Knob(x,y,w,h,path)
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
    Fl_Click_Knob(int x, int y, int w, int h, string path)
        :Fl_Knob(x,y,w,h,path)
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
    Fl_Square<T>(int x, int y, int w, int h, int _pad, string name)
        :Fl_Group(x,y,w,h,NULL), pad(_pad)
    {
        const int l = min(max(w-2*pad,0),max(h-2*pad,0));
        t = new T(x+(w-l)/2,y+(h-l)/2,l,l, name.c_str());
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

        //Make controls
        new Fl_Square<Fl_Center_Knob>(x+1*W,y+20,W,H,15,N+"av:1:-1:1:p");
        new Fl_Square<Fl_Center_Knob>(x+2*W,y+20,W,H,15,N+"dv:1:-1:1:p");
        new Fl_Square<Fl_Center_Knob>(x+3*W,y+20,W,H,15,N+"sv:1:-1:1:p");
        new Fl_Square<Fl_Center_Knob>(x+4*W,y+20,W,H,15,N+"rv:1:-1:1:p");
        
        new Fl_Square<Fl_Click_Knob>(x+1*W,y+20+H,W,H,15,N+"at:10^:0.001:10:s");
        new Fl_Square<Fl_Click_Knob>(x+2*W,y+20+H,W,H,15,N+"dt:10^:0.001:10:s");
        new Fl_Square<Fl_Click_Knob>(x+3*W,y+20+H,W,H,15,N+"st:10^:0.001:10:s");
        new Fl_Square<Fl_Click_Knob>(x+4*W,y+20+H,W,H,15,N+"rt:10^:0.001:10:s");
        end();
        resizable(new Fl_Box(x+W,y+20,4*W,2*H));
    }
};

const char *gui_read(void);
void gui_dispatch(const char *msg)
{
    puts("Got GUI Event...");
}

void osc_init(void);
void audio_init(void);
int main()
{
    osc_init();
    audio_init();
    Fl::scheme("plastic");
    Fl_Window *win = new Fl_Double_Window(400, 600, "Oscillation");
    ADSR_Pane *ampl_env = new ADSR_Pane(0,0,400,200, "/amp/env/");
    ADSR_Pane *freq_env = new ADSR_Pane(0,200,400,200, "/frq/env/");
    ampl_env->box(FL_DOWN_BOX);
    freq_env->box(FL_DOWN_BOX);

    //Trigger
    Fl_Button *b=new Fl_Toggle_Button(0,400,400,50,"Gate");
    Fl_Hor_Slider *s = new Fl_Hor_Slider(0, 460, 400, 50, "Frequency");
    s->callback([](Fl_Widget*w,void*v){gui_message("/freq", "f", Fl_Knob::translate("10^", 10, 1000, static_cast<Fl_Slider*>(w)->value()));});
    b->callback([](Fl_Widget*w,void*v){gui_message("/gate", static_cast<Fl_Button*>(w)->value()? "T" : "F");}, NULL);

    win->resizable(new Fl_Box(0,0,400,400));
    win->show();
    while(1) {
        Fl::wait(0.1f);
        while(const char *msg = gui_read())
            gui_dispatch(msg);
    }

    return 0;
}
