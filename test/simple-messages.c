#include <rtosc/rtosc.h>
#include "common.h"

//                   4      8        12     16
char ref1[] = "/pag" "e/po" "ge\0\0" ",TIF" "\0\0\0";
//                 4     8        12      16         20    24    28    32
char ref2[] = "/tes""ting""\0\0\0\0"",is\0""\0\0\0\x17""this"" str""ing";
char buffer[256];
char buffer2[256];

int main()
{
    memset(buffer2, 0xff, sizeof(buffer2));

    printf("#Suite 1:\n");
    //Check the creation of a simple no arguments message
    int sz = rtosc_message(buffer, 256, "/page/poge", "TIF");
    assert_hex_eq(ref1, buffer, sizeof(ref1), sz,
            "Build A Simple No Allocated Arguments Message", __LINE__);
    assert_int_eq(sizeof(ref1), rtosc_message_length(buffer, 256),
            "Check rtosc_message_length() On Message", __LINE__);

    //Verify that it can be read properly
    assert_int_eq(3, rtosc_narguments(buffer),
            "Check rtosc_narguments() On Message", __LINE__);

    assert_char_eq('T', rtosc_type(buffer, 0), "Check Argument T", __LINE__);
    assert_char_eq('I', rtosc_type(buffer, 1), "Check Argument I", __LINE__);
    assert_char_eq('F', rtosc_type(buffer, 2), "Check Argument F", __LINE__);

    //Check the creation of a more complex message
    printf("#Suite 2:\n");
    sz = rtosc_message(buffer, 256, "/testing", "is", 23, "this string");
    assert_hex_eq(ref2, buffer, sizeof(ref2), sz,
            "Build A Simple With-Allocation Message", __LINE__);

    printf("#Suite 3: misc regression tests\n");
    //Verify that a string argument can be retreived
    rtosc_message(buffer, 256, "/register", "iis", 2, 13, "/amp-env/av");
    assert_str_eq("/amp-env/av",rtosc_argument(buffer,2).s,
            "Verify Basic String Can Be Read", __LINE__);

    //Verify that buffer overflows will not occur
    printf("#checking overflow semantics\n");
    sz = rtosc_message(buffer, 32, "/testing", "is", 23, "this string");
    assert_int_eq(32, sz, "Build Non-Overflowing Message", __LINE__);
    assert_int_eq(32, rtosc_message_length(buffer,256),
            "Check rtosc_message_length()", __LINE__);

    sz = rtosc_message(buffer, 31, "/testing", "is", 23, "this string");
    assert_int_eq(0, sz, "Build Overflowing Message", __LINE__);
    assert_int_eq(0, *buffer,
            "Verify Buffers Are Cleared When Message Overflows", __LINE__);
    assert_int_eq(0, rtosc_message_length(buffer, 256),
            "Check rtosc_message_length()", __LINE__);

    //check simple float retrevial
    printf("#Check Float Args\n");
    float f = 13523.34;
    sz = rtosc_message(buffer, 32, "oscil/freq", "f", f);
    assert_true(sz != 0, "Build Simple Float Message", __LINE__);
    assert_flt_eq(f, rtosc_argument(buffer,0).f, "Check Float Retreival", __LINE__);


    //Simple Char Ret
    printf("#Check simple character retrevial\n");
    sz = rtosc_message(buffer, 256, "/test", "cccc", 0xde,0xad,0xbe,0xef);
    assert_true(sz != 0, "Verify Message Can Be Built", __LINE__);

    assert_int_eq(0xde, rtosc_argument(buffer, 0).i,
            "Check First Char Argument", __LINE__);
    assert_int_eq(0xef, rtosc_argument(buffer, 3).i,
            "Check Last Char Argument", __LINE__);

    //Verify argument retreval for short messages
    sz = rtosc_message(buffer, 256, "/b", "c", 7);
    assert_int_eq(12, sz, "Build Min-Length Allocated Arg Message", __LINE__);
    assert_int_eq(7, rtosc_argument(buffer+1, 0).i,
            "Verify Arg Retreival", __LINE__);

    //Work on a recently found bug
    printf("#Check packed blobs\n");
    sz = rtosc_message(buffer, 256, "m", "bb", 4, buffer2, 1, buffer2);
    assert_int_eq(24, sz, "Build Packed Blob Message", __LINE__);
    assert_int_eq(4,rtosc_argument(buffer, 0).b.len,
            "Check First Blob Length", __LINE__);
    assert_int_eq(1,rtosc_argument(buffer, 1).b.len,
            "Check Second Blob Length", __LINE__);
    return test_summary();
}
