#include "common.h"
#include <lo/lo.h>
#include <rtosc/rtosc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

char buffer1[128];

char buffer2[1024];

char buffer3[2048];

char buffer4[2048];
char buffer5[2048];

#define DO_PERF 1
#define PERF_TRIALS 200000000
void speed_liblo_write(void)
{
    char buffer6[2048];

    int repeats = PERF_TRIALS;
    int t_on = clock(); // timer before calling func
    for(int i=0; i<repeats; ++i) {
        lo_message lo = lo_message_new();
        lo_message_add_string(lo, "This is a string");
        lo_message_add_int32(lo, 123);
        lo_message_add_float(lo, 3.14);
        size_t len = sizeof(buffer6);
        lo_message_serialise(lo, "/methodname", buffer6, &len);
        lo_message_free(lo);
    }

    int t_off = clock(); // timer when func returns

    double seconds = (t_off - t_on) * 1.0 / CLOCKS_PER_SEC;
    printf("Liblo Write Performance: %f seconds for the test\n", seconds);
    printf("Liblo Write Performace:  %f ns per message\n", seconds*1e9/repeats);
}

void speed_liblo_read(void)
{
    char buffer6[2048];
    lo_message lo = lo_message_new();
    lo_message_add_string(lo, "This is a string");
    lo_message_add_int32(lo, 123);
    lo_message_add_float(lo, 3.14);
    size_t len = sizeof(buffer6);
    lo_message_serialise(lo, "/methodname", buffer6, &len);
    lo_message_free(lo);

    int repeats = PERF_TRIALS;
    int t_on = clock(); // timer before calling func
    for(int i=0; i<repeats; ++i) {
        int result_var = 0;
        lo_message lo = lo_message_deserialise(buffer6, len, &result_var);
        lo_message_get_argv(lo);
        lo_message_free(lo);
    }

    int t_off = clock(); // timer when func returns

    double seconds = (t_off - t_on) * 1.0 / CLOCKS_PER_SEC;
    printf("Liblo Read Performance: %f seconds for the test\n", seconds);
    printf("Liblo Read Performace:  %f ns per message\n", seconds*1e9/repeats);
}

void speed_rtosc_write(void)
{
    char buffer6[2048];

    int repeats = PERF_TRIALS;
    int t_on = clock(); // timer before calling func
    for(int i=0; i<repeats; ++i)
        rtosc_message(buffer6, sizeof(buffer6), 
                "/methodname", "sif", "This is a string", 123, 3.14);

    int t_off = clock(); // timer when func returns

    double seconds = (t_off - t_on) * 1.0 / CLOCKS_PER_SEC;
    printf("RTOSC Write Performance: %f seconds for the test\n", seconds);
    printf("RTOSC Write Performace:  %f ns per message\n", seconds*1e9/repeats);
}

void speed_rtosc_read(void)
{
    char buffer6[2048];

    rtosc_message(buffer6, sizeof(buffer6), 
            "/methodname", "sif", "This is a string", 123, 3.14);

    int repeats = PERF_TRIALS;
    int t_on = clock(); // timer before calling func
    for(int i=0; i<repeats; ++i) {
        rtosc_arg_itr_t itr = rtosc_itr_begin(buffer6);
        while(!rtosc_itr_end(itr))
            rtosc_itr_next(&itr);
    }

    int t_off = clock(); // timer when func returns

    double seconds = (t_off - t_on) * 1.0 / CLOCKS_PER_SEC;
    printf("RTOSC Read Performance: %f seconds for the test\n", seconds);
    printf("RTOSC Read Performace:  %f ns per message\n", seconds*1e9/repeats);
}

int result_var = 0;
int main()
{
    //clean buffer
    memset(buffer1, 0xc3, sizeof(buffer1));

    //generate liblo message 1
    size_t len = 128;
    lo_message message = lo_message_new();
    assert_non_null(message, "Generating A Liblo Message 1 (int/float)", __LINE__);
    lo_message_add_float(message, 24.0);
    lo_message_add_int32(message, 42);
    lo_message_serialise(message, "/path", buffer1, &len);

    assert_str_eq("/path", buffer1, "Verifying Path From Message 1", __LINE__);
    assert_f32_eq(24.0f, rtosc_argument(buffer1, 0).f,
            "Verifying Float From Message 1", __LINE__);
    assert_int_eq(42, rtosc_argument(buffer1, 1).i,
            "Verifying Int From Message 1", __LINE__);
    assert_int_eq(20, rtosc_message_length(buffer1, 128),
            "Verifying Length From Message 1", __LINE__);


    //Message 2
    size_t len2 = rtosc_message(buffer2, 1024, "/li", "bb", 4, buffer1, 4, buffer1);
    assert_int_eq(24, len2, "Generate A Rtosc Message 2 (bb)", __LINE__);
    lo_message msg2 = lo_message_deserialise((void*)buffer2, len2, &result_var);
    if(assert_non_null(msg2, "Deserialize Message 2 To Liblo", __LINE__))
        printf("# Liblo Did Not Accept the Rtosc Message [error '%d']\n", result_var);

    //Message 3
    size_t len3 = rtosc_message(buffer3+4, 2048, "/close-ui", "");
    assert_int_eq(16, len3, "Generate A Rtosc Message 3 ()", __LINE__);
    lo_message msg3 = lo_message_deserialise((void*)(buffer3+4), len3, &result_var);
    if(assert_non_null(msg2, "Deserialize Message 3 To Liblo", __LINE__))
        printf("#Liblo Did Not Accept the Rtosc Message [error '%d']\n", result_var);

    //Bundle 4
    size_t len4 = rtosc_bundle(buffer4, 2048, 0xdeadbeefcafebaad, 3, buffer1, buffer2, buffer3+4);
    assert_int_eq(88, len4, "Generate A Bundle 4", __LINE__);

    //Bundle 5
    lo_timetag time;
    time.sec  = 0xdeadbeef;
    time.frac = 0xcafebaad;
    lo_bundle ms4 = lo_bundle_new(time);
    lo_bundle_add_message(ms4, "/path",     message);
    lo_bundle_add_message(ms4, "/li",       msg2);
    lo_bundle_add_message(ms4, "/close-ui", msg3);
    size_t len5 = 2048;
    lo_bundle_serialise(ms4,(void*)buffer5, &len5);

    //Verify 4 == 5
    assert_non_null(ms4, "Generate A Liblo Bundle 5", __LINE__);
    assert_hex_eq(buffer5, buffer4, len5, len4,
            "Verify Liblo Style Bundles", __LINE__);

    //Cleanup
    lo_message_free(message);
    lo_message_free(msg2);
    lo_message_free(msg3);
    lo_bundle_free(ms4);

    speed_liblo_read();
    speed_rtosc_read();
    speed_liblo_write();
    speed_rtosc_write();

    return test_summary();
}
