#include "../src/util.h"
#include "common.h"

void test_fast_strcpy()
{
    char* src = "rtosc is a good library";
    char dest[32];
    memset(dest, 0, sizeof(dest));
    dest[strlen(src)+1] = 'x';

    fast_strcpy(dest, src, 32);
    assert_str_eq(src, dest,
                  "fast_strcpy() copies at most <strlen> bytes", __LINE__);

    fast_strcpy(dest, src, 32);
    assert_char_eq('x', dest[strlen(src)+1],
                  "fast_strcpy() does not pad the dest with zeros", __LINE__);

    fast_strcpy(dest, src, 6);
    assert_str_eq("rtosc", dest,
                  "fast_strcpy() copies at most <buffersize> bytes", __LINE__);
}

/*
    all tests
*/
int main()
{
    test_fast_strcpy();

    return test_summary();
}


