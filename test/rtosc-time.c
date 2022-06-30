#include <rtosc/rtosc-time.h>
#include <math.h>
#include "common.h"

void check_secfrac_convert()
{
    struct
    {
        float    f;
        uint64_t secfracs;
    } testvals[] = { { 0.f,   0 },          /* 0 secfracs is always 0 */
                     { 0.49f, 2104534016},  /* 0.49 ~ max(signed int) */
                     { 0.5f,  2147483648},
                     { 0.99999f, 4294924288} };      /* 0.99 ~ max(unsigned int) */

    char teststr_f2s[] = "testvals 0, float -> secfracs";
    char teststr_s2f[] = "testvals 0, secfracs -> float";
    for(size_t i = 0; i < sizeof(testvals)/sizeof(testvals[0]); ++i)
    {
        teststr_f2s[9]++;
        teststr_s2f[9]++;
        uint64_t secfracs = rtosc_float2secfracs(testvals[i].f);
        assert_true(testvals[i].secfracs == secfracs, teststr_f2s, __LINE__);
        float f = rtosc_secfracs2float(testvals[i].secfracs);
        assert_f32_eq(testvals[i].f, f, teststr_s2f, __LINE__);
    }
}

void check_conversions()
{
    rtosc_arg_val_t av;
    time_t tval = 1*365*24*3600 // 1 year
                  +31*24*3600   // +1 month (2nd month of year)
                  +1*24*3600    // +1 day (2nd day of month)
                  +1*3600+60+1; // 1:01:01
    uint64_t secfracs = 1000;
    // => Feb 2, 1971, 1:01:01 (UTC)

    // convert
    rtosc_arg_val_from_time_t(&av, tval, secfracs);

    // convert back
    time_t tval_res = rtosc_time_t_from_arg_val(&av);
    uint64_t secfracs_res = rtosc_secfracs_from_arg_val(&av);
    assert_true(tval_res == tval,
                "Correct time value after conversion", __LINE__);
    assert_true(secfracs_res == secfracs,
                "Correct secfracs value after conversion", __LINE__);

    // convert to params
    struct tm* tpars;
    tpars = rtosc_params_from_arg_val(&av);
    // posix: year is subtracted by 1900
    assert_int_eq(71, tpars->tm_year, "params: year", __LINE__);
    // posix: month range is [0,11]
    assert_int_eq(1, tpars->tm_mon, "params: mon", __LINE__);
    // day is ~2, not fully reliable, depends on timezone:
    assert_true(tpars->tm_mday>=1 && tpars->tm_mday<=3, "params: mday", __LINE__);
    // (hour depends on timezone, not reliable)
    assert_int_eq(1, tpars->tm_min, "params: min", __LINE__);
    assert_int_eq(1, tpars->tm_sec, "params: sec", __LINE__);

    // convert back from params
    rtosc_arg_val_t av2;
    rtosc_arg_val_from_params(&av2, tpars, secfracs);
    assert_char_eq(av.type, av2.type,
                   "rtosc_arg_val_from_params: type", __LINE__);
    assert_true(av.val.t == av2.val.t,
                "rtosc_arg_val_from_params: value", __LINE__);
}

void check_immediatelly()
{
    rtosc_arg_val_t av;
    rtosc_arg_val_immediatelly(&av);
    assert_char_eq('t', av.type, "rtosc_arg_val_immediatelly type", __LINE__);
    assert_int_eq(1, av.val.t, "rtosc_arg_val_immediatelly val", __LINE__);

    bool is_imm = rtosc_arg_val_is_immediatelly(&av);
    assert_true(is_imm, "rtosc_arg_val_is_immediatelly", __LINE__);
}

int main()
{
    check_secfrac_convert();
    check_conversions();
    check_immediatelly();

    return test_summary();
}
