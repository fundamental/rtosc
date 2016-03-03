#include "rtosc/ports.h"
#include "common.h"

int main()
{
    const char *P1 = "/foo/bar/../baz";
    char *p1 = rtosc::Ports::collapsePath(strdup(P1));
    const char *P2 = "/../bar/../baz";
    char *p2 = rtosc::Ports::collapsePath(strdup(P2));
    const char *P3 = "/foo/path/bam/blaz/asdfasdf/../../..";
    char *p3 = rtosc::Ports::collapsePath(strdup(P3));
    assert_str_eq("/foo/baz",  p1, "Check Single Level Collapse", __LINE__);
    assert_str_eq("/baz",      p2, "Check Dual   Level Collapse", __LINE__);
    assert_str_eq("/foo/path", p3, "Check Triple Level Collapse", __LINE__);
    return test_summary();
}

