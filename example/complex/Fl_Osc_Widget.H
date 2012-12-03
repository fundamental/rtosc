#pragma once
#include <string>
#include <cstring>
#include <cmath>
#include <FL/Fl_Group.H>
#include "Fl_Osc_Pane.H"

class Fl_Osc_Widget
{
    public:
        Fl_Osc_Widget(std::string nname, std::string metadata)
            :path(dynamic_cast<Fl_Osc_Group*>(Fl_Group::current()) ?
                    dynamic_cast<Fl_Osc_Group*>(Fl_Group::current())->pane_name : "/"),
            name(nname,0,nname.find_first_of(":")),
            full_path(path+name),metadata(metadata)
        {}
        ~Fl_Osc_Widget(void){};

        virtual void OSC_value(float) {}
        virtual void OSC_value(bool) {}
        virtual void OSC_value(int) {}
        virtual void OSC_value(unsigned,void*) {}

        std::string path;
        std::string name;
        std::string full_path;
        std::string metadata;

        class Fl_Osc_Interface *osc;
    protected:
        //translations
        static float translate(float x, const char *meta)
        {
            //Gather type
            char shape[4] = {0};
            unsigned pos  = 0;
            while(*meta && *meta != ',' && pos < 3)
                shape[pos++] = *meta++;


            //Gather args
            while(*meta && *meta!=',') meta++; meta++;
            float min = atof(meta);
            while(*meta && *meta!=',') meta++; meta++;
            float max = atof(meta);


            //Translate
            if(!strcmp("lin",shape))
                return x*(max-min)+min;
            else if(!strcmp("log", shape)) {
                const float b = log(min);
                const float a = log(max)-b;
                return expf(a*x+b);
            }

            return 0.0f;
        }

        static float inv_translate(float x, const char *meta)
        {
            //Gather type
            char shape[4] = {0};
            unsigned pos  = 0;
            while(*meta && *meta != ',' && pos < 3)
                shape[pos++] = *meta++;


            //Gather args
            while(*meta && *meta!=',') meta++; meta++;
            float min = atof(meta);
            while(*meta && *meta!=',') meta++; meta++;
            float max = atof(meta);


            //Translate
            if(!strcmp("lin",shape))
                return (x-min)/(max-min);
            else if(!strcmp("log", shape)) {
                const float b = log(min);
                const float a = log(max)-b;
                return (log(x)-b)/a;
            }

            return 0.0f;
        }
};
