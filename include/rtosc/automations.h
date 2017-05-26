#include <rtosc/ports.h>
#include <rtosc/rtosc.h>
#include <cassert>
namespace rtosc {
struct AutomationMapping
{
    //0 - linear
    //1 - log
    int   control_scale;

    //0 - simple linear (only first four control points are used)
    //1 - piecewise linear
    int   control_type;

    float *control_points;
    int    npoints;
    int    upoints;
};

struct Automation
{
    //If automation is allocated to anything or not
    bool used;

    //If automation is used or not
    bool active;

    //relative or absolute
    bool relative;

    //Cached infomation
    float       param_base_value;
    const char *param_path;
    char        param_type;
    float       param_min;
    float       param_max;
    float       param_step;       //resolution of parameter. Useful for:
                                  //- integer valued controls
    AutomationMapping map;
};

struct AutomationSlot
{
    //If automation slot has active automations or not
    bool  active;

    //True if a new MIDI binding is being learned
    bool  learning;

    //-1 or a valid MIDI CC + MIDI Channel
    int   midi_cc;

    //Current state supplied by MIDI value or host 
    float current_state;

    //Collection of automations
    Automation *automations;
};

class AutomationMgr
{
    public:
        AutomationMgr(int slots, int per_slot, int control_points);
        ~AutomationMgr(void);

        /**
         * Create an Automation binding
         *
         * - Assumes that each binding takes a new automation slot unless learning
         *   a macro
         * - Can trigger a MIDI learn (recommended for standalone and not
         *   recommended for plugin modes)
         */
        void createBinding(int slot, const char *path, bool start_midi_learn);

        //Get/Set Automation Slot values 0..1
        void setSlot(int slot_id, float value);
        void setSlotSub(int slot_id, int sub, float value);
        float getSlot(int slot_id);

        bool handleMidi(int channel, int cc, int val);

        void set_ports(const class Ports &p);

        void set_instance(void *v);

        void simpleSlope(int slot, int au, float slope, float offset);


        AutomationSlot *slots;
        int per_slot;
        struct AutomationMgrImpl *impl;
        const rtosc::Ports *p;
        void *instance;
};
AutomationMgr::AutomationMgr(int slots, int per_slot, int control_points)
    :per_slot(per_slot)
{
    this->slots = new AutomationSlot[slots];
    memset(this->slots, 0, sizeof(AutomationSlot)*slots);
    for(int i=0; i<slots; ++i) {
        this->slots[i].automations = new Automation[per_slot];
        memset(this->slots[i].automations, 0, sizeof(Automation)*per_slot);
        for(int j=0; j<per_slot; ++j) {
            this->slots[i].automations[j].map.control_points = new float[control_points];
            this->slots[i].automations[j].map.npoints = control_points;
        }
    }

}
AutomationMgr::~AutomationMgr(void)
{
}

void AutomationMgr::createBinding(int slot, const char *path, bool start_midi_learn)
{
    const Port *port = p->apropos(path);
    assert(port);
    auto meta = port->meta();
    if(!(meta.find("min") && meta.find("max"))) {
        fprintf(stderr, "No bounds for '%s' known\n", path);
        return;
    }
    int ind = -1;
    for(int i=0; i<per_slot; ++i) {
        if(slots[slot].automations[i].used == false) {
            ind = i;
            break;
        }
    }

    slots[slot].automations[ind].used = true;
    slots[slot].automations[ind].param_min = atof(meta["min"]);
    slots[slot].automations[ind].param_max = atof(meta["max"]);
    slots[slot].automations[ind].param_type = 'i';
    slots[slot].automations[ind].param_path = path;

    slots[slot].automations[ind].map.upoints = 4;

    if(start_midi_learn)
        slots[slot].learning = true;

};

void AutomationMgr::setSlot(int slot_id, float value) 
{
    for(int i=0; i<per_slot; ++i)
        setSlotSub(slot_id, i, value);
}

void AutomationMgr::setSlotSub(int slot_id, int par, float value)
{
    if(slots[slot_id].automations[par].used == false)
        return;
    const char *path = slots[slot_id].automations[par].param_path;
    float mn = slots[slot_id].automations[par].param_min;
    float mx = slots[slot_id].automations[par].param_max;

    char type = slots[slot_id].automations[par].param_type;

    char msg[128] = {0};
    if(type == 'i') {
        rtosc_message(msg, 128, path, "f", value*(mx-mn) + mn);
    }
    RtData d;
    char loc[256];
    d.loc = loc;
    d.loc_size = 256;
    d.obj = instance;
    p->dispatch(msg, d, true);
};
//        float getSlot(int slot_id);
//
bool AutomationMgr::handleMidi(int channel, int cc, int val)
{
    int ccid = channel*128 + cc;
    if(slots[0].learning) {
        slots[0].learning = false;
        slots[0].midi_cc = ccid;
    }
    if(slots[0].midi_cc == ccid)
        setSlot(0, val/127.0);
    return 0;
}

void AutomationMgr::set_ports(const class Ports &p_) {
    p = &p_;
};
//
//        AutomationSlot *slots;
//        struct AutomationMgrImpl *impl;
//};
void AutomationMgr::set_instance(void *v)
{
    this->instance = v;
}

void AutomationMgr::simpleSlope(int slot, int au, float slope, float offset)
{
}
};
