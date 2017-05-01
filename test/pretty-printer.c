#include <assert.h>
#include <rtosc/rtosc.h>
#include "common.h"

/**
 * @brief check_alt like check, but specify an alternative expectation
 * @see check()
 * @param arg_val_str @see check()
 * @param opt @see check()
 * @param tc_base @see check()
 * @param line @see check()
 * @param _exp_print alternative expectation of what shall be printed
 */
void check_alt(const char* arg_val_str, const rtosc_print_options* opt,
               const char* tc_base, int line,
               const char* _exp_print)
{
    int tc_len = 256;
    char tc_full[tc_len]; // descr. for full testcase name
    int strbuflen = 256;
    char strbuf[strbuflen];

    int num = rtosc_count_printed_arg_vals(arg_val_str);
    assert(num > 0);
    rtosc_arg_val_t scanned[num];
    size_t rd = rtosc_scan_arg_vals(arg_val_str, scanned, num,
                                    strbuf, strbuflen);

    strcpy(tc_full, "scan ");
    strncat(tc_full, tc_base, tc_len);
    strncat(tc_full, " (read exactly the input string)",
            tc_len - strlen(tc_full));
    assert_int_eq(strlen(arg_val_str), rd, tc_full, line);

    size_t len = 128;
    char printed[len];
    size_t written = rtosc_print_arg_vals(scanned, num, printed, len, opt, 0);

    const char* exp_print = _exp_print ? _exp_print : arg_val_str;
    strcpy(tc_full, "print ");
    strncat(tc_full, tc_base, tc_len);
    strncat(tc_full, " (value = value before scan)",
            tc_len - strlen(tc_full));
    assert_str_eq(exp_print, printed, tc_full, line);

    strcpy(tc_full, "print ");
    strncat(tc_full, tc_base, tc_len);
    strncat(tc_full, " (return value check)", tc_len - strlen(tc_full));
    assert_int_eq(strlen(exp_print), written, tc_full, line);
}

/**
 * @brief check scan string into rtosc_arg_val_t array, print the result
 *   and compare the two strings
 * @param arg_val_str input string: to be scanned and then printed
 * @param opt options for printing
 * @param tc_base basic testcase name: what shall be scanned and printed?
 * @param line line where this function is being called
 */
void check(const char* arg_val_str, const rtosc_print_options* opt,
           const char* tc_base, int line)
{
    check_alt(arg_val_str, opt, tc_base, line, NULL);
}

void scan_and_print_single()
{
    /*
        simple types
    */
    check("nil", NULL, "'nil'", __LINE__);
    check("inf", NULL, "'inf'", __LINE__);
    check("true", NULL, "'true'", __LINE__);
    check("false", NULL, "'false'", __LINE__);

    /*
        colors
    */
    check("#deadbeef", NULL, "the rgba color '#deafbeef'", __LINE__);

    /*
        timestamps
    */
    rtosc_print_options lossy = ((rtosc_print_options) { false, 3, " ", 80 });
    check_alt("1970-01-02 00:00:00", NULL, "one day after the epoch", __LINE__,
              "1970-01-02");
    check("2016-11-16 19:44:06", NULL, "a timestamp", __LINE__);
    check("2016-11-16 19:44:06.123", &lossy,
          "a timestamp with second fractions", __LINE__);
    check("2016-11-16 19:44", NULL, "a timestamp with 0 seconds", __LINE__);
    check("2016-11-16", NULL, "a timestamp without clocktime", __LINE__);
    check("immediately", NULL, "the timestamp \"immediately\"", __LINE__);
    check_alt("now", NULL, "the timestamp \"now\"", __LINE__, "immediately");

    // must be printed lossless, but second fractions are zero => no parantheses
    check_alt("2016-11-16 00:00:00.123 (...+0x0p-32s)", NULL,
              "a timestamp with exact second fractions", __LINE__,
              "2016-11-16");
    check_alt("2016-11-16 00:00:00.123 ( ... + 0xa0000000p-32 s )", NULL,
              "a timestamp with exact second fractions", __LINE__,
              "2016-11-16 00:00:00.62 (...+0x1.4p-1s)");

    /*
        doubles
    */
    // saving this as a float will lose precision
    rtosc_print_options prec6 = ((rtosc_print_options) { true, 6, " ", 80 });
    check_alt("1234567890.098700d", &prec6,
              "a double that would not fit into a float", __LINE__,
              "1234567890.098700d (0x1.26580b486511ap+30)");

    /*
        floats
    */
    check_alt("0.56789", &lossy, "a float with loss of precision", __LINE__,
              "0.568");
    // this looks like 0.0f, but the lossless representation indicates "-1.50"
    // rtosc *must* discard the first number and read the lossless one
    check_alt("0.0f (-0x1.8p+0)", NULL, "the zero float (0.0f)", __LINE__,
              "-1.50 (-0x1.8p+0)");
    rtosc_print_options prec0 = ((rtosc_print_options) { true, 0, " ", 80 });
    // this float may not lose its period (otherwise,
    // it would be read as an int!)
    check_alt("1.", &prec0, "a float with zero precision", __LINE__,
              "1. (0x1p+0)");
    check_alt("1e1", &lossy, "the float 1e1", __LINE__, "10.000");

    /*
        chars
    */
    check("'#'", NULL, "the hash character", __LINE__);
    check("'\\a'", NULL, "a bell ('\\a')", __LINE__);
    check("'\\b'", NULL, "a backspace ('\\b')", __LINE__);
    check("'\\t'", NULL, "a horizontal tab('\\t')", __LINE__);
    check("'\\n'", NULL, "a newline ('\\n')", __LINE__);
    check("'\\v'", NULL, "a vertical ('\\v')", __LINE__);
    check("'\\f'", NULL, "a form feed ('\\f')", __LINE__);
    check("'\\r'", NULL, "a carriage ret ('\\r')", __LINE__);
    check("'\\''", NULL, "an escaped single quote character", __LINE__);
    check_alt("'\\'", NULL, "a non-escaped backslash", __LINE__, "'\\\\'");
    // the following means that the user enters '\\'
    check("'\\\\'", NULL, "an escaped backslash", __LINE__);

    /*
        integers
    */
    check("-5000000000h", NULL,
           "an integer that requires 64 bit storage", __LINE__);
    check("-1", NULL, "a simple integer (-1)", __LINE__);
    check_alt("-0xffffffed", NULL,
              "an integer in hex notation", __LINE__, "19");

    /*
        MIDI
    */
    check("MIDI [0xff 0xff 0xff 0xff]", NULL, "a MIDI integer", __LINE__);
    check_alt("MIDI[ 0x00 0x00 0x00 0x00 ]", NULL,
               "another MIDI integer", __LINE__,
               "MIDI [0x00 0x00 0x00 0x00]");

    /*
        strings
    */
    check("\"12345678901234567890123456789\"", NULL,
           "a long string", __LINE__);
    check("\"\"", NULL, "the empty string", __LINE__);
    check_alt("\"\\\"\\a\\b\\t\\n\\v\\f\\r\\\\\"", NULL,
              "a string with all escape sequences", __LINE__,
              "\"\\\"\\a\\b\\t\\n\"\\\n"
              "    \"\\v\\f\\r\\\\\"");

    /*
        identifiers aka symbols
    */
    check("an_identifier_42", NULL, "a simple identifier", __LINE__);
    check("_", NULL, "the identifier \"_\"", __LINE__);
    check("truely falseeee infinite nilpferd immediatelyly nowhere MIDINOTE",
          NULL,
          "reserved keywords with letters appended are identifiers", __LINE__);

    /*
        blobs
    */
    check("[6 0x72 0x74 0x6f 0x73 0x63 0x00]", NULL,
          "a six bytes blob", __LINE__);
    check("[0]", NULL, "an empty blob", __LINE__);

    /*
        comments
    */
    check_alt("1 % 2\n"
              "3", NULL,
              "three ints, one after a comment", __LINE__, "1 3");
    check_alt("true%false", NULL,
              "comment right after \"true\"", __LINE__, "true");

    /*
        linebreaks
    */
    rtosc_print_options shortline = ((rtosc_print_options) { true, 3,
                                                             " ", 10 });
    check_alt("\"01234567890123456789\"", &shortline,
              "string exceeding line length", __LINE__,
              "\"01234567\"\\\n"
              "    \"890\"\\\n"
              "    \"123\"\\\n"
              "    \"456\"\\\n"
              "    \"789\"");
    check("\"01234567\"\\\n    \"890\"", &shortline,
          "broken down string", __LINE__);
    check_alt("[2 0xff 0xff]", &shortline, "long blob is being broken",
              __LINE__, "[2 0xff\n    0xff]");
    check_alt("false false 'a'", &shortline,
              "break between arguments", __LINE__, "false\n    false 'a'");
}

void scan_and_print_mulitple()
{
    check("0 1 2 true", NULL, "3 ints and one bool", __LINE__);
    check("\"012345678901234567890123\" 'c'", NULL,
          "a string and a char", __LINE__);
    // interesting, because they overlap in the "buffer for strings"
    // (see "strbuf" above):
    check("\"1\" [1 0xff] \"2\"", NULL,
          "a string, a blob and a string", __LINE__);
}

void fail_at_arg(const char* arg_val_str, int exp_fail, int line)
{
    int tc_len = 256;
    char tc_full[tc_len]; // descr. for full testcase name

    size_t num = rtosc_count_printed_arg_vals(arg_val_str);

    strcpy(tc_full, "find 1st invalid arg in ");
    strncat(tc_full, arg_val_str, tc_len);
    assert_int_eq(exp_fail, -num, tc_full, line);
}

void messages()
{
    int strbuflen = 256;
    char strbuf[strbuflen];
    int msgbuflen = 256;
    char msgbuf[msgbuflen];
    const char* input = "%this is a savefile\n"
                        "/noteOn 0 0 0 % a noteOn message";

    int num = rtosc_count_printed_arg_vals_of_msg(input);
    assert_int_eq(3, num, "read a /noteOn message", __LINE__);
    rtosc_arg_val_t scanned[num];
    size_t rd = rtosc_scan_message(input, msgbuf, msgbuflen,
                                   scanned, num,
                                   strbuf, strbuflen);
    assert_int_eq(strlen(input), rd,
                  "read a message and return correct length", __LINE__);
    assert_str_eq("/noteOn", msgbuf, "scan address correctly", __LINE__);

    size_t len = 128;
    char printed[len];
    rtosc_print_options shortline = ((rtosc_print_options) { true, 3, " ", 7 });
    size_t written = rtosc_print_message("/noteOn", scanned, num,
                                         printed, len, &shortline, 0);

    const char* exp = "/noteOn\n    0 0\n    0";
    assert_str_eq(exp, printed, "print a message", __LINE__);
    assert_int_eq(strlen(exp), written,
                  "print a message and return written number of bytes",
                  __LINE__);
}

void scan_invalid()
{
#define BAD(s) fail_at_arg(s, 1, __LINE__);

    /*
        simple types
    */
    // not testable: mistyped keywords like "tru" are considered
    // valid identifiers

    /*
        colors
    */
    BAD("#deadhorse");
    BAD("#abcdefgh");
    BAD("#");

    /*
        timestamps
    */
    BAD("2016:11:16");
    BAD("16.11.2016");
#if 0
    BAD("2016-11-16 00"); // correct: date + int
#endif
    fail_at_arg("2016-11-16 00:00:0", 2, __LINE__); // 2 digits needed
    fail_at_arg("2016-11-16 00:00:00:1", 2, __LINE__); // period needed
    fail_at_arg("2016-11-16 00:00:00 (...+0x0p-32s)", 2,
                __LINE__); // period needed
    fail_at_arg("2016-11-16 00:00:00.1 (. ..+0x0p-32s)", 2, __LINE__);
    fail_at_arg("2016-11-16 00:00:00.1 (. ..+0x0p+32s)", 2, __LINE__);

    /*
        doubles
    */
    BAD("1.2.3d");
    BAD("1dd");

    /*
        floats
    */
    BAD("0xgp+1"); // no hex char
    BAD("1p+"); // 0x missing
    BAD(".e3"); // numbers missing
#if 0
    // reading strtod(3p) these don't look legal,
    // but "unfortunatelly" they work
    BAD("0x1p+"); // exponent missing
    BAD("1.0e"); // exponent missing
#endif

    BAD("1 (-0x1.8p+0)"); // only floating numbers have postfix
    fail_at_arg("1.0 -0x1.8p+0)", 3, __LINE__); // read as 2 ints + ')'
    BAD("1.0 (x1.8p+0)");
    BAD("1.0 (-0x1.8p+0");
    BAD("(-0x0p-32)"); // parantheses not allowed

    /*
        chars
    */
    BAD("''");
    BAD("'too many chars'");
    BAD("'\\k'");
    BAD("'\\\\\\'"); // 3 backslashes

    /*
        integers
    */
    BAD("42j");
    BAD("0xgg");
    BAD("0123Fh");

    /*
        MIDI
    */
    // valid identifier + invalid blob
    fail_at_arg("MI [ 0xff 0xff 0xff 0xff ]", 2, __LINE__);
    BAD("MIDI x");
    BAD("MIDI 0xff 0xff 0xff 0xff ]");
    fail_at_arg("2 MIDI [ 0xff ]", 2, __LINE__);
    BAD("MIDI [ 0 0xff 0xff 0xff ]");
    BAD("MIDI [ 0xff 0xff 0xff 0xff");

    /*
        strings
    */
    BAD("\"no endquote");
    BAD("\"\\\"no endquote 2");
    BAD("\\u");
    BAD("\\\\\\"); // 3 backslashes

    /*
        identifier aka symbols
    */
    fail_at_arg("identifier_with-minus-sign", 2, __LINE__);

    /*
        blobs
    */
    BAD("[");
    BAD("[ x ]");
    BAD("[ 1 ]");
    BAD("[ 2 0xff ]");
    BAD("[ 1 0xff 0xff ]");
    BAD("[ 0 0xff ]");
    BAD("[ 1 0xff 123 ]");
    BAD("[ 1 0xff ");
    BAD("[ 1 0xff");

    /*
        long message
    */
    fail_at_arg("\"rtosc is\" true \"ly\" '\a' 0x6rea7 \"library\"",
                5, __LINE__);

#undef BAD
}

int main()
{
    scan_and_print_single();
    scan_and_print_mulitple();
    messages();

    scan_invalid();

    return test_summary();
}
