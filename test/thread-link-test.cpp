#include "common.h"

#include <rtosc/thread-link.h>

int main()
{
    // max 4 messages of each 32 -> 128 bytes size
    rtosc::ThreadLink thread_link(32,4);
    char portname[] = "abcdefghijklmnop"; // length 16, array size 17
    rtosc::msg_t read_msg;

    // always write one message ahead, to check that a second message
    // does not corrupt the "message under test"
    thread_link.write(portname, "i", 42);

    // 10 rounds are enough to have 1 discontigous write
    for (int round = 0; round < 10; ++round)
    {
        // write 17 bytes + 3 bytes padding + 4 bytes argstr + 4 bytes args = 28 bytes
        thread_link.write(portname, "i", 42);
        read_msg = thread_link.read();

        char test_name [] = "round 0 string equals";
        test_name[6] += round % 10;
        assert_str_eq(portname, read_msg, test_name, __LINE__);

        const char* arg_str = rtosc_argument_string(read_msg);
        assert_str_eq("i", arg_str, "arg string correct", __LINE__);
        if(!strcmp(arg_str, "i"))
            assert_int_eq(42, rtosc_argument(read_msg, 0).i, "arg 1 correct", __LINE__);
        else {
            // invalid state, don't crash the test
        }
    }

    return test_summary();
}

