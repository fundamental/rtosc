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

    // 10 rounds are enough to have 1 discontiguous write
    for (int round = 0; round < 10; ++round)
    {
        // write 17 bytes + 3 bytes padding + 4 bytes argstr + 4 bytes args = 28 bytes
        thread_link.write(portname, "i", 43 + round);
        read_msg = thread_link.read();

        char test_name [] = "round 0";
        test_name[6] += round % 10;
        verify_msg(read_msg, portname, 42 + round, test_name, __LINE__);
    }
}

void test_read_lookahead()
{
    // max 4 messages of each 32 -> 128 bytes size
    rtosc::ThreadLink thread_link(32,4);
    char portname[] = "abcdefghijklmnop"; // length 16, array size 17
    rtosc::msg_t read_msg;

    // write five messages, causing the buffer to wrap around, thus testing
    // that the lookahead read works under this circumstance as well
    // message length:
    // 17 bytes + 3 bytes padding + 4 bytes argstr + 4 bytes args = 28 bytes
    // first message is not actually used in test, it's just there to
    // cause the buffer to wrap around.
    thread_link.write(portname, "i", 41);
    // we can still verify that it reads back properly
    assert_true(thread_link.hasNext(), "Initial message has next", __LINE__);
    read_msg = thread_link.read();
    verify_msg(read_msg, portname, 41, "Initial message read", __LINE__);
    assert_false(thread_link.hasNext(), "Initial message empty queue", __LINE__);

    // Now for the writes that we are going to use for the tests
    thread_link.write(portname, "i", 42);
    thread_link.write(portname, "i", 43);
    thread_link.write(portname, "i", 44);
    thread_link.write(portname, "i", 45);

    // Verify there is something in buffer, then read lookahead queue
    // until nothing left to read
    assert_true(thread_link.hasNext(), "1: Has next", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "1: Has next lookahead [1]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 42, "1: Lookahead [1]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "1: Has next lookahead [2]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 43, "1: Lookahead [2]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "1: Has next lookahead [3]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 44, "1: Lookahead [3]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "1: Has next lookahead [4]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 45, "1: Lookahead [4]", __LINE__);

    assert_false(thread_link.hasNextLookahead(), "1: Has no lookahead next", __LINE__);

    // Actually read first message, verify it, then read lookahead queue
    // until nothing left to read.
    assert_true(thread_link.hasNext(), "2: Has next", __LINE__);
    read_msg = thread_link.read();
    verify_msg(read_msg, portname, 42, "2: Read [1]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "2: Has next lookahead [1]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 43, "2: Lookahead [2]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "2: Has next lookahead [2]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 44, "2: Lookahead [3]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "2: Has next lookahead [3]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 45, "2: Lookahead [4]", __LINE__);

    assert_false(thread_link.hasNextLookahead(), "2: Has no lookahead next", __LINE__);

    // Read two messages, verify them, then read lookahead queue
    // until nothing left to read.
    assert_true(thread_link.hasNext(), "3: Has next", __LINE__);
    read_msg = thread_link.read();
    verify_msg(read_msg, portname, 43, "3: Read [1]", __LINE__);

    assert_true(thread_link.hasNext(), "3: Has next", __LINE__);
    read_msg = thread_link.read();
    verify_msg(read_msg, portname, 44, "3: Read [1]", __LINE__);

    assert_true(thread_link.hasNextLookahead(), "3: Has next lookahead [1]", __LINE__);
    read_msg = thread_link.read_lookahead();
    verify_msg(read_msg, portname, 45, "3: Lookahead [1]", __LINE__);

    assert_false(thread_link.hasNextLookahead(), "3: Has no lookahead next", __LINE__);

    // Read final message, verify it, then verify lookahead queue is empty,
    // and read queue as well.
    assert_true(thread_link.hasNext(), "4: Has next", __LINE__);
    read_msg = thread_link.read();
    verify_msg(read_msg, portname, 45, "4: Read [1]", __LINE__);

    assert_false(thread_link.hasNextLookahead(), "4: Has no lookahead next", __LINE__);

    assert_false(thread_link.hasNext(), "4: Has no next", __LINE__);
}

int main()
{
    test_contiguous_write();
    test_read_lookahead();

    return test_summary();
}

