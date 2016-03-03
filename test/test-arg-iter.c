#include <rtosc/rtosc.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

typedef uint8_t midi_t[4];
char buffer[1024];
int err = 0;
#define CHECK(x) \
    if(!(x)) {\
        fprintf(stderr, "failure at line %d (" #x ")\n", __LINE__); \
        ++err;}

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

    CHECK(message_len = rtosc_message(buffer, 1024, "/dest",
                "[ifsbhtdScrmTFNI]",
                i,f,s,b.len,b.data,h,t,d,S,c,r,m));

    rtosc_arg_itr_t itr = rtosc_itr_begin(buffer);
    //while(!rtosc_itr_end(itr))
    //    printf("rtosc_itr_next->'%c'\n", rtosc_itr_next(&itr).type);
    rtosc_arg_val_t val = rtosc_itr_next(&itr);
    assert_false(rtosc_itr_end(itr),
            "Create A Valid Iterator", __LINE__);
    CHECK(val.type == 'i');
    CHECK(val.val.i == i);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'f');
    CHECK(val.val.f == f);
    
    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 's');
    CHECK(!strcmp(val.val.s,s));

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'b');
    CHECK(val.val.b.len == 3);
    CHECK(!strcmp((char*)val.val.b.data, "str"));

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'h');
    CHECK(val.val.h == h);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 't');
    CHECK(val.val.t == t);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'd');
    CHECK(val.val.d == d);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'S');
    CHECK(!strcmp(val.val.s,S));

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'c');
    CHECK(val.val.i == c);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'r');
    CHECK(val.val.i == r);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'm');
    CHECK(val.val.m[0] == m[0])
    CHECK(val.val.m[1] == m[1])
    CHECK(val.val.m[2] == m[2])
    CHECK(val.val.m[3] == m[3])

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'T');
    CHECK(val.val.T);

    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'F');
    CHECK(!val.val.T);
    
    val = rtosc_itr_next(&itr);
    CHECK(!rtosc_itr_end(itr));
    CHECK(val.type == 'N');
    
    val = rtosc_itr_next(&itr);
    CHECK(rtosc_itr_end(itr));
    CHECK(val.type == 'I');

    CHECK(rtosc_valid_message_p(buffer, message_len));


#if 0
    rtosc_arg_t speed_check[4096];
    char speed_check_type[4097];
    memset(speed_check_type, 'f', 4096);
    char buffer[4096*8];
    for(int i=0; i<4096; ++i)
        speed_check[i].f = i;
    rtosc_amessage(buffer, sizeof(buffer), "/nil", speed_check_type, speed_check);

    int sum = 0;
#if 0
    for(int i=0; i<4096; ++i)
        sum += rtosc_argument(buffer, i).f;
#else
    rtosc_arg_itr_t itr2 = rtosc_itr_begin(buffer);
    while(!rtosc_itr_end(itr2))
        sum += rtosc_itr_next(&itr2).val.f;
#endif

    //sum 0..4095
    int truth = 0;
    for(int i=0; i<4096; ++i)
        truth += i;
    printf("sum(%d) = %d (difference %d)\n", truth, sum, sum-truth);
#endif




    return err || test_summary();
}
