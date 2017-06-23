#include <rtosc/automations.h>

using namespace rtosc;

AutomationMgr::AutomationMgr(int slots, int per_slot, int control_points)
    :nslots(slots), per_slot(per_slot), active_slot(0), p(NULL), learn_queue_len(0), damaged(0)
{
    this->slots = new AutomationSlot[slots];
    memset(this->slots, 0, sizeof(AutomationSlot)*slots);
    for(int i=0; i<slots; ++i) {
        sprintf(this->slots[i].name, "Slot %d", i);
        this->slots[i].midi_cc  = -1;
        this->slots[i].learning = -1;

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
    assert(p);
    const Port *port = p->apropos(path);
    if(!port) {
        fprintf(stderr, "[Zyn:Error] port '%s' does not exist\n", path);
        return;
    }
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

    slots[slot].used = true;

    auto &au = slots[slot].automations[ind];

    au.used   = true;
    au.active = true;
    au.param_min = atof(meta["min"]);
    au.param_max = atof(meta["max"]);
    au.param_type = 'i';
    if(strstr(port->name, ":f"))
        au.param_type = 'f';
    strncpy(au.param_path, path, sizeof(au.param_path));

    au.map.upoints = 2;
    au.map.control_points[0] = 0;
    au.map.control_points[1] = au.param_min;
    au.map.control_points[2] = 1;
    au.map.control_points[3] = au.param_max;

    if(start_midi_learn && slots[slot].learning == -1 && slots[slot].midi_cc == -1)
        slots[slot].learning = ++learn_queue_len;

    damaged = true;

};

void AutomationMgr::setSlot(int slot_id, float value)
{
    for(int i=0; i<per_slot; ++i)
        setSlotSub(slot_id, i, value);

    slots[slot_id].current_state = value;
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

    char msg[256] = {0};
    if(type == 'i') {
        //printf("[%f..%f] rather than [%f..%f]\n", b, a, mx, mn);
        float v = value*(b-a) + a;
        if(v > mx)
            v = mx;
        else if(v < mn)
            v = mn;

        rtosc_message(msg, 256, path, "i", (int)v);
    } else if(type == 'f') {
        float v = value*(b-a) + a;
        if(v > mx)
            v = mx;
        else if(v < mn)
            v = mn;

        rtosc_message(msg, 256, path, "f", v);
    } else if(type == 'T' || type == 'F') {
        float v = value*(b-a) + a;
        if(v > 0.5)
            v = 1.0;
        else
            v = 0.0;

        rtosc_message(msg, 256, path, v == 1.0 ? "T" : "F");
    } else
        return;

    if(backend)
        backend(msg);
}

float AutomationMgr::getSlot(int slot_id)
{
    return slots[slot_id].current_state;
}


void AutomationMgr::setName(int slot_id, const char *msg)
{
    strncpy(slots[slot_id].name, msg, sizeof(slots[slot_id].name));
    damaged = 1;
}
const char *AutomationMgr::getName(int slot_id)
{
    return slots[slot_id].name;
}
bool AutomationMgr::handleMidi(int channel, int cc, int val)
{
    int ccid = channel*128 + cc;

    bool bound_cc = false;
    for(int i=0; i<nslots; ++i) {
        if(slots[i].midi_cc == ccid) {
            bound_cc = true;
            setSlot(i, val/127.0);
        }
    }

    if(bound_cc)
        return 1;

    //No bound CC, now to see if there's something to learn
    for(int i=0; i<nslots; ++i) {
        if(slots[i].learning == 1) {
            slots[i].learning = -1;
            slots[i].midi_cc = ccid;
            for(int j=0; j<nslots; ++j)
                if(slots[j].learning > 1)
                    slots[j].learning -= 1;
            learn_queue_len--;
            damaged = 1;
            break;
        }
    }
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

int AutomationMgr::free_slot(void) const
{
    for(int i=0; i<nslots; ++i)
        if(!slots[i].used)
            return i;
    return -1;
}
