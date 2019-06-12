#include "common.h"
#include <stdlib.h>
#include <rtosc/rtosc.h>

char buffer_a[256];
char buffer_b[256];
char buffer_c[256];

size_t len_a;
size_t len_b;
size_t len_c;

#define MSG1 "/fly" "ing-" "monk" "ey\0\0" ",s\0\0" "bann" "ana\0"
#define MSG2 "/foo" "bar-" "mess" "age\0" ",iT\0" "\0\0\0\x2a"

               /*bundle          timetag*/
#define RESULT "#bun" "dle\0" "\0\0\0\0" "\0\0\0\0" \
               /*size1             size2*/ \
               "\0\0\0\x1c" MSG1 "\0\0\0\x18" MSG2

int main()
{
    //Message 1
    assert_int_eq(sizeof(MSG1)-1, len_a = rtosc_message(buffer_a, 256,
                    "/flying-monkey", "s", "bannana"),
            "Creating Simple Message 1", __LINE__);
    assert_hex_eq(MSG1, buffer_a, sizeof(MSG1)-1, len_a,
            "Verifying Content of Message 1", __LINE__);
    assert_int_eq(0, rtosc_bundle_p(buffer_a),
            "Simple Message 1 Is Not A Bundle", __LINE__);

    //Message 2
    assert_int_eq(sizeof(MSG2)-1, len_b = rtosc_message(buffer_b, 256,
                    "/foobar-message", "iT", 42),
            "Creating Simple Message 2", __LINE__);
    assert_hex_eq(MSG2, buffer_b, sizeof(MSG2)-1, len_b,
            "Verifying Content of Message 2", __LINE__);
    assert_int_eq(0, rtosc_bundle_p(buffer_b),
            "Simple Message 2 Is Not A Bundle", __LINE__);

    //Bundle 1
    assert_int_eq(sizeof(RESULT)-1, len_c = rtosc_bundle(buffer_c, 256, 0, 2, buffer_a, buffer_b),
            "Creating Bundle 1", __LINE__);
    assert_int_eq(len_c, rtosc_message_length(buffer_c, len_c),
            "Verifying Bundle 1's Length", __LINE__);
    assert_hex_eq(RESULT, buffer_c, sizeof(RESULT)-1, len_c,
            "Verifying Bundle 1's Content", __LINE__);

    assert_int_eq(1, rtosc_bundle_p(buffer_c),
            "Verifying Bundle 1 Is A Bundle", __LINE__);
    assert_int_eq(2, rtosc_bundle_elements(buffer_c, 256),
            "Verifying Bundle 2 Has Two Messages", __LINE__);

    assert_str_eq("/flying-monkey", rtosc_bundle_fetch(buffer_c, 0),
            "Verifying Message 1 Path", __LINE__);
    assert_str_eq("/foobar-message", rtosc_bundle_fetch(buffer_c, 1),
            "Verifying Message 2 Path", __LINE__);

    //Verify that rtosc_bundle_size works
    assert_int_eq(0x1c, rtosc_bundle_size(buffer_c, 0),
            "Verify rtosc_bundle_size() Works", __LINE__);

    assert_int_eq(0x18, rtosc_bundle_size(buffer_c, 1),
            "Verify rtosc_bundle_size() Works", __LINE__);

    //Check minimum bundle size #bundle + time tag
    assert_int_eq(8+8, rtosc_bundle(buffer_c, 256, 1, 0),
            "Verify Minimum Legal Bundle Size", __LINE__);

    //check message length support
    assert_int_eq(8+8, rtosc_message_length(buffer_c, 256),
            "Verify rtosc_message_length() on Minimum Bundle", __LINE__);

    //Verify that timetag can be fetched
    assert_int_eq(1, rtosc_bundle_timetag(buffer_c),
            "Verify rtosc_bundle_timetag() Works", __LINE__);

    return test_summary();
}
