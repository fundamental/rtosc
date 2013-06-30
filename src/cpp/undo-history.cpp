#include <vector>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <rtosc/rtosc.h>
#include <rtosc/undo-history.h>

class UndoHistoryImpl
{
    public:
        std::vector<const char *> history;
        long history_pos;
        std::function<void(const char*)> cb;

        void rewind(const char *msg);
        void replay(const char *msg);
};

UndoHistory::UndoHistory(void)
{
    impl = new UndoHistoryImpl;
    impl->history_pos = 0;
}

void UndoHistory::recordEvent(const char *msg)
{
    //TODO account for when you have traveled back in time.
    //while this could result in another branch of history, the simple method
    //would be to kill off any future redos when new history is recorded
    
    size_t len = rtosc_message_length(msg, -1);
    char *data = new char[len];
    memcpy(data, msg, len);
    impl->history.push_back(data);
    impl->history_pos++;
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
    printf("value is going to be '%d'\n", arg.i);

    printf("'%s'\n", rtosc_argument(msg, 0).s);
    rtosc_amessage(tmp, 256, rtosc_argument(msg,0).s,
            rtosc_argument_string(msg)+2,
            &arg);
    printf("tmp => '%s'\n", tmp);
    printf("tmp => '%s'\n", rtosc_argument_string(tmp));
    printf("tmp => '%d'\n", rtosc_argument(tmp, 0).i);

    puts("rewind...");
    cb(tmp);
}

void UndoHistoryImpl::replay(const char *msg)
{
    printf("replay...'%s'\n", msg);
    rtosc_arg_t arg = rtosc_argument(msg,2);
    printf("value is going to be '%d'\n", arg.i);

    printf("replay address: '%s'\n", rtosc_argument(msg, 0).s);
    int len = rtosc_amessage(tmp, 256, rtosc_argument(msg,0).s,
            rtosc_argument_string(msg)+2,
            &arg);
    
    for(int i=0; i<len; ++i)
        printf("%hx",tmp[i]);
    printf("\n");

    printf("replay => '%s'\n", tmp);
    printf("replay => '%s'\n", rtosc_argument_string(tmp));
    printf("replay => '%d'\n", rtosc_argument(tmp, 0).i);

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
