#include "rtosc/ports.h"

#define CHECK(x) \
    if(!(x)) {\
        fprintf(stderr, "failure at line %d (" #x ")\n", __LINE__); \
        ++err;}

int main()
{
    const char *P1 = "/foo/bar/../baz";
    char *p1 = rtosc::Ports::collapsePath(strdup(P1));
    const char *P2 = "/../bar/../baz";
    char *p2 = rtosc::Ports::collapsePath(strdup(P2));
    const char *P3 = "/foo/path/bam/blaz/asdfasdf/../../..";
    char *p3 = rtosc::Ports::collapsePath(strdup(P3));
    printf(" '%s' = '%s'\n", P1, p1);
    printf(" '%s' = '%s'\n", P2, p2);
    printf(" '%s' = '%s'\n", P3, p3);
    return 0;
}
