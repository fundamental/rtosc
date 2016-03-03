#include <rtosc/rtosc.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

char buffer[1024];
int err = 0;
#define CHECK(x) \
    if(!(x)) {\
        fprintf(stderr, "failure at line %d (" #x ")\n", __LINE__); \
        ++err;}

//Check sanity with buffer offset k and within message offset j
#define CheckSanity(k,j) \
        { \
            int len = rtosc_message_length(buffer+k+j,1024-k-j); \
            assert_int_eq((32-j), len,\
                    "Checking Message Length", __LINE__); \
            if(len != 0) { \
                assert_int_eq(i, rtosc_argument(buffer+k+j, 0).i, \
                        "Checking Integer Argument", __LINE__); \
                assert_flt_eq(f, rtosc_argument(buffer+k+j, 1).f, \
                        "Checking Float Argument", __LINE__); \
                assert_str_eq(s,rtosc_argument(buffer+k+j, 2).s,\
                        "Checking String Argument", __LINE__);\
            } \
        }

int main()
{
    int32_t      i = 42;             //integer
    float        f = 0.25;           //float
    const char  *s = "string";       //string
    size_t message_size = 32;
    assert_int_eq(message_size,
            rtosc_message(buffer, 1024, "/dest", "ifs", i,f,s),
            "Building A Test Message", __LINE__);
    printf("#Everything Should Be Aligned\n");
    CheckSanity(0,0);

    printf("#Whole Offset Of 1\n");
    memmove(buffer+1, buffer, message_size);
    CheckSanity(1,0);

    printf("#Whole Offset Of 2\n");
    memmove(buffer+2, buffer+1, message_size);
    CheckSanity(2,0);

    printf("#Whole Offset Of 3\n");
    memmove(buffer+3, buffer+2, message_size);
    CheckSanity(3,0);

    printf("#Whole Offset Of 4\n");
    memmove(buffer+4, buffer+3, message_size);
    CheckSanity(4,0);

    for(int j=0; j<4; ++j) {
        printf("#Offset within aligned message %d byte\n", j);
        CheckSanity(4,j);
    }

    return err || test_summary();
}
