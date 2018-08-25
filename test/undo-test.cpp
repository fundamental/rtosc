#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>
#include <rtosc/undo-history.h>
#include <cstdarg>

#include "common.h"

using namespace rtosc;

class Object
{
    public:
        Object(void)
            :x(0),b(0),i(0)
        {}
        float x;
        char b;
        int i;
};


#define rObject Object

Ports ports = {
    rParam(b, "b"),
    rParamF(x, "x"),
    rParamI(i, "i"),
};

char reply_buf[256];
struct Rt:public RtData
{
    Rt(Object *o, UndoHistory *uh_)
    {
        memset(reply_buf, 0, sizeof(reply_buf));
        loc      = new char[128];
        memset(loc, 0, 128);
        loc_size = 128;
        obj      = o;
        uh       = uh_;
        enable   = true;
    }
    ~Rt(void)
    {
        delete [] loc;
    }
    void reply(const char *path, const char *args, ...)
    {
        //printf("# reply <%s, %s>\n", path, args);
        if(strcmp(path, "/undo_change") || !enable)
            return;

        va_list va;
        va_start(va, args);
        rtosc_vmessage(reply_buf, sizeof(reply_buf),
                path, args, va);
        uh->recordEvent(reply_buf);
        va_end(va);
    }

    bool enable;
    UndoHistory *uh;
};

char ref[] = "b\0\0\0,c\0\0\0\0\0\7";

char message_buff[256];
int main()
{
    memset(message_buff, 0, sizeof(reply_buf));
    //Initialize structures
    Object o;
    UndoHistory hist;
    Rt rt(&o, &hist);
    hist.setCallback([&rt](const char*msg) {ports.dispatch(msg+1, rt);});

    assert_int_eq(0, o.b, "Verify Zero Based Initialization", __LINE__);

    rt.matches = 0;
    int len = rtosc_message(message_buff, 128, "b", "c", 7);
    assert_hex_eq(ref, message_buff, sizeof(ref)-1, len,
            "Build Param Change Message", __LINE__);

    ports.dispatch(message_buff, rt);
    assert_int_eq(1, rt.matches,
            "Verify A Single Leaf Dispatch Has Occured", __LINE__);
    assert_int_eq(7, o.b, "Verify State Change Has Been Applied", __LINE__);

    rt.enable = false;

    printf("#Current History:\n");
    hist.showHistory();
    hist.seekHistory(-1);
    assert_int_eq(0, o.b,
            "Verify Undo Has Returned To Zero-Init State", __LINE__);
    printf("#Current History:\n");
    hist.showHistory();

    hist.seekHistory(+1);
    assert_int_eq(7, o.b,
            "Verify Redo Has Returned To Altered State", __LINE__);

    return test_summary();
}


