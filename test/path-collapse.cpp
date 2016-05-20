#include "rtosc/ports.h"
#include "common.h"

int main()
{
    char *P1 = strdup("/foo/bar/../baz");
    char *p1 = rtosc::Ports::collapsePath(P1);
    char *P2 = strdup("/../bar/../baz");
    char *p2 = rtosc::Ports::collapsePath(P2);
    char *P3 = strdup("/foo/path/bam/blaz/asdfasdf/../../..");
    char *p3 = rtosc::Ports::collapsePath(P3);
    assert_str_eq("/foo/baz",  p1, "Check Single Level Collapse", __LINE__);
    assert_str_eq("/baz",      p2, "Check Dual   Level Collapse", __LINE__);
    assert_str_eq("/foo/path", p3, "Check Triple Level Collapse", __LINE__);

    free(P1);
    free(P2);
    free(P3);

    return test_summary();
}

