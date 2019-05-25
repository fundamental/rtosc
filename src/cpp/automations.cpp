#include "../util.h"
#include <rtosc/automations.h>
#include <cstring>
#include <cmath>

using namespace rtosc;

AutomationMgr::AutomationMgr(int slots, int per_slot, int control_points)
    :nslots(slots), per_slot(per_slot), active_slot(0), learn_queue_len(0), p(NULL), damaged(0)
{
    this->slots = new AutomationSlot[slots];
    memset(this->slots, 0, sizeof(AutomationSlot)*slots);
    for(int i=0; i<slots; ++i) {
        auto &s = this->slots[i];
        sprintf(s.name, "Slot %d", i + 1);
        s.midi_cc  = -1;
        s.midi_nrpn  = -1;
        s.learning = -1;

        s.automations = new Automation[per_slot];
        memset(s.automations, 0, sizeof(Automation)*per_slot);
        for(int j=0; j<per_slot; ++j) {
            s.automations[j].map.control_points = new float[control_points];
            s.automations[j].map.npoints = control_points;
            s.automations[j].map.gain   = 100.0;
            s.automations[j].map.offset = 0.0;
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
        if(!strstr(port->name, ":T")) {
            fprintf(stderr, "No bounds for '%s' known\n", path);
            return;
        }
    }
    if(meta.find("internal") || meta.find("no learn")) {
        fprintf(stderr, "[Warning] port '%s' is unlearnable\n", path);
        return;
    }
    int ind = -1;
    for(int i=0; i<per_slot; ++i) {
        if(slots[slot].automations[i].used == false) {
            ind = i;
            break;
        }
    }

    if(ind == -1)
        return;

    slots[slot].used = true;

    auto &au = slots[slot].automations[ind];

    au.used   = true;
    au.active = true;
    au.param_type = 'i';
    if(strstr(port->name, ":f"))
        au.param_type = 'f';
    else if(strstr(port->name, ":T"))
        au.param_type = 'T';
    if(au.param_type == 'T') {
        au.param_min = 0.0;
        au.param_max = 1.0;
    } else {
        au.param_min = atof(meta["min"]);
        au.param_max = atof(meta["max"]);
    }
    fast_strcpy(au.param_path, path, sizeof(au.param_path));

    if(meta["scale"] && strstr(meta["scale"], "log")) {
        au.map.control_scale = 1;
        au.param_min = logf(au.param_min);
        au.param_max = logf(au.param_max);
    } else
        au.map.control_scale = 0;

    au.map.gain   = 100.0;
    au.map.offset = 0;
    updateMapping(slot, ind);

    if(start_midi_learn && slots[slot].learning == -1 && slots[slot].midi_cc == -1)
        slots[slot].learning = ++learn_queue_len;

    damaged = true;

};

void AutomationMgr::updateMapping(int slot_id, int sub)
{
    if(slot_id >= nslots || slot_id < 0 || sub >= per_slot || sub < 0)
        return;


    auto &au = slots[slot_id].automations[sub];

    float mn = au.param_min;
    float mx = au.param_max;
    float center = (mn+mx)*(0.5 + au.map.offset/100.0);
    float range  = (mx-mn)*au.map.gain/100.0;

    au.map.upoints = 2;
    au.map.control_points[0] = 0;
    au.map.control_points[1] = center-range/2.0;
    au.map.control_points[2] = 1;
    au.map.control_points[3] = center+range/2.0;
}

void AutomationMgr::setSlot(int slot_id, float value)
{
    if(slot_id >= nslots || slot_id < 0)
        return;
    for(int i=0; i<per_slot; ++i)
        setSlotSub(slot_id, i, value);

    slots[slot_id].current_state = value;
}

void AutomationMgr::setSlotSub(int slot_id, int par, float value)
{
    if(slot_id >= nslots || slot_id < 0 || par >= per_slot || par < 0)
        return;
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
        float v = value*(b-a) + a;
        if(v > mx)
            v = mx;
        else if(v < mn)
            v = mn;

        rtosc_message(msg, 256, path, "i", (int)roundf(v));
    } else if(type == 'f') {
        float v = value*(b-a) + a;
        if(v > mx)
            v = mx;
        else if(v < mn)
            v = mn;

        if(au.map.control_scale == 1)
            v = expf(v);

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
    if(slot_id >= nslots || slot_id < 0)
        return 0.0;
    return slots[slot_id].current_state;
}


void AutomationMgr::clearSlot(int slot_id)
{
    if(slot_id >= nslots || slot_id < 0)
        return;
    auto &s = slots[slot_id];
    s.active = false;
    s.used   = false;
    if(s.learning)
        learn_queue_len--;
    for(int i=0; i<nslots; ++i)
        if(slots[i].learning > s.learning)
            slots[i].learning--;
    s.learning = -1;
    s.midi_cc  = -1;
    s.midi_nrpn  = -1;
    s.current_state = 0;
    memset(s.name, 0, sizeof(s.name));
    sprintf(s.name, "Slot %d", slot_id + 1);
    for(int i=0; i<per_slot; ++i)
        clearSlotSub(slot_id, i);
    damaged = true;
}

void AutomationMgr::clearSlotSub(int slot_id, int sub)
{
    if(slot_id >= nslots || slot_id < 0 || sub >= per_slot || sub < 0)
        return;
    auto &a = slots[slot_id].automations[sub];
    a.used    = false;
    a.active  = false;
    a.relative = false;
    a.param_base_value = false;
    memset(a.param_path, 0, sizeof(a.param_path));
    a.param_type = 0;
    a.param_min  = 0;
    a.param_max  = 0;
    a.param_step = 0;
    a.map.gain   = 100;
    a.map.offset = 0;

    damaged = true;
}

void AutomationMgr::setSlotSubPath(int slot, int ind, const char *path)
{
    if(slot >= nslots || slot < 0)
        return;

	assert(p);
    const Port *port = p->apropos(path);
    if(!port) {
        fprintf(stderr, "[Zyn:Error] port '%s' does not exist\n", path);
        return;
    }
    auto meta = port->meta();
    if(!(meta.find("min") && meta.find("max"))) {
        if(!strstr(port->name, ":T")) {
            fprintf(stderr, "No bounds for '%s' known\n", path);
            return;
        }
    }
    if(meta.find("internal") || meta.find("no learn")) {
        fprintf(stderr, "[Warning] port '%s' is unlearnable\n", path);
        return;
    }

    slots[slot].used = true;

    auto &au = slots[slot].automations[ind];

    au.used   = true;
    au.active = true;
    au.param_type = 'i';
    if(strstr(port->name, ":f"))
        au.param_type = 'f';
    else if(strstr(port->name, ":T"))
        au.param_type = 'T';
    if(au.param_type == 'T') {
        au.param_min = 0.0;
        au.param_max = 1.0;
    } else {
        au.param_min = atof(meta["min"]);
        au.param_max = atof(meta["max"]);
    }
    fast_strcpy(au.param_path, path, sizeof(au.param_path));

    if(meta["scale"] && strstr(meta["scale"], "log")) {
        au.map.control_scale = 1;
        au.param_min = logf(au.param_min);
        au.param_max = logf(au.param_max);
    } else
        au.map.control_scale = 0;

    updateMapping(slot, ind);
	damaged = true;

}

void  AutomationMgr::setSlotSubGain(int slot_id, int sub, float f)
{
    if(slot_id >= nslots || slot_id < 0 || sub >= per_slot || sub < 0)
        return;
    auto &m = slots[slot_id].automations[sub].map;
    m.gain = f;
}
float AutomationMgr::getSlotSubGain(int slot_id, int sub)
{
    if(slot_id >= nslots || slot_id < 0 || sub >= per_slot || sub < 0)
        return 0.0;
    auto &m = slots[slot_id].automations[sub].map;
    return m.gain;
}
void  AutomationMgr::setSlotSubOffset(int slot_id, int sub, float f)
{
    if(slot_id >= nslots || slot_id < 0 || sub >= per_slot || sub < 0)
        return;
    auto &m = slots[slot_id].automations[sub].map;
    m.offset = f;
}
float AutomationMgr::getSlotSubOffset(int slot_id, int sub)
{
    if(slot_id >= nslots || slot_id < 0 || sub >= per_slot || sub < 0)
        return 0.0;
    auto &m = slots[slot_id].automations[sub].map;
    return m.offset;
}

void AutomationMgr::setName(int slot_id, const char *msg)
{
    if(slot_id >= nslots || slot_id < 0)
        return;
    fast_strcpy(slots[slot_id].name, msg, sizeof(slots[slot_id].name));
    damaged = 1;
}
const char *AutomationMgr::getName(int slot_id)
{
    if(slot_id >= nslots || slot_id < 0)
        return "";
    return slots[slot_id].name;
}
bool AutomationMgr::handleMidi(int channel, int type, int val)
{
    bool is_nrpn = false;
    int par_id;
    int value;
    if((type == C_dataentryhi) || (type == C_dataentrylo)
       || (type == C_nrpnhi) || (type == C_nrpnlo)) { //Process RPN and NRPN by the Master (ignore the chan)
        
        setparameternumber(type, val);

        int parhi = -1, parlo = -1, valhi = -1, vallo = -1;
        if(getnrpn(&parhi, &parlo, &valhi, &vallo) == 0){ //this is NRPN
            is_nrpn = true;
            par_id = (parhi<<7) + parlo;
            value = (valhi<<7) + vallo;
            bool bound_nrpn = false;
            for(int i=0; i<nslots; ++i) {
                if(slots[i].midi_nrpn == par_id) {
                    bound_nrpn = true;
                    setSlot(i, value/16383.0);
                }
            }

        if(bound_nrpn)
            return 1;
        }
        
    }
    else {
        
        par_id = channel*128 + type;

        bool bound_cc = false;
        for(int i=0; i<nslots; ++i) {
            if(slots[i].midi_cc == par_id) {
                bound_cc = true;
                setSlot(i, val/127.0);
            }
        }

        if(bound_cc)
            return 1;
        
    }

    //No bound CC, now to see if there's something to learn
    for(int i=0; i<nslots; ++i) {
        if(slots[i].learning == 1) {
            slots[i].learning = -1;
            if (is_nrpn)
                slots[i].midi_nrpn = par_id;
            else
                slots[i].midi_cc = par_id;
            for(int j=0; j<nslots; ++j)
                if(slots[j].learning > 1)
                    slots[j].learning -= 1;
            learn_queue_len--;
            setSlot(i, val/127.0);
            damaged = 1;
            break;
        }
    }
    return 0;
}

//Returns 0 if there is NRPN or 1 if there is not
int AutomationMgr::getnrpn(int *parhi, int *parlo, int *valhi, int *vallo)
{
    if((NRPN.parhi < 0) || (NRPN.parlo < 0) || (NRPN.valhi < 0)
       || (NRPN.vallo < 0))
        return 1;

    *parhi = NRPN.parhi;
    *parlo = NRPN.parlo;
    *valhi = NRPN.valhi;
    *vallo = NRPN.vallo;
    return 0;
}

void AutomationMgr::setparameternumber(unsigned int type, int value)
{
    switch(type) {
        case C_nrpnhi:
            NRPN.parhi = value;
            NRPN.valhi = -1;
            NRPN.vallo = -1; //clear the values
            break;
        case C_nrpnlo:
            NRPN.parlo = value;
            NRPN.valhi = -1;
            NRPN.vallo = -1; //clear the values
            break;
        case C_dataentryhi:
            if((NRPN.parhi >= 0) && (NRPN.parlo >= 0))
                NRPN.valhi = value;
            break;
        case C_dataentrylo:
            if((NRPN.parhi >= 0) && (NRPN.parlo >= 0))
                NRPN.vallo = value;
            break;
    }
}

void AutomationMgr::set_ports(const struct Ports &p_) {
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
    if(slot_id >= nslots || slot_id < 0 || par >= per_slot || par < 0)
        return;
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
