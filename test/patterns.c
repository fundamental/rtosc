#include <stdlib.h>
#include <rtosc/rtosc.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "common.h"

const char paths[12][32] = {
    "/",
    "#24:",
    "#20:ff",
    "path",
    "path#234/:ff",
    "path#1asdf",
    "foobar#123/::ff",
    "blam/",
    "bfn::c",
    "bfnpar1::c",
    "z{oo,am,it}",
    "flam/{args,bugs}"
};

int error = 0;

//project_via_rtosc_match
int64_t prm(const char *path, const char *args, ...)
{
    char buffer[256];
    va_list va;
    va_start(va, args);
    rtosc_vmessage(buffer, 256, path, args, va);
    va_end(va);

    int64_t result = 0;
    for(int i=0; i<12; ++i)
        result |= ((bool)rtosc_match(paths[i], buffer, NULL)) << i;
    return result;
}

#define rmp rtosc_match_partial
bool rtosc_match_partial(const char *a, const char *b);

int main()
{
    //Check the normal path fragment matching used for the C++ layer
    assert_int_eq(1<<0, prm("/",          ""),
            "Check Simple Empty Path",           __LINE__);
    assert_int_eq(1<<1, prm("19",         ""),
            "Check Basic Enum Matching",         __LINE__);
    assert_int_eq(1<<2, prm("19",         "ff", 1.0, 2.0),
            "Check Enum+Args Matching",          __LINE__);
    assert_int_eq(0<<3, prm("pat",        ""),
            "Check Partial Path Failure",        __LINE__);
    assert_int_eq(1<<3, prm("path",       ""),
            "Check Full Path Match",             __LINE__);
    assert_int_eq(0<<3, prm("paths",      ""),
            "Check Overfull Path Failure",       __LINE__);
    assert_int_eq(1<<4, prm("path123/",   "ff", 1.0, 2.0),
            "Check Composite Path",              __LINE__);
    assert_int_eq(1<<5, prm("path0asdf",  ""),
            "Check Embedded Enum",               __LINE__);
    assert_int_eq(1<<6, prm("foobar23/",  ""),
            "Check Another Enum",                __LINE__);
    assert_int_eq(0<<6, prm("foobar123/", ""),
            "Check Enum Too Large Failure",      __LINE__);
    assert_int_eq(1<<6, prm("foobar122/", ""),
            "Check Enum Edge Case",              __LINE__);
    assert_int_eq(1<<7, prm("blam/",      ""),
            "Check Subpath Match",               __LINE__);
    assert_int_eq(1<<7, prm("blam/blam",  ""),
            "Check Partial Match Of Full Path",  __LINE__);
    assert_int_eq(0<<7, prm("blam",       ""),
            "Check Subpath Missing '/' Failure", __LINE__);
    assert_int_eq(1<<9, prm("bfnpar1",    "c"),
            "Check Optional Arg Case 1",         __LINE__);
    assert_int_eq(1<<9, prm("bfnpar1",    ""),
            "Check Optional Arg Case 2",         __LINE__);
    assert_int_eq(1<<10,prm("zoo",        ""),
            "Check PseudoWild Case 1",           __LINE__);
    assert_int_eq(1<<10,prm("zam",        ""),
            "Check PseudoWild Case 2",           __LINE__);
    assert_int_eq(1<<10,prm("zit",        ""),
            "Check PseudoWild Case 3",           __LINE__);
    assert_int_eq(0<<10,prm("zap",        ""),
            "Check PseudoWild Case 4",           __LINE__);
    assert_int_eq(1<<11,prm("flam/args",  ""),
            "MultiPath Case 1",                  __LINE__);
    assert_int_eq(1<<11,prm("flam/bugs",  ""),
            "MultiPath Case 2",                  __LINE__);
    assert_int_eq(0<<11,prm("flam/error",  ""),
            "MultiPath Case 3",                  __LINE__);


    printf("\n# Suite 2 On Standard Based Matching Alg.\n");
    //Check the standard path matching algorithm
    assert_true(rmp("foobar", "foobar"),  "Check Literal Equality",   __LINE__);
    assert_false(rmp("foocar", "foobar"), "Check Literal Inequality", __LINE__);

    assert_true(rmp("foobar",  "?[A-z]?bar"), "Check Char Pattern Equality", __LINE__);
    assert_false(rmp("foobar",  "?[!A-z]?bar"), "Check Char Pattern Inequality", __LINE__);

    assert_true(rmp("baz", "*"), "Check Trivial Wildcard", __LINE__);
    assert_true(rmp("baz", "ba*"), "Check Wildcard Partial Equality", __LINE__);
    assert_false(rmp("baz", "bb*"), "Check Wildcard Partial Inequality", __LINE__);
    assert_false(rmp("baz", "ba*z"), "Check Invalid Wildcard Partial Inequality", __LINE__);

    assert_true(rmp("lizard52", "lizard#53"), "Check Enum Equality", __LINE__);
    assert_false(rmp("lizard99", "lizard#53"), "Check Enum Inequality", __LINE__);
    assert_false(rmp("lizard", "lizard#53"), "Check Enum Inequality", __LINE__);


    const char *rtosc_match_options(const char *pattern, const char **msg);
    //Check additional extension
    const char *ex_msgA = "A";
    const char *ex_msgB = "B";
    const char *ex_msgF = "F";
    assert_true(rtosc_match_options("{A,B,C,D,E}",  &ex_msgA) != NULL,
            "Verify rtosc_match_options (A)", __LINE__);
    assert_true(rtosc_match_options("{A,B,C,D,E}",  &ex_msgB) != NULL,
            "Verify rtosc_match_options (B)", __LINE__);
    assert_false(rtosc_match_options("{A,B,C,D,E}", &ex_msgF) != NULL,
            "Verify rtosc_match_options (F)", __LINE__);

    return test_summary();
}
