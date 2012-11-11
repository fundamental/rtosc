#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rtosc.h>

char buffer_a[256];
char buffer_b[256];
char buffer_c[256];

void check(int b, const char *msg, int loc)
{
    if(!b) {
        fprintf(stderr, "%s:%d\n", msg, loc);
        exit(1);
    }
}

int main()
{
    check(rtosc_message(buffer_a, 256, "/flying-monkey", "s", "bannana"),
            "bad message", __LINE__);
    check(rtosc_message(buffer_b, 256, "/foobar-message", "ifT", 42, 3.14159),
            "bad message", __LINE__);
    check(!rtosc_bundle_p(buffer_a),
            "False positive bundle_p()", __LINE__);
    check(!rtosc_bundle_p(buffer_b),
            "False positive bundle_p()", __LINE__);
    check(rtosc_bundle(buffer_c, 256, 0, 2, buffer_a, buffer_b) == 88,
            "bad bundle", __LINE__);
    check(rtosc_message_length(buffer_c) == 88,
            "bad message length", __LINE__);

    check(rtosc_bundle_p(buffer_c),
            "Bad bundle detection", __LINE__);
    check(rtosc_bundle_elements(buffer_c)==2,
            "Bad bundle_elements length", __LINE__);
    check(!strcmp("/flying-monkey", rtosc_bundle_fetch(buffer_c, 0)),
            "Bad bundle_fetch", __LINE__);
    check(!strcmp("/foobar-message", rtosc_bundle_fetch(buffer_c, 1)),
            "Bad bundle_fetch", __LINE__);

    //Check minimum bundle size #bundle + time tag + ending null
    check(rtosc_bundle(buffer_c, 256, 1, 0) == (8+8+4),
            "Bad minimum bundle length", __LINE__);

    //check message length support
    check(rtosc_message_length(buffer_c) == (8+8+4),
            "Bad message length", __LINE__);

    //Verify that timetag can be fetched
    check(rtosc_bundle_timetag(buffer_c)==1,
            "Bad timetag", __LINE__);

    return 0;
}
