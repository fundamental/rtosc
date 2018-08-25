#include "common.h"
#include <stdlib.h>
#include <rtosc/rtosc.h>

char buffer_a[256];
char buffer_b[256];
char buffer_c[256];
char buffer_d[256];
char buffer_e[256];

#define TIMETAG_VALUE ((uint64_t)0xdeadbeefcafebaad)
#define BUNDLE_ARG "bund" "le\0\0"
#define POUND_BUNDLE "#bun" "dle\0"
#define TIMETAG "\xde\xad\xbe\xef" "\xca\xfe\xba\xad"
#define MSG_A "/bun" "dle\0" ",s\0\0" BUNDLE_ARG
#define MSG_B "/bun" "dle-" "bund" "le\0\0" ",ss\0" BUNDLE_ARG BUNDLE_ARG
#define LEN_A "\0\0\0\x14"
#define LEN_B "\0\0\0\x24"
#define LEN_C "\0\0\0\x40"

#define BAD_SIZE "\xba\xad\xca\xfe"

#define BUNDLE_C \
POUND_BUNDLE \
TIMETAG \
LEN_A \
MSG_A \
LEN_A \
MSG_A
    
#define BUNDLE_D \
POUND_BUNDLE \
TIMETAG \
LEN_C \
BUNDLE_C \
LEN_C \
BUNDLE_C \
LEN_B \
MSG_B

const int La = sizeof(MSG_A)-1;
const int Lb = sizeof(MSG_B)-1;
const int Lc = sizeof(BUNDLE_C)-1;
const int Ld = sizeof(BUNDLE_D)-1;

int main()
{
    //Message 1
    assert_int_eq(La, rtosc_message(buffer_a, 256, "/bundle", "s", "bundle"),
            "Create Message 1", __LINE__);
    assert_hex_eq(MSG_A, buffer_a, La, La,
            "Verifying Contents Of Message 1", __LINE__);

    //Message 2
    assert_int_eq(Lb, rtosc_message(buffer_b, 256, "/bundle-bundle", "ss", "bundle", "bundle"),
            "Create Message 2", __LINE__);
    assert_hex_eq(MSG_B, buffer_b, Lb, Lb,
            "Verifying Contents Of Message 2", __LINE__);

    int len;
    //Bundle 1
    assert_int_eq(64, rtosc_bundle(buffer_c, 256, TIMETAG_VALUE, 2, buffer_a, buffer_a),
            "Create Bundle 1", __LINE__);
    assert_hex_eq(BUNDLE_C, buffer_c, Lc, Lc,
            "Verifying Bundle 1's Content", __LINE__);
    assert_int_eq(64, rtosc_message_length(buffer_c, 256),
            "Verify Bundle 1's Length", __LINE__);

    //Bundle 2
    assert_int_eq(192, len = rtosc_bundle(buffer_d, 256, TIMETAG_VALUE, 3, buffer_c, buffer_c, buffer_b),
            "Create Bundle 2", __LINE__);
    assert_hex_eq(BUNDLE_D, buffer_d, Ld, Ld,
            "Verifying Bundle 2's Content", __LINE__);
    assert_int_eq(192, rtosc_message_length(buffer_d, 256),
            "Verify Bundle 2's Length", __LINE__);
    assert_int_eq(192, rtosc_message_length(buffer_d, len),
            "Verify Bundle 2's Length With Tight Bounds", __LINE__);


    assert_int_eq(3, rtosc_bundle_elements(buffer_d, len),
            "Verify Bundle 2's Subelements", __LINE__);
    assert_str_eq("bundle", rtosc_argument(rtosc_bundle_fetch(rtosc_bundle_fetch(buffer_d, 1), 1), 0).s,
            "Verify Nested Message's Integrety", __LINE__);

    //Verify the failure behavior when a bad length is provided
    assert_int_eq(2, rtosc_bundle_elements(buffer_d, len-1),
            "Verify Aparent Bundles With Truncated Length", __LINE__);
    assert_int_eq(0, rtosc_message_length(buffer_d, len-1),
            "Verify Bad Message Is Detected With Truncation", __LINE__);

    return test_summary();
}
