#include <rtosc/rtosc.h>
#include "common.h"

char buffer[1024];

//verify that empty strings can be retreived
int main()
{
    size_t length = rtosc_message(buffer, 1024, "/path", "sss", "", "", "");
    // /pat h000 ,sss 0000 0000 0000 0000
    assert_int_eq(28, length, "Build Empty String Based Message", __LINE__);
    assert_non_null(rtosc_argument(buffer, 0).s, "Check Arg 1", __LINE__);
    assert_non_null(rtosc_argument(buffer, 1).s, "Check Arg 2", __LINE__);
    assert_non_null(rtosc_argument(buffer, 2).s, "Check Arg 3", __LINE__);
    return test_summary();
}
