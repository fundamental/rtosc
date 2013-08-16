#include <vector>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <ctime>
#include <rtosc/rtosc.h>
#include <rtosc/undo-history.h>

class UndoHistoryImpl
{
    public:
        std::vector<const char *> history;
        long history_pos;
        std::function<void(const char*)> cb;

        time_t last_ev_time;

        void rewind(const char *msg);
        void replay(const char *msg);
};

UndoHistory::UndoHistory(void)
{
    impl = new UndoHistoryImpl;
    impl->history_pos  = 0;
    impl->last_ev_time = 0;
}

void UndoHistory::recordEvent(const char *msg)
{
    //TODO Properly account for when you have traveled back in time.
    //while this could result in another branch of history, the simple method
    //would be to kill off any future redos when new history is recorded
    if(impl->history.size() != (unsigned) impl->history_pos) {
        impl->history.resize(impl->history_pos);
        impl->last_ev_time = 0;
    }
    
    size_t len = rtosc_message_length(msg, -1);
    char *data = new char[len];
    if(difftime(impl->last_ev_time, time(NULL)) < 2 && // 2 second threshold
            !impl->history.empty() &&
            !strcmp(rtosc_argument(msg,0).s,
                    rtosc_argument(impl->history[impl->history.size()-1],0).s)) {
        //We can splice events together, merging them into one event
        rtosc_arg_t args[3];
        args[0] = rtosc_argument(msg, 0);
        args[1] = rtosc_argument(impl->history[impl->history.size()-1],1);
        args[2] = rtosc_argument(msg, 2);

        rtosc_amessage(data, len, msg, rtosc_argument_string(msg), args);

        delete [] impl->history[impl->history.size()-1];
        impl->history[impl->history.size()-1] = data;
        impl->last_ev_time = time(NULL);
    } else {
        memcpy(data, msg, len);
        impl->last_ev_time = time(NULL);
        impl->history.push_back(data);
        impl->history_pos++;
    }

}

void UndoHistory::showHistory(void) const
{
    int i = 0;
    for(auto s : impl->history)
        printf("#%d type: %s dest: %s arguments: %s\n", i++,
                s, rtosc_argument(s, 0).s, rtosc_argument_string(s));
}

static char tmp[256];
void UndoHistoryImpl::rewind(const char *msg)
{
    memset(tmp, 0, sizeof(tmp));
    printf("rewind('%s')\n", msg);
    rtosc_arg_t arg = rtosc_argument(msg,1);
    rtosc_amessage(tmp, 256, rtosc_argument(msg,0).s,
            rtosc_argument_string(msg)+2,
            &arg);
    cb(tmp);
}

void UndoHistoryImpl::replay(const char *msg)
{
    printf("replay...'%s'\n", msg);
    rtosc_arg_t arg = rtosc_argument(msg,2);
    printf("replay address: '%s'\n", rtosc_argument(msg, 0).s);
    int len = rtosc_amessage(tmp, 256, rtosc_argument(msg,0).s,
            rtosc_argument_string(msg)+2,
            &arg);
    
    if(len)
        cb(tmp);
}

void UndoHistory::seekHistory(int distance)
{
    //TODO print out the events that would need to take place to get to the
    //final destination
    
    //TODO limit the distance to be to applicable sizes
    //ie ones that do not exceed the known history/future
    long dest = impl->history_pos + distance;
    if(dest < 0)
        distance -= dest;
    if(dest > (long) impl->history.size())
        distance  = impl->history.size() - impl->history_pos;
    if(!distance)
        return;
    
    printf("distance == '%d'\n", distance);
    printf("history_pos == '%ld'\n", impl->history_pos);
    //TODO account for traveling back in time
    if(distance<0)
        while(distance++)
            impl->rewind(impl->history[--impl->history_pos]);
    else
        while(distance--)
            impl->replay(impl->history[impl->history_pos++]);
}

void UndoHistory::setCallback(std::function<void(const char*)> cb)
{
    impl->cb = cb;
}
