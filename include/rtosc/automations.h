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
};

struct Automation
{
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
        float getSlot(int slot_id);

        bool handleMidi(int channel, int cc, int val);

        AutomationSlot *slots;
        struct AutomationMgrImpl *impl;
};
