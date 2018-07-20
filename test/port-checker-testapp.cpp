#include <set>
#include <string>
#include <iostream>

#include <lo/lo.h>
#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>

//! @file port-checker-testapp.cpp
//! Application that is used when port-checker-tester checks the port-checker
//! Many similarities to MiddleWare, but we don't have an RT thread

static void liblo_error_cb(int i, const char *m, const char *loc)
{
    std::cerr << "liblo :-( " << i << "-" << m << "@" << loc << std::endl;
}

static int handle_st(const char *path, const char *types, lo_arg **argv,
                     int argc, lo_message msg, void *data);

class port_checker_tester
{
    class pc_data_obj : public rtosc::RtData
    {
            port_checker_tester* pc;
            int buffersize = 4*4096;
        public:
            pc_data_obj(port_checker_tester* pc) : pc(pc)
            {
                loc_size = 1024;
                loc = new char[loc_size];
                memset(loc, 0, loc_size);
                buffer = new char[buffersize];
                memset(buffer, 0, buffersize);
                obj       = pc;
                forwarded = false;
            }

            ~pc_data_obj(void)
            {
                delete[] loc;
                delete[] buffer;
            }

            virtual void reply(const char *path, const char *args, ...) override
            {
                //std::cout << "reply building" << path << std::endl;
                va_list va;
                va_start(va,args);
                rtosc_vmessage(buffer,4*4096,path,args,va);
                reply(buffer);
                va_end(va);
            }
            virtual void replyArray(const char *path, const char *args,
                                    rtosc_arg_t *argd) override
            {
                //std::cout << "reply building" << path << std::endl;
                rtosc_amessage(buffer,4*4096,path,args,argd);
                reply(buffer);
            }

            //! reply to the sender
            virtual void reply(const char *msg) override{
                pc->send_to_current_remote(msg, buffersize);
            }

            //! broadcast to all senders
            virtual void broadcast(const char *msg) override {
                //std::cout << "broadcast building" << msg << std::endl;
                pc->broadcast_to_remote(msg, buffersize);
            }

            virtual void chain(const char *msg) override
            {
                assert(false);
            }

            virtual void chain(const char *path, const char *args, ...) override
            {
                assert(false);
            }

            virtual void forward(const char *) override
            {
                assert(false);
            }

            bool forwarded;
        private:
            char *buffer;
            pc_data_obj *mwi;
    };

    lo_server srv;
    char recv_buffer[2048];
    std::string last_url;
    std::set<std::string> known_remotes;

    friend int handle_st(const char *path, const char *types, lo_arg **argv,
                         int argc, lo_message msg, void *data);
    void on_recv(const char* path, const char*, lo_arg**, int, lo_message msg);
    void on_recv(const char* msg);
    void send_to_remote(const char *rtmsg, int msgsize, const char* dest);
    void send_to_current_remote(const char* rtmsg, int msgsize);
    void broadcast_to_remote(const char* rtmsg, int msgsize);
    bool alive = true;

public:

    int rdefault_without_rparameter = 0;
    int roption_without_ics = 0;
    int double_rdefault = 0;
    int double_rpreset = 0;
    int rpreset_not_in_roptions = 0;
    int perfect_param_1 = 0;
    int duplicate_mapping = 0;
    int no_rdefault = 0;
    int bundle_size[16];
    int perfect_param_2[16];
    int perfect_param_3 = 0;
    int invalid_rdefault;
    int duplicate_param;

    void add_url(const char* url);
    void run();

    port_checker_tester(const char *preferred_port);
};

#define rObject port_checker_tester
static const rtosc::Ports test_ports = {
    {"echo:ss", 0, 0, [](const char* msg, rtosc::RtData& d) {
        const char* type = rtosc_argument(msg, 0).s;
        const char* url = rtosc_argument(msg, 1).s;
        d.reply("/echo", "ss", type, url);
    }},
    {"duplicate_mapping::i:S", rOptions(same, same), NULL,
        rParamICb(duplicate_mapping)},
    {"rdefault_without_rparameter_A::i", rDefault(0), NULL,
        rParamICb(rdefault_without_rparameter)},
    {"invalid_rdefault::i", rProp(parameter) rDefault($$$), NULL,
        rParamICb(invalid_rdefault)},
    {"rdefault_without_rparameter_B::i", rPresets(0,1,2), NULL,
        rParamICb(rdefault_without_rparameter)},
    {"no_query::i", rProp(parameter) rDefault(0), nullptr,
        [](const char* msg, rtosc::RtData& d) {} },
    {"no_reply_A::i", rProp(parameter) rDefault(0), NULL,
        [](const char* msg, rtosc::RtData& d) {
            const char *args = rtosc_argument_string(msg);
            if(!*args) { d.reply(d.loc, "i", 0); }
        }
    },
    {"no_reply_B::i", rProp(parameter) rDefault(0), NULL,
        [](const char* msg, rtosc::RtData& d) {
            const char *args = rtosc_argument_string(msg);
            if(!*args) {
                d.reply(d.loc, "i", 0);
            } else {
                int var = rtosc_argument(msg, 0).i;
                d.reply("/undo_change", "s" "i" "i", d.loc,
                        (var==0) ? 1 : 0, var);
                // reply is missing here
            }
        }
    },
    {"no_broadcast::i", rProp(parameter) rDefault(0), NULL,
        [](const char* msg, rtosc::RtData& d) {
            const char *args = rtosc_argument_string(msg);
            if(!*args) {
                d.reply(d.loc, "i", 0);
            } else {
                int var = rtosc_argument(msg, 0).i;
                d.reply("/undo_change", "s" "i" "i", d.loc,
                        (var==0) ? 1 : 0, var);
                d.reply(d.loc, "i", var); // this should be d.broadcast...
            }
        }
    },
    {"roption_without_ics::i:c", rProp(enumerated) rProp(parameter) rDefault(0),
        nullptr, rOptionCb(roption_without_ics)
    },
    rParamI(no_rdefault, ""),
    rParamI(double_rdefault, rDefault(0) rDefault(0), ""),
    rParamI(double_rpreset, rPreset(0, 1), rPreset(1, 0), rPreset(0, 1), ""),
    rArrayI(bundle_size, 16, rDefault([15x2]), ""),
    rOption(rpreset_not_in_roptions, rOptions(one, two, three),
            rPresetsAt(2, one, does_not_exist, one), rDefault(two), ""),

    rParamI(duplicate_param, rNoDefaults, ""),
    rParamI(duplicate_param, rNoDefaults, ""),

    rOption(perfect_param_1, rOptions(one, two, three),
            rPresetsAt(2, one, three, one), rDefault(two), ""),
    rArrayI(perfect_param_2, 16, rDefault([1 2 3...]), ""),
    rParamI(perfect_param_3, rNoDefaults, ""), // no rDefault, but that's OK here
    // correct enabled-condition
    rEnabledCondition(not_enabled, false),
    // invalid enabled-condition: returns integer
    {"not_enabled_bad:", rProp(internal), NULL,
        [](const char* , rtosc::RtData& d){d.reply(d.loc, "i", 42); }},
    // bad parameter, but does not occur since it is disabled:
    {"invisible_param::i", rEnabledByCondition(not_enabled), NULL,
     [](const char* , rtosc::RtData& ){}},
    {"enabled_port_not_existing::i", rEnabledByCondition(not_existing), NULL,
     [](const char* , rtosc::RtData& ){}},
    {"enabled_port_bad_reply::i", rEnabledByCondition(not_enabled_bad), NULL,
     [](const char* , rtosc::RtData& ){}}
};
#undef rObject

int handle_st(const char *path, const char *types, lo_arg **argv,
                     int argc, lo_message msg, void *data)
{
    static_cast<port_checker_tester*>(data)->on_recv(path, types,
                                                     argv, argc, msg);
    return 0;
}

int main(int argc, char** argv)
{
    const char* preferred_port = (argc >= 2) ? argv[1] : nullptr;
    port_checker_tester tester(preferred_port);
    tester.run();

    return EXIT_SUCCESS;
}


void port_checker_tester::on_recv(const char *path, const char *,
                                  lo_arg **, int, lo_message msg)
{
    lo_address addr = lo_message_get_source(msg);
    if(addr) {
        const char *tmp = lo_address_get_url(addr);
        if(tmp != last_url) {
            add_url(last_url.c_str());
            //mw->transmitMsg("/echo", "ss", "OSC_URL", tmp);
            last_url = tmp;
        }
        free((void*)tmp);
    }

    // TODO: here and in MiddleWare.cpp: memset() required for liblo?
    memset(recv_buffer, 0, sizeof(recv_buffer));
    lo_message_serialise(msg, path, recv_buffer, nullptr);
    on_recv(recv_buffer);
}

void port_checker_tester::on_recv(const char *msg) {
    if(!strcmp(msg, "/path-search") &&
            !strcmp("ss", rtosc_argument_string(msg)))
    {

        char buffer[1024*20];
        std::size_t length =
            rtosc::path_search(test_ports, msg, 128, buffer, sizeof(buffer));
        if(length) {
            lo_message msg  = lo_message_deserialise((void*)buffer,
                                                     length, NULL);
            lo_address addr = lo_address_new_from_url(last_url.c_str());
            if(addr)
                lo_send_message(addr, buffer, msg);
            lo_address_free(addr);
            lo_message_free(msg);
        }
    }
    else if(msg[0]=='/' && strrchr(msg, '/')[1]) {
        pc_data_obj d(this);
        test_ports.dispatch(msg, d, true);
        //std::cout << "matches: " << d.matches << std::endl;
    }
}

void port_checker_tester::send_to_remote(const char *rtmsg, int msgsize,
                                         const char *dest)
{
    //std::cout << "send_to_remote: " << rtmsg << " -> " << dest << std::endl;
    if(!rtmsg || rtmsg[0] != '/' || !rtosc_message_length(rtmsg, -1)) {
        std::cout << "[Warning] Invalid message in sendToRemote <" << rtmsg
                  << ">..." << std::endl;
        return;
    }

    if(*dest) {
        size_t len = rtosc_message_length(rtmsg, msgsize);
        lo_message msg  = lo_message_deserialise((void*)rtmsg, len, nullptr);
        if(!msg) {
            std::cout << "[ERROR] OSC to <" << rtmsg
                      << "> Failed To Parse In Liblo" << std::endl;
            return;
        }

        //Send to known url
        lo_address addr = lo_address_new_from_url(dest);
        if(addr)
            lo_send_message(addr, rtmsg, msg);
        lo_address_free(addr);
        lo_message_free(msg);
    }
}

void port_checker_tester::send_to_current_remote(const char *rtmsg,
                                                 int msgsize) {
    //std::cout << "sending to last url: " << last_url << std::endl;
    send_to_remote(rtmsg, msgsize, last_url.c_str());
}

void port_checker_tester::broadcast_to_remote(const char *rtmsg, int msgsize) {
    for(auto rem:known_remotes)
        send_to_remote(rtmsg, msgsize, rem.c_str());
}

void port_checker_tester::run()
{
    while(alive)
    {
        int n = lo_server_recv_noblock(srv, 1000 /* 1000 ms = 1 second */);
        (void)n;
        // message will be dispatched to the server's callback
    }
}

void port_checker_tester::add_url(const char *url) {
    known_remotes.emplace(url);
}

port_checker_tester::port_checker_tester(const char* preferred_port)
{
    srv = lo_server_new_with_proto(preferred_port, LO_UDP, liblo_error_cb);
    if(srv == nullptr)
        throw std::runtime_error("Could not create lo server");
    lo_server_add_method(srv, NULL, NULL, handle_st, this);
    std::cerr << "lo server running on " << lo_server_get_port(srv)
              << std::endl;
}

