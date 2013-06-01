/*
 * Copyright (c) 2012 Mark McCurry
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <rtosc/ports.h>
#include <array>
#include <string.h>
#include <algorithm>

namespace rtosc {

#define RTOSC_INVALID_MIDI 255
template<int len>
struct MidiAddr
{
    MidiAddr(void):ch(RTOSC_INVALID_MIDI),ctl(RTOSC_INVALID_MIDI){}
    //The midi values that map to the specified action
    uint8_t ch, ctl;

    //The type of the event 'f', 'i', 'T', 'c'
    char type;
    //The path of the event
    char path[len];
    //The conversion function for 'f' types
    char conversion[len];
};


/**
 * Table of midi mappings
 */
template<int len, int elms>
struct MidiTable
{
    std::array<MidiAddr<len>,elms> table;

    Ports &dispatch_root;
    short unhandled_ch;
    short unhandled_ctl;
    char  unhandled_path[len];
    
    void (*error_cb)(const char *, const char *);
    void (*event_cb)(const char *);

    MidiTable(Ports &_dispatch_root)
        :dispatch_root(_dispatch_root), unhandled_ch(-1), unhandled_ctl(-1),
        error_cb(MidiTable::black_hole2), event_cb(MidiTable::black_hole1)
    {
        memset(unhandled_path, 0, sizeof(unhandled_path));
    }


    bool has(uint8_t ch, uint8_t ctl) const
    {
        for(auto e:table) {
            if(e.ch == ch && e.ctl == ctl)
                return true;
        }
        return false;
    }
    
    MidiAddr<len> *get(uint8_t ch, uint8_t ctl)
    {
        for(auto &e:table)
            if(e.ch==ch && e.ctl == ctl)
                return &e;
        return NULL;
    }

    const MidiAddr<len> *get(uint8_t ch, uint8_t ctl) const
    {
        for(auto &e:table)
            if(e.ch==ch && e.ctl == ctl)
                return &e;
        return NULL;
    }

    bool mash_port(MidiAddr<len> &e, const Port &port)
    {
        const char *args = index(port.name, ':');

        //Consider a path to be typed based upon the argument restrictors
        if(index(args, 'f')) {
            e.type = 'f';
            memset(e.conversion, 0, sizeof(e.conversion));
            //Take care of mapping
            const char *start = port.metadata,
                       *end   = index(start,':');

            if((end-start)<len)
                std::copy(start, end, e.conversion);
            else
                return false;
        } else if(index(args, 'i'))
            e.type = 'i';
        else if(index(args, 'T'))
            e.type = 'T';
        else if(index(args, 'c'))
            e.type = 'c';
        else
            return false;
        return true;
    }

    void addElm(uint8_t ch, uint8_t ctl, const char *path)
    {
        const Port *port = dispatch_root.apropos(path);

        if(!port || port->ports) {//missing or directory node
            error_cb("Bad path", path);
            return;
        }

        if(MidiAddr<len> *e = this->get(ch,ctl)) {
            strncpy(e->path,path,len);
            if(!mash_port(*e, *port)) {
                e->ch  = RTOSC_INVALID_MIDI;
                e->ctl = RTOSC_INVALID_MIDI;
                error_cb("Failed to read metadata", path);
            }
            return;
        }

        for(MidiAddr<len> &e:table) {
            if(e.ch == RTOSC_INVALID_MIDI) {//free spot
                e.ch  = ch;
                e.ctl = ctl;
                strncpy(e.path,path,len);
                if(!mash_port(e, *port)) {
                    e.ch  = RTOSC_INVALID_MIDI;
                    e.ctl = RTOSC_INVALID_MIDI;
                    error_cb("Failed to read metadata", path);
                }
                return;
            }
        }
    }

    void check_learn(void)
    {
        if(unhandled_ctl == RTOSC_INVALID_MIDI || unhandled_path[0] == '\0')
            return;
        addElm(unhandled_ch, unhandled_ctl, unhandled_path);
        unhandled_ch = unhandled_ctl = RTOSC_INVALID_MIDI;
        memset(unhandled_path, 0, sizeof(unhandled_path));
    }

    void learn(const char *s)
    {
        if(strlen(s) > len) {
            error_cb("String too long", s);
            return;
        }
        strcpy(unhandled_path, s);
        check_learn();
    }

    void process(uint8_t ch, uint8_t ctl, uint8_t val)
    {
        const MidiAddr<len> *addr = get(ch,ctl);
        if(!addr) {
            unhandled_ctl = ctl;
            unhandled_ch  = ch;
            check_learn();
            return;
        }

        char buffer[1024];
        switch(addr->type)
        {
            case 'f':
                rtosc_message(buffer, 1024, addr->path,
                        "f", translate(val,addr->conversion));
                break;
            case 'i':
                rtosc_message(buffer, 1024, addr->path,
                        "i", val);
                break;
            case 'T':
                rtosc_message(buffer, 1024, addr->path,
                        (val<64 ? "F" : "T"));
                break;
            case 'c':
                rtosc_message(buffer, 1024, addr->path,
                        "c", val);
        }

        event_cb(buffer);
    }

    static float translate(uint8_t val, const char *meta)
    {
        //Allow for middle value to be set
        float x = val!=64.0 ? val/127.0 : 0.5;

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
    static void black_hole2(const char *, const char *){}
    static void black_hole1(const char *){}

};

};
