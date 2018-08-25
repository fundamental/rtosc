#include <rtosc/rtosc.h>
#include "common.h"


int main()
{
    //Verify that given a null buffer, it does not segfault
    assert_int_eq(20, rtosc_message(0,0,"/page/poge","TIF"),
            "Build A Message With NULL Buffer", __LINE__);
    return test_summary();
}
