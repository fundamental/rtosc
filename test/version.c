#include <rtosc/rtosc-version.h>
#include "common.h"

void comparisons()
{
    rtosc_version v_3_1_1 = { 3, 1, 1 },
                  v_1_3_3 = { 1, 3, 3 },
                  v_2_9_9 = { 2, 9, 9 },
                  v_3_4_3 = { 3, 4, 3 };

    assert_int_eq(1, rtosc_version_cmp(v_3_1_1, v_1_3_3)  > 0,
                  "v3.1.1  > v1.3.3", __LINE__);
    assert_int_eq(1, rtosc_version_cmp(v_2_9_9, v_3_4_3)  < 0,
                  "v2.9.9  < v3.4.3", __LINE__);
    assert_int_eq(1, rtosc_version_cmp(v_3_4_3, v_3_4_3) == 0,
                  "v3.4.3 == v3.4.3", __LINE__);
}

void current_version()
{
    rtosc_version cur = rtosc_current_version();
    rtosc_version v_0_0_9 = { 0, 0, 9 };
    rtosc_version v_9_9_9 = { 9, 9, 9 };

    // always true since versions were introduced in 0.1.0
    assert_int_eq(1, rtosc_version_cmp(cur, v_0_0_9) > 0,
                  "current version is > 0.0.9", __LINE__);
    // probably true for a very long time
    assert_int_eq(1, rtosc_version_cmp(cur, v_9_9_9) < 0,
                  "current version is < 9.9.9", __LINE__);
}

void print()
{
    rtosc_version v_0_1_2 = {   0,   1,   2 },
                  v_max   = { 255, 255, 255 };

    char buf[32];
    rtosc_version_print_to_12byte_str(&v_0_1_2, buf);
    assert_str_eq("0.1.2", buf, "print version 0.1.2", __LINE__);
    rtosc_version_print_to_12byte_str(&v_max, buf);
    assert_str_eq("255.255.255", buf, "print version with max width", __LINE__);
}

int main()
{
    comparisons();
    current_version();
    print();

    return test_summary();
}
