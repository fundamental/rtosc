#include <rtosc/ports.h>

#include "common.h"

using namespace rtosc;

int main()
{
    assert_true (port_is_less("xa", "xb"), "ports without pattern 1", __LINE__);
    assert_false(port_is_less("xb", "xa"), "ports without pattern 2", __LINE__);
    assert_false(port_is_less("xa", "xa"), "ports without pattern 3", __LINE__);
    assert_true (port_is_less("x" , "xa"), "ports without pattern 4", __LINE__);
    assert_false(port_is_less("xa", "x" ), "ports without pattern 5", __LINE__);
    assert_true (port_is_less("x" , "y" ), "ports without pattern 6", __LINE__);
    assert_false(port_is_less("y" , "x" ), "ports without pattern 7", __LINE__);

    assert_true (port_is_less("ax:", "ay:"), "ports with pattern 1", __LINE__);
    assert_false(port_is_less("ay:", "ax:"), "ports with pattern 2", __LINE__);
    assert_false(port_is_less("ay:", "ay:"), "ports with pattern 3", __LINE__);
    assert_true (port_is_less("a:",  "ax:"), "ports with pattern 4", __LINE__);
    assert_false(port_is_less("ax:", "a:"),  "ports with pattern 5", __LINE__);
    assert_false(port_is_less("a:" , "a:"),  "ports with pattern 6", __LINE__);
    assert_false(port_is_less("a:i", "a:f"), "ports with pattern 7", __LINE__);
    assert_false(port_is_less("a:f", "a:i"), "ports with pattern 8", __LINE__);

    // same length
    assert_true (port_is_less("ax:", "ay" ), "mixes ports 1", __LINE__);
    assert_false(port_is_less("ay:", "ax" ), "mixes ports 2", __LINE__);
    assert_true (port_is_less("ax" , "ay:"), "mixes ports 3", __LINE__);
    assert_false(port_is_less("ay" , "ax:"), "mixes ports 4", __LINE__);
    assert_false(port_is_less("ax:", "ax" ), "mixes ports 5", __LINE__);
    assert_false(port_is_less("ax" , "ax:"), "mixes ports 6", __LINE__);

    // different length
    assert_false(port_is_less("ax:", "a"  ),  "mixes ports 7",  __LINE__);
    assert_true (port_is_less("a:" , "ax:"),  "mixes ports 8",  __LINE__);
    assert_true (port_is_less("a:" , "ax:"),  "mixes ports 9",  __LINE__);
    assert_false(port_is_less("ax:", "a"  ),  "mixes ports 10", __LINE__);
    assert_true (port_is_less("a:i", "ax:"),  "mixes ports 11", __LINE__);
    assert_false(port_is_less("ax:", "a:i"),  "mixes ports 12", __LINE__);
    assert_false(port_is_less("a:i", "a:ii"), "mixes ports 13",  __LINE__);
    assert_false(port_is_less("a:ii","a:i" ), "mixes ports 14",  __LINE__);

    // more practical tests in walk-ports tests

    return test_summary();
}
