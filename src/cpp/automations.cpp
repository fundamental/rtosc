#include <rtosc/automations.h>

using namespace rtosc;

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

    auto &au = slots[slot].automations[ind];

    au.used = true;
    au.param_min = atof(meta["min"]);
    au.param_max = atof(meta["max"]);
    au.param_type = 'i';
    au.param_path = path;

    au.map.upoints = 2;
    au.map.control_points[0] = 0;
    au.map.control_points[1] = au.param_min;
    au.map.control_points[2] = 1;
    au.map.control_points[3] = au.param_max;

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
    auto &au = slots[slot_id].automations[par];
    if(au.used == false)
        return;
    const char *path = au.param_path;
    float mn = au.param_min;
    float mx = au.param_max;

    float a  = au.map.control_points[1];
    float b  = au.map.control_points[3];

    char type = au.param_type;

    char msg[128] = {0};
    if(type == 'i') {
        //printf("[%f..%f] rather than [%f..%f]\n", b, a, mx, mn);
        float v = value*(b-a) + a;
        if(v > mx)
            v = mx;
        else if(v < mn)
            v = mn;

        rtosc_message(msg, 128, path, "f", v);
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

void AutomationMgr::simpleSlope(int slot_id, int par, float slope, float offset)
{
    auto &map = slots[slot_id].automations[par].map;
    map.upoints = 2;
    map.control_points[0] = 0;
    map.control_points[1] = -(slope/2)+offset;
    map.control_points[2] =  1;
    map.control_points[3] =  slope/2+offset;

}
