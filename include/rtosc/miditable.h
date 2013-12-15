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
#include <string.h>
#include <algorithm>

namespace rtosc {

struct MidiAddr
{
    //The midi values that map to the specified action
    uint8_t ch, ctl;

    //The type of the event 'f', 'i', 'T', 'c'
    char  type;
    //The path of the event
    char *path;
    //The conversion function for 'f' types
    const char *conversion;
};


/**
 * Table of midi mappings
 */
class MidiTable
{
    public:

        Ports  &dispatch_root;
        short  unhandled_ch;
        short  unhandled_ctl;
        char  *unhandled_path;

        void (*error_cb)(const char *, const char *);
        void (*event_cb)(const char *);
        void (*modify_cb)(const char *, const char *, const char *, int, int);

        MidiTable(Ports &_dispatch_root);

        bool has(uint8_t ch, uint8_t ctl) const;

        MidiAddr *get(uint8_t ch, uint8_t ctl);

        const MidiAddr *get(uint8_t ch, uint8_t ctl) const;

        bool mash_port(MidiAddr &e, const Port &port);

        void addElm(uint8_t ch, uint8_t ctl, const char *path);

        void check_learn(void);

        void learn(const char *s);

        void clear_entry(const char *s);

        void process(uint8_t ch, uint8_t ctl, uint8_t val);

        Port learnPort(void);
        Port unlearnPort(void);
        Port registerPort(void);

        //TODO generalize to an addScalingFunction() system
        static float translate(uint8_t val, const char *meta);

    private:
        class MidiTable_Impl *impl;
};

};
