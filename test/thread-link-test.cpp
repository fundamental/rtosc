#include "common.h"
#include <string>

#include <rtosc/thread-link.h>

void verify_msg(rtosc::msg_t read_msg, const char *portname, int value, const char *test_id, int line)
{
    const std::string test_string = test_id;

    assert_str_eq(portname, read_msg, (test_string + " (portname)").c_str(), line);
    const char* arg_str = rtosc_argument_string(read_msg);
    assert_str_eq("i", arg_str, (test_string + " (arg type)").c_str(), line);
    if(!strcmp(arg_str, "i"))
        assert_int_eq(value, rtosc_argument(read_msg, 0).i, (test_string + " (arg value)").c_str(), line);
}

void test_contiguous_write()
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

        char test_name [] = "round 0";
        test_name[6] += round % 10;
        verify_msg(read_msg, portname, 42, test_name, __LINE__);
    }
}

int main()
{
    test_contiguous_write();

    return test_summary();
}

