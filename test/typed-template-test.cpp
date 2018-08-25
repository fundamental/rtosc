//#include <rtosc/typed-message.h>
#include "../include/rtosc/typed-message.h"
#include "common.h"
#include <cstdio>

using rtosc::rtMsg;
using rtosc::get;

typedef const char *cstr;
typedef rtMsg<cstr, int32_t> Type1;
typedef rtMsg<cstr, int32_t, typestring_is("send-to")> Type2;
typedef rtMsg<cstr> Type3;
typedef rtMsg<int32_t, int32_t> Type4;
typedef rtMsg<int32_t, float, float> Type5;
typedef rtMsg<int32_t, cstr> Type6;


//Zyn Example Types
#define MKTYPE(x, y, ...) \
    typedef rtMsg<__VA_ARGS__, typestring_is(y)>  x
#define MKSTYP(x,y) \
    typedef rtMsg<typestring_is(y)> x
#define MKPATT(...)
#define MKMATCH(...)
MKTYPE(T_echo, "/echo", cstr, cstr);
MKTYPE(T_free, "/free", cstr, void*);
MKSTYP(T_rmem, "/request-memory");
MKTYPE(T_setp, "/setprogram", char, char);
MKSTYP(T_unpa, "/undo_pause");
MKSTYP(T_unre, "/undo_resume");
MKPATT(T_unch, "/undo_change");
MKSTYP(T_brod, "/broadcast");
MKSTYP(T_clos, "/close-ui");
//XXX ADD SOME HERE
MKTYPE(T_sxmz, "/save_xmz", cstr);
MKTYPE(T_sxiz, "/save_xiz", int, cstr);
MKTYPE(T_lxmz, "/load_xmz", cstr);
MKSTYP(T_rest, "/reset_master");
MKTYPE(T_lxiz, "/load_xiz",  int, cstr);
MKTYPE(T_lprt, "/load-part", int, cstr);
MKTYPE(T_bspt, "/save-bank-part", int, int);
MKTYPE(T_brnm, "/bank-rename", int, cstr);
MKTYPE(T_bsws, "/swap-bank-slots", int, int);
MKTYPE(T_clbs, "/clear-bank-slot", int);

MKSTYP(T_pclr, "/part#16/clear");
MKPATT(T_undo, "/undo");
MKPATT(T_redo, "/redo");
MKMATCH(T_config,  "/config/");
MKMATCH(T_presets, "/presets/");
MKMATCH(T_io,      "/io/");

int main() {
    char buf[1024];
    char buf2[1024];
    rtosc_message(buf, 1024, "/send-to", "si", "Testing", 42);
    rtosc_message(buf2, 1024, "/send-to", "iff", 12, 3.15, 81.0);
    printf("Starting...\n");
    rtMsg<const char *, int32_t> msg{buf};
    assert_str_eq("Testing", get<0>(msg), "Verify get<0>() Operator", __LINE__);
    assert_int_eq(42, get<1>(msg), "Verify get<1>() Operator", __LINE__);

    Type1 m1{buf+1, "send-to"};
    Type2 m2{buf+1};
    Type3 m3{buf};
    Type4 m4{buf2};
    Type5 m5{buf2};
    Type6 m6{buf2};

    assert_true(m1, "Check String Pattern Arg", __LINE__);
    assert_true(m2, "Check String Pattern Type", __LINE__);
    assert_str_eq("Testing", get<0>(m2), "Verify get<0>() Operator On Pattern Match", __LINE__);
    assert_int_eq(42, get<1>(m2), "Verify get<1>() Operator On Pattern Match", __LINE__);
    assert_false(m3, "Check Type Conflict", __LINE__);
    assert_false(m4, "Check Type Conflict", __LINE__);
    assert_true(m5, "Check Type Match", __LINE__);
    assert_false(m6, "Check Type Conflict", __LINE__);

    return test_summary();
};
