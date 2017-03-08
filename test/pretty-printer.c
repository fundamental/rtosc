#include <rtosc/rtosc.h>
#include "common.h"

void check(size_t n, const rtosc_arg_val_t* av,
           const rtosc_print_options* opt,
           const char* exp, const char* testcase,
           int line)
{
    static char testcase_lc[1024]; // descr. for the lenght check testcase
    size_t len = 128;
    char buffer[len];
    size_t written = rtosc_print_arg_vals(av, n, buffer, len, opt);

    // checks
    assert_str_eq(exp, buffer, testcase, line);
    strncpy(testcase_lc, testcase, 1024);
    strncat(testcase_lc, " (length check)", 1024 - strlen(testcase_lc));
    assert_int_eq(strlen(exp), written, testcase_lc, line);
}

void print_simple()
{
    rtosc_arg_val_t av;

    av.type = 'N';
    check(1, &av, NULL, "nil", "pretty print 'N' (nil)", __LINE__);

    av.type = 'I';
    check(1, &av, NULL, "inf", "pretty print 'I' (inf)", __LINE__);

    av.type = 'T';
    check(1, &av, NULL, "true", "pretty print 'T' (true)", __LINE__);

    av.type = 'F';
    check(1, &av, NULL, "false", "pretty print 'F' (false)", __LINE__);

    av.type = 'h';
    av.val.h = 0xFFFFFFFFFFFFFFFE;
    check(1, &av, NULL, "-2", "pretty print 'h' (signed 64 bit)", __LINE__);

    av.type = 't';
    av.val.t = 0xFFFFFFFFFFFFFFFF;
    check(1, &av, NULL, "18446744073709551615", "pretty print 't' (unsigned 64 bit)", __LINE__);

    av.type = 'd';
    av.val.d = 1234567890.0987;
    check(1, &av, NULL, "1234567890.098700", "pretty print 'd' (double)", __LINE__);

    av.type = 'r';
    av.val.i = 0xDEADBEEF;
    check(1, &av, NULL, "#deadbeef", "pretty print 'r' (rgb color)", __LINE__);

    av.type = 'f';
    av.val.f = 0.56789;
    rtosc_print_options lossy = ((rtosc_print_options) { false, 3, " "});
    check(1, &av, &lossy, "0.568", "pretty print 'f' (float) lossy", __LINE__);

    av.type = 'f';
    av.val.f = -1.5;
    check(1, &av, NULL, "-1.50 (-0x1.8p+0)", "pretty print 'f' (float) lossless", __LINE__);

    av.type = 'f';
    av.val.f = 1.0;
    rtosc_print_options prec0 = ((rtosc_print_options) { true, 0, " "});
    check(1, &av, &prec0, "1. (0x1p+0)", "pretty print 'f' (float) with zero precision", __LINE__);

    av.type = 'c';
    av.val.i = '#';
    check(1, &av, NULL, "'#'", "pretty print 'c' (char as char)", __LINE__);

    av.type = 'i';
    av.val.i = 0xFFFFFFFF;
    check(1, &av, NULL, "-1", "pretty print 'i' (integer)", __LINE__);

    av.type = 'm';
    av.val.m[0] = 0xFF;
    av.val.m[1] = 0xFF;
    av.val.m[2] = 0xFF;
    av.val.m[3] = 0xFF;
    check(1, &av, NULL, "MIDI [0xff 0xff 0xff 0xff]", "pretty print 'm' (midi)", __LINE__);

#define STR_WITH_29_CHARS "12345678901234567890123456789"
    av.type = 'S';
    av.val.s = STR_WITH_29_CHARS;
    check(1, &av, NULL, "\""STR_WITH_29_CHARS"\"",
          "pretty print 'S' (C string)", __LINE__);
#undef STR_WITH_29_CHARS

    av.type = 's';
    av.val.s = "";
    check(1, &av, NULL, "\"\"", "pretty print 's' (C string)", __LINE__);

    av.type = 'b';
    av.val.b.data = (uint8_t*)"rtosc";
    av.val.b.len = 6;
    check(1, &av, NULL, "[6 0x72 0x74 0x6f 0x73 0x63 0x00]", "pretty print 'b' (blob)", __LINE__);
}

void pack_arg_vals()
{
    rtosc_arg_val_t av[4];

    rtosc_2argvals(av, 1, "i", -42);
    check(1, av, NULL, "-42", "packing 1 int into arg val array", __LINE__);

    rtosc_2argvals(av, 4, "iiiT", 0, 1, 2);
    check(4, av, NULL, "0 1 2 true",
          "packing 3 ints and one bool into arg val array", __LINE__);

#define TMPSTR "012345678901234567890123"
    rtosc_print_options sep_comma = ((rtosc_print_options) { true, 2, ", "});
    rtosc_2argvals(av, 2, "sc", TMPSTR, 'c');
    check(2, av, &sep_comma, "\""TMPSTR"\", 'c'",
          "packing a string and a char into an arg val array", __LINE__);
#undef TMPSTR
}

int main()
{
    print_simple();
    pack_arg_vals();

    return test_summary();
}
