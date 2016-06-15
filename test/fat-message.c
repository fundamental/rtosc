#include <rtosc/rtosc.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

typedef uint8_t midi_t[4];
char buffer[1024];

#define CHECK(a, b, o) \
    arg = rtosc_argument(buffer, o); \
    assert_hex_eq((char*)&a, (char*)&(arg.b), sizeof(a), sizeof(a), \
            "Checking '"#a"' Argument", __LINE__);

//verifies a message with all types included serializes and deserializes
int main()
{
    unsigned message_len;
    int32_t      i = 42;             //integer
    float        f = 0.25;           //float
    const char  *s = "string";       //string
    rtosc_blob_t b = {3,(uint8_t*)s};//blob
    int64_t      h = -125;           //long integer
    uint64_t     t = 22412;          //timetag
    double       d = 0.125;          //double
    const char  *S = "Symbol";       //symbol
    char         c = 25;             //character
    int32_t      r = 0x12345678;     //RGBA
    midi_t       m = {0x12,0x23,     //midi
                      0x34,0x45};
    //true
    //false
    //nil
    //inf

    message_len = rtosc_message(buffer, 1024, "/dest",
                "[ifsbhtdScrmTFNI]", i,f,s,b.len,b.data,h,t,d,S,c,r,m);

    assert_int_eq(96, message_len,
            "Generating A Message With All Arg Types", __LINE__);


    rtosc_arg_t arg;
    assert_int_eq(i, rtosc_argument(buffer, 0).i,
            "Checking 'i' Argument", __LINE__);
    CHECK(f,f,1);
    assert_str_eq(s, rtosc_argument(buffer, 2).s,
            "Checking 's' Argument", __LINE__);
    CHECK(b.len,b.len,3);
    assert_hex_eq((char*)b.data, (char*)arg.b.data, 3, b.len,
            "Checking 'b.data' Argument", __LINE__);
    CHECK(h,h,4);
    CHECK(t,t,5);
    CHECK(d,d,6);
    assert_str_eq(S,rtosc_argument(buffer, 7).s,
            "Checking 'S' Argument", __LINE__);
    assert_char_eq(c,rtosc_argument(buffer, 8).i,
            "Checking 'c' Argument", __LINE__);
    CHECK(r,i,9);
    CHECK(m,m,10);
    assert_char_eq('T', rtosc_type(buffer,11),
            "Checking 'T' Argument", __LINE__);
    assert_char_eq('F', rtosc_type(buffer,12),
            "Checking 'F' Argument", __LINE__);
    assert_char_eq('N', rtosc_type(buffer,13),
            "Checking 'N' Argument", __LINE__);
    assert_char_eq('I', rtosc_type(buffer,14),
            "Checking 'I' Argument", __LINE__);
    assert_true(rtosc_valid_message_p(buffer, message_len),
            "Verifying Message Is Valid", __LINE__);

    return test_summary();
}
