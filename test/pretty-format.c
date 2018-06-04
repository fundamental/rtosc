#include <assert.h>
#include <rtosc/rtosc.h>
#include <rtosc/pretty-format.h>
#include "common.h"

rtosc_arg_val_t scanned[32];

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
    memset(strbuf, 0x7f, strbuflen); /* init with rubbish */

    int num = rtosc_count_printed_arg_vals(arg_val_str);
    assert(num < 32);
    // note: when using this, the next step is usually
    //       rtosc_arg_val_t scanned[num];
    //       in this case, however, we need the variable globally accessible
    size_t rd = rtosc_scan_arg_vals(arg_val_str, scanned, num,
                                    strbuf, strbuflen);

    strcpy(tc_full, "scan \"");
    strncat(tc_full, tc_base, tc_len-7);
    strncat(tc_full, "\" (read exactly the input string)",
            tc_len - strlen(tc_full));
    assert_int_eq(strlen(arg_val_str), rd, tc_full, line);

    size_t len = 128;
    char printed[len];
    memset(printed, 0x7f, len); /* init with rubbish */
    size_t written = rtosc_print_arg_vals(scanned, num, printed, len, opt, 0);

    const char* exp_print = _exp_print ? _exp_print : arg_val_str;
    strcpy(tc_full, "print \"");
    strncat(tc_full, tc_base, tc_len-8);
    strncat(tc_full, "\" (value = value before scan)",
            tc_len - strlen(tc_full));
    assert_str_eq(exp_print, printed, tc_full, line);

    strcpy(tc_full, "print \"");
    strncat(tc_full, tc_base, tc_len-8);
    strncat(tc_full, "\" (return value check)", tc_len - strlen(tc_full));
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
    rtosc_print_options lossy = ((rtosc_print_options) { false, 3, " ", 80,
                                                         true });
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
    rtosc_print_options prec6 = ((rtosc_print_options) { true, 6, " ", 80,
                                                         true });
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
    rtosc_print_options prec0 = ((rtosc_print_options) { true, 0, " ", 80,
                                                         true });
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
    rtosc_print_options shortline_9 = ((rtosc_print_options) { true, 3, " ", 9,
                                                               true });
    check("an_identifier_42", NULL, "a simple identifier", __LINE__);
    check("_", NULL, "the identifier \"_\"", __LINE__);
    check("truely falseeee infinite nilpferd immediatelyly nowhere MIDINOTE",
          NULL,
          "reserved keywords with letters appended are identifiers", __LINE__);
    check_alt("\"identifier_in_quotes\"S", NULL, "an identifier that has, but "
              "does not need quotes", __LINE__, "identifier_in_quotes");
    check("\"identifier with whitespace\"S", NULL,
          "An identifier with whitespace", __LINE__);
    check("\"A more \\\"complicated\\\" identifier!\"S", NULL,
          "An identifier with special characters", __LINE__);
    check("\"identi\"\\\n    \"fi\"\\\n    \"er\"", &shortline_9,
          "a long identifier", __LINE__);

    /*
        blobs
    */
    check("BLOB [6 0x72 0x74 0x6f 0x73 0x63 0x00]", NULL,
          "a six bytes blob", __LINE__);
    check("BLOB [0]", NULL, "an empty blob", __LINE__);
    check("BLOB [2 0x00 0x01] BLOB [1 0x02]", NULL,
          "two consecutive blobs", __LINE__);

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
    rtosc_print_options shortline = ((rtosc_print_options) { true, 3, " ", 10,
                                                             true });
    check_alt("\"0123456789012345678\"", &shortline,
              "string exceeding line length", __LINE__,
              "\"0123456\"\\\n"
              "    \"789\"\\\n"
              "    \"012\"\\\n"
              "    \"345\"\\\n"
              "    \"678\"");
    check("\"0123456\"\\\n    \"789\"", &shortline,
          "broken down string", __LINE__);
    check_alt("BLOB [1 0xff]", &shortline, "long blob is being broken",
              __LINE__, "BLOB [1\n    0xff]");
    check_alt("false false 'a'", &shortline,
              "break between arguments", __LINE__, "false\n    false\n    'a'");
}

void scan_and_print_mulitple()
{
    check("0 1 2 true", NULL, "3 ints and one bool", __LINE__);
    check("\"012345678901234567890123\" 'c'", NULL,
          "a string and a char", __LINE__);
    // interesting, because they overlap in the "buffer for strings"
    // (see "strbuf" above):
    check("\"1\" BLOB [1 0xff] \"2\"", NULL,
          "a string, a blob and a string", __LINE__);
}

void arrays()
{
    check("[0 1 2]", NULL, "simple integer array", __LINE__);
    check("[]", NULL, "empty array", __LINE__);

    rtosc_print_options shortline = ((rtosc_print_options)
                                     { true, 3, " ", 12, true });
    // TODO: arrays with strings print newline positions wrong
    //       write *one* generic function rtosc_insert_newlines?
    check("[\"123\" \"45\" \"\"\\\n    \"6\"]", &shortline,
          "array with linebreak", __LINE__);

    check("[1 2 3] ['a' 'b'] [] [\"Hello World\"]", NULL,
          "Consecutive arrays", __LINE__);

    check("[[0 1] [] [2 3]]", NULL, "arrays inside arrays", __LINE__);

    check("[true false]", NULL, "true and false have the same type", __LINE__);
}

void ranges()
{
    rtosc_print_options uncompressed = ((rtosc_print_options) { false, 3, " ",
                                                                10, false });
    rtosc_print_options simplefloats = ((rtosc_print_options) { false, 2, " ",
                                                                80, true });

    /*
        with delta
     */
    check_alt("1 ... 7", &uncompressed,
              "a simple upwards integer range", __LINE__,
              "1 2 3 4 5\n"
              "    6 7");
    check_alt("-3 ... -5", &uncompressed,
              "a simple downwards integer range", __LINE__, "-3 -4 -5");
    check_alt("3 ... 4", &uncompressed,
              "a very short range, uncompressed", __LINE__, "3 4");
    check("3 ... 4", &simplefloats, "a very short range, compressed", __LINE__);
    check_alt("2.00 1.40 ... -0.40 -1.00", &simplefloats,
              "a simple float range", __LINE__,
              "2.00 1.40 0.80 ... -0.40 -1.00"); // TODO: bug: input != output
                                                 //       fix in later commit?
    check_alt("'z' 'x' ... 'r'", &uncompressed,
              "a simple downward char range", __LINE__,
              "'z' 'x'\n"
              "    'v'\n"
              "    't'\n"
              "    'r'");
    check_alt("0.0f 0.3333f ... 1.0f", &uncompressed,
              "float range which only fits approximatelly", __LINE__,
              "0.000\n    0.333\n    0.667\n    1.000");
    check_alt("[3...0]", &uncompressed,
              "downward range in an array", __LINE__,
              "[3 2 1 0]");
    check_alt("1 3 ... 11 13 ... 19", &uncompressed,
              "two almost subsequent ranges", __LINE__,
              "1 3 5 7 9\n"
              "    11 13\n"
              "    15 17\n"
              "    19");
    check_alt("[ 1 ... 3 ]", &uncompressed,
              "range with delta 1 in an array", __LINE__,
              "[1 2 3]");
    check_alt("[3...0]", &uncompressed,
              "range with delta -1 in an array", __LINE__,
              "[3 2 1 0]");

    /*
        without delta
     */
    check_alt("3x0x0", &uncompressed, "range of 3 zeros", __LINE__, "0 0 0");
    check_alt("[ 2x0.42f ]", &uncompressed, "array with range of 2 floats",
              __LINE__, "[0.420\n    0.420]");
    check_alt("2x[0 1]", &uncompressed,
              "range of arrays", __LINE__, "[0 1] [0\n    1]");
    check("[127 104 64 106 7x64 101 64 64 64 92 112x64]", NULL,
          "long array", __LINE__);
    check_alt("[127 104 64 106 64 64 64 64 64 64 64 101 64 64 64 92 112x64]", NULL,
          "long array", __LINE__, "[127 104 64 106 7x64 101 64 64 64 92 112x64]");

    /*
        combined
     */
    check_alt("3x0.5 1.0 ... 2.5", &uncompressed,
              "combined ranges", __LINE__,
              "0.500\n    0.500\n    0.500\n"
              "    1.000\n    1.500\n    2.000\n    2.500");
    /* combined, but not yet implemented: */
#if 0
    check_alt("3x-6 ... -3", &uncompressed, "combined ranges (2)", __LINE__,
              "-6 -6 -6\n-5 -4 -3");
    check_alt("'a' 'b' ... 3x'f'", &uncompressed,
              "combined ranges (3)", __LINE__,
              "'a' 'b' 'c' 'd' 'e' 'f' 'f' 'f'");
#endif

    /*
        infinite ranges
     */
    check("[1 ... ]", NULL, "delta-less infinite range (1)", __LINE__);
    check("[\"Next Effect\"S ... ]", NULL,
          "delta-less infinite range (2)", __LINE__);
    check_alt("[false...]", NULL, "delta-less infinite range (3)", __LINE__,
              "[false ... ]");
    check("[1 0 0 ... ]", NULL, "delta-less infinite range (4)", __LINE__);
    check("[0 1 ... ]", NULL, "infinite range with delta", __LINE__);
    check("[true false false ... ]", NULL,
          "endless range after \"true false false\"", __LINE__);
    check("[[0 1] ... ]", NULL, "range of arrays", __LINE__);
    check("[true false ... ]", NULL, "alternating true-false-range", __LINE__);
    assert('-' == scanned[2].type);
    assert_int_eq(0, scanned[2].val.r.num,
                  "true-false range is inifinite", __LINE__);
    assert_int_eq(1, scanned[2].val.r.has_delta,
                  "true-false range has delta", __LINE__);
}

// most tests here are reverse to those in ranges()
void scan_ranges()
{
    /*
        no range conversion
     */
    rtosc_print_options uncompressed = ((rtosc_print_options) { false, 3, " ",
                                                                80, false });
    rtosc_print_options simplefloats = ((rtosc_print_options) { false, 2, " ",
                                                                80, true });
    check("0 1 2 3", NULL, "too less args for range conversion", __LINE__);
    check("0.00 1.00 2.00 3.00 4.00", &simplefloats,
          "wrong type for range conversion", __LINE__);
    check("0 1 2 3 4", &uncompressed,
          "no range conversion due to opts", __LINE__);

    /*
        with delta
     */
    check_alt("1 2 3 4 5 6 7", NULL,
              "convert to simple upwards integer range", __LINE__,
              "1 ... 7");
    check_alt("-3 -4 -5 -6 -7", NULL,
              "convert to simple downwards integer range", __LINE__,
              "-3 ... -7");
    check_alt("'z' 'x' 'v' 't' 'r'", NULL,
              "convert to simple downward char range", __LINE__,
              "'z' 'x' ... 'r'");
    check_alt("[4 3 2 1 0]", NULL,
              "convert to downward range in an array", __LINE__,
              "[4 ... 0]");
    check_alt("1 3 5 7 9 13 15 17 19 21", NULL,
              "convert to two almost subsequent ranges", __LINE__,
              "1 3 ... 9 13 15 ... 21");
    check_alt("[ 1 2 3 4 5 ]", NULL,
              "convert to range with delta 1 in an array", __LINE__,
              "[1 ... 5]");
    check_alt("[5 4 3 2 1 0]", NULL,
              "convert to range with delta -1 in an array", __LINE__,
              "[5 ... 0]");

    /*
        without delta
     */
    check_alt("0 0 0 0 0", NULL,
              "convert to range of 5 zeros", __LINE__, "5x0");
    check_alt("[ 0h 0h 0h 0h 0h ]", NULL,
              "convert to array with range of 5 huge ints", __LINE__, "[5x0h]");
    check_alt("[0 1] [0 1] [0 1] [0 1] [0 1]", NULL,
              "convert to range of arrays", __LINE__, "5x[0 1]");
    /*
        combined
     */
    check_alt("0.5 0.5 0.5 0.5 0.5 'a' 'b' 'c' 'd' 'e'", NULL,
              "convert to combined ranges", __LINE__,
              "5x0.50 (0x1p-1) 'a' ... 'e'");
    /* combined, but not yet implemented: */
#if 0
    check_alt("-6 -6 -6 -6 -6 -6 -5 -4 ... 0", NULL,
              "convert to combined ranges (2)", __LINE__,
              "6x-6 -5 ... 0");
    check_alt("'a' 'b' 'c' 'd' 'e' 'f' 'f' 'f' 'f' 'f' 'f'", NULL,
              "convert to combined ranges (3)", __LINE__,
              "'a' ... 'f' 6x'f'");
#endif

    /*
        endless ranges
     */
    // (you cannot convert fixed size ranges to endless ranges)
}

void fail_at_arg(const char* arg_val_str, int exp_fail, int line)
{
    int tc_len = 256;
    char tc_full[tc_len]; // descr. for full testcase name

    int num = rtosc_count_printed_arg_vals(arg_val_str);

    strcpy(tc_full, "find 1st invalid arg in \"");
    strncat(tc_full, arg_val_str, tc_len-25);
    strncat(tc_full, "\"", tc_len);
    assert_int_eq(exp_fail, -num, tc_full, line);
}

void messages()
{

    int num = rtosc_count_printed_arg_vals_of_msg("not beginning with a '/'");
    assert_int_eq(-1, num, "return -1 if the message does not start"
                           "with a slash", __LINE__);

    int strbuflen = 256;
    char strbuf[strbuflen];
    memset(strbuf, 0x7f, strbuflen); /* init with rubbish */
    int msgbuflen = 256;
    char msgbuf[msgbuflen];
    memset(msgbuf, 0x7f, msgbuflen); /* init with rubbish */
    const char* input = "%this is a savefile\n"
                        "/noteOn 0 0 0 % a noteOn message";

    num = rtosc_count_printed_arg_vals_of_msg(input);
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
    memset(printed, 0x7f, len); /* init with rubbish */
    rtosc_print_options shortline = ((rtosc_print_options) { true, 3, " ", 7,
                                                             true });
    size_t written = rtosc_print_message("/noteOn", scanned, num,
                                         printed, len, &shortline, 0);

    const char* exp = "/noteOn\n    0 0\n    0";
    assert_str_eq(exp, printed, "print a message", __LINE__);
    assert_int_eq(strlen(exp), written,
                  "print a message and return written number of bytes",
                  __LINE__);

    // scan message that has no parameters
    // => a following argument is not considered as an argument of
    //    the first message
    rd = rtosc_scan_message("/first_param\n"
                            "/second_param 123", msgbuf, msgbuflen,
                            scanned, 0, strbuf, strbuflen);
    assert_int_eq(13, rd, "scan message without arguments", __LINE__);
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
    BAD("BLOB [");
    BAD("BLOB [ x ]");
    BAD("BLOB [ 1 ]");
    BAD("BLOB [ 2 0xff ]");
    BAD("BLOB [ 1 0xff 0xff ]");
    BAD("BLOB [ 0 0xff ]");
    BAD("BLOB [ 1 0xff 123 ]");
    BAD("BLOB [ 1 0xff ");
    BAD("BLOB [ 1 0xff");

    /*
        arrays
    */
    // note: array start and array args each do count
    fail_at_arg("[0 1 2", 4, __LINE__);
    fail_at_arg("[0 2h]", 3, __LINE__);
//  this bad array is currently not detected (TODO):
//  fail_at_arg("[0.0 1 ... 5]", 3, __LINE__);

    /*
        ranges
    */
    // the results are sometimes a bit strange here,
    // but let's at least document that they do not change
    BAD("... 25");
    BAD("4 ...");
    BAD("6 ... 12h"); // different types
    // there's no natural number "n" with 0.25n = 1.1:
    fail_at_arg("0.0 0.25 ... 1.1", 2, __LINE__);
    BAD("1.9f ... 0f");
    fail_at_arg("[ ... 3 ]", 2, __LINE__); // what is the range start here?
    BAD("\"ranges don't work \" ... \"with strings\"");
    BAD("-4 ... -4");
    BAD("[] ... [1 2 3]"); // no arrays in ranges
    // this has been disallowed (intercepting ranges):
    // was the intention "... 11 12 13 14 15" or "... 11 13 15" ?
    fail_at_arg("1 3 ... 11 ... 15", 5 , __LINE__);

    BAD("2x");

    // infite ranges
    BAD("1 ..."); // not allowed outside of bundle

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
    arrays();
    scan_ranges();
    ranges();

    messages();

    scan_invalid();

    return test_summary();
}
