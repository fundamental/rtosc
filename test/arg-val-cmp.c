#include <stdint.h>
#include <rtosc/arg-val-cmp.h>
#include "common.h"

enum {
    gt,
    lt,
    eq
};

rtosc_arg_val_t l[3], r[3]; // left, right
const int tc_len = 1024;
char tc_full[1024]; // descr. for full testcase name

/*
    helper functions
 */
void cmp_1(int exp,
           rtosc_arg_val_t* lhs, rtosc_arg_val_t* rhs,
           size_t lsize, size_t rsize,
           const rtosc_cmp_options* opt,
           const char* ldesc, const char* rdesc, int line)
{
    printf("cmp1(%c, %c)\n", lhs->type, rhs->type);
    int res = rtosc_arg_vals_cmp(lhs, rhs, lsize, rsize, opt);

    *tc_full = 0;
    strncat(tc_full, ldesc, tc_len-1);
    const char* sgn = "ERROR";
    switch(exp)
    {
        case gt: sgn = " >(3way)"; break;
        case eq: sgn = "==(3way)"; break;
        case lt: sgn = " <(3way)"; break;
    }

    snprintf(tc_full, tc_len, "\"%s\" %s \"%s\"", ldesc, sgn, rdesc);

    switch(exp)
    {
        case gt: assert_int_eq(1, res  > 0, tc_full, line); break;
        case eq: assert_int_eq(1, res == 0, tc_full, line); break;
        case lt: assert_int_eq(1, res  < 0, tc_full, line); break;
    }

    int exp_eq = (exp == eq);

    sgn = exp_eq ? "==(bool)" : "!=(bool)";
    snprintf(tc_full, tc_len, "\"%s\" %s \"%s\"", ldesc, sgn, rdesc);

    res = rtosc_arg_vals_eq(lhs, rhs, lsize, rsize, opt);
    assert_int_eq(exp_eq, res, tc_full, line);
}

void cmp_gt(rtosc_arg_val_t* lhs, rtosc_arg_val_t* rhs,
            size_t lsize, size_t rsize,
            const rtosc_cmp_options* opt,
            const char* ldesc, const char* rdesc, int line)
{
    cmp_1(gt, lhs, rhs, lsize, rsize, opt, ldesc, rdesc, line);
    cmp_1(lt, rhs, lhs, rsize, lsize, opt, rdesc, ldesc, line);
    cmp_1(eq, lhs, lhs, lsize, lsize, opt, ldesc, ldesc, line);
    cmp_1(eq, rhs, rhs, rsize, rsize, opt, rdesc, rdesc, line);
}

/*
    tests
 */

void ints()
{
    l[0].type = r[0].type = 'i';
    l[0].val.i = 12345;
    r[0].val.i = 42;

    cmp_gt(l, r, 1, 1, NULL, "12345", "42", __LINE__);
}

void asn_special(rtosc_arg_val_t* l, rtosc_arg_val_t* r, char type)
{
    l->type = r->type = type;
    if(type == 'T')
        l->val.T = r->val.T = 1;
    else if(type == 'F')
        l->val.T = r->val.T = 0;
}

void special()
{
    asn_special(l, r, 'N');
    cmp_1(eq, l, r, 1, 1, NULL, "nil", "nil", __LINE__);
    asn_special(l, r, 'F');
    cmp_1(eq, l, r, 1, 1, NULL, "false", "false", __LINE__);
    asn_special(l, r, 'T');
    cmp_1(eq, l, r, 1, 1, NULL, "true", "true", __LINE__);
    asn_special(l, r, 'I');
    cmp_1(eq, l, r, 1, 1, NULL, "inf", "inf", __LINE__);
}

void floats()
{
    l[0].type = r[0].type = 'f';
    l[0].val.f = 1.0f;
    r[0].val.f = -1.0f;

    cmp_gt(l, r, 1, 1, NULL, "1.0f", "-1.0f", __LINE__);

    rtosc_cmp_options very_tolerant = { 2.0 };

    cmp_1(eq, l, r, 1, 1, &very_tolerant,
          "1.0f", "-1.0f (2.0 tolerance)", __LINE__);
    cmp_1(eq, r, l, 1, 1, &very_tolerant,
          "-1.0f", "1.0f (2.0 tolerance)", __LINE__);
}

void doubles()
{
    l[0].type = r[0].type = 'd';
    l[0].val.d = 123456790.123456789;
    r[0].val.d = 123456789.0;

    cmp_gt(l, r, 1, 1, NULL,
           "larger double value", "smaller double value", __LINE__);

    l[0].type = r[0].type = 'd';
    l[0].val.d = 1.0f;
    r[0].val.d = -1.0f;

    cmp_gt(l, r, 1, 1, NULL, "1.0", "-1.0", __LINE__);

    rtosc_cmp_options very_tolerant = { 2.0 };

    cmp_1(eq, l, r, 1, 1, &very_tolerant,
          "1.0", "-1.0 (2.0 tolerance)", __LINE__);
    cmp_1(eq, r, l, 1, 1, &very_tolerant,
          "-1.0", "1.0 (2.0 tolerance)", __LINE__);
}

void huge_ints()
{
    l[0].type = r[0].type = 'h';
    l[0].val.h = 5000000000;
    r[0].val.h = -5000000000;

    cmp_gt(l, r, 1, 1, NULL, "5000000000", "-5000000000", __LINE__);
}

void timestamps()
{
    l[0].type = r[0].type = 't';
    l[0].val.t = 0xFFFF0000;
    r[0].val.t = 0x00000002;

    cmp_gt(l, r, 1, 1, NULL, "future", "nearly epoch", __LINE__);

    l[0].val.t = 0; // epoch
    r[0].val.t = 1; // immediately
    cmp_gt(l, r, 1, 1, NULL, "immediately", "epoch", __LINE__);

    l[0].val.t = 0xFFFFFFFF; // future
    cmp_gt(l, r, 1, 1, NULL, "immediately", "future", __LINE__);
}

void midi()
{
    l[0].type = r[0].type = 'm';
    l[0].val.m[0] = l[0].val.m[1] = l[0].val.m[2] = 0; l[0].val.m[3] = 1;
    r[0].val.m[0] = r[0].val.m[1] = r[0].val.m[2] = r[0].val.m[3] = 0;

    cmp_gt(l, r, 1, 1, NULL, "MIDI event 1", "MIDI event 2", __LINE__);
}

void strings()
{
    l[0].type = l[1].type = 's';

    l[0].val.s = "rtosc rtosc rtosc";
    l[1].val.s = "rtosc";
    r[0].val.s = "rt0sc";
    r[1].val.s = "";

    cmp_gt(l, r, 1, 1, NULL, "one string", "another string", __LINE__);
    cmp_gt(l, l+1, 1, 1, NULL, "one string", "subset string", __LINE__);
    cmp_gt(r, r+1, 1, 1, NULL, "normal string", "empty string", __LINE__);
}

void blobs()
{
    l[0].type = 'b';
    l[0].val.b.len = 14;
    l[0].val.b.data = (uint8_t*)"rtosc_is_awful";

    l[1].type = 'b';
    l[1].val.b.len = 0;
    l[1].val.b.data = (uint8_t*)"";

    r[0].type = 'b';
    r[0].val.b.len = 16;
    r[0].val.b.data = (uint8_t*)"rtosc_is_awesome";

    r[1].type = 'b';
    r[1].val.b.len = 11;
    r[1].val.b.data = (uint8_t*)"rtosc_is_aw";

    cmp_gt(l, r, 1, 1, NULL, "one blob", "another blob", __LINE__);
    cmp_gt(r, r+1, 1, 1, NULL, "one blob", "subset blob", __LINE__);
    cmp_gt(l, l+1, 1, 1, NULL, "normal blob", "empty blob", __LINE__);
}

void arrays()
{
    // fill three arrays, both with the ints 1, 2, 3 resp. 1, 2, 2
    rtosc_arg_val_t la[16], ra[16]; // left array, right array
    la[0].type = ra[0].type = 'a';
    la[0].val.a.type = ra[0].val.a.type = 'i';
    la[0].val.a.len = ra[0].val.a.len = 3;
    la[1].type = la[2].type = la[3].type =
    ra[1].type = ra[2].type = ra[3].type = 'i';
    la[1].val.i = 1; la[2].val.i = 2; la[3].val.i = 3;
    ra[1].val.i = 1; ra[2].val.i = 2; ra[3].val.i = 2;

    // same size
    cmp_gt(la, ra, 4, 4, NULL, "[1 2 3]", "[1 2 2]", __LINE__);

    // different size
    ra[0].val.a.len = 2;
    ra[3].val.i = 3;
    cmp_gt(la, ra, 4, 4, NULL, "[1 2 3]", "[1 2] 3", __LINE__);
}

void ranges()
{
    rtosc_arg_val_t l[8], r[8];

    // generate range 1 ... 5 inside of an array
    l[0].type = 'a';
    l[0].val.a.len = 3; // number of arg_val_t inside
    l[0].val.a.type = 'i';
    l[1].type = '-';
    l[1].val.r.has_delta = 1;
    l[1].val.r.num = 5;
    l[2].type = 'i';
    l[2].val.i = 1;
    l[3].type = 'i';
    l[3].val.i = 1;

    r[0].type = 'a';
    r[0].val.a.len = 5;
    r[0].val.a.type = 'i';
    for(size_t i = 0; i < 5; ++i) {
        r[i+1].type = 'i';
        r[i+1].val.i = i+1;
    }

    cmp_1(eq, l+1, r+1, 3, 5, NULL,
          "range 1 ... 5", "integers 1 2 3 4 5", __LINE__);

    cmp_1(eq, l, r, 4, 6, NULL,
          "[1 ... 5]", "[1 2 3 4 5]", __LINE__);

    // let l+1 = 1 ... 5 6 7
    // and r+1 = 1 2 3 4 ... 7
    l[4].val.i = 6;
    l[5].val.i = 7;
    l[4].type = l[5].type = 'i';
    r[4].type = '-';
    r[4].val.r.has_delta = 1;
    r[4].val.r.num = 4;
    r[5].type = 'i';
    r[5].val.i = 1;
    r[6].type = 'i';
    r[6].val.i = 4;

    cmp_1(eq, l+1, r+1, 5, 6, NULL,
          "1 ... 5 6 7", "1 2 3 ... 7", __LINE__);

    // keep    l+1 = 1 ... 5 6 7
    // and let r+1 = 1 2 3 4 ...
    r[4].val.r.num = 0;
    //r[6].type = 'x';

    cmp_1(eq, l+1, r+1, 5, 6, NULL,
          "1 ... 5 6 7", "1 2 3 4 ...", __LINE__);

    // let      l+1 = 1 ... 5 6 6
    // and keep r+1 = 1 2 3 4 ...
    l[5].val.i = 6;
    // careful! r and l are swapped!
    cmp_gt(r+1, l+1, 6, 5, NULL, "infinite range", "integers", __LINE__);

    // let      l+1 = 1 ... 7
    // and keep r+1 = 1 2 3 4 ...
    l[1].val.r.num = 7;
    cmp_1(eq, l+1, r+1, 3, 6, NULL, "finite range", "infinite range", __LINE__);

    // let l = 1 2 3 ...
    // and r = 1 2 ...
    l[0].type = l[1].type = 'i';
    l[0].val.i = 1; l[1].val.i = 2;
    l[2].type = '-';
    l[2].val.r.has_delta = 1;
    l[2].val.r.num = 0;
    l[3].type = l[4].type = 'i';
    l[3].val.i = 3; l[4].val.i = 1;

    r[0].type = 'i';
    r[0].val.i = 1;
    r[1].type = '-';
    r[1].val.r.has_delta = 1;
    r[1].val.r.num = 0;
    r[2].type = r[3].type = 'i';
    r[2].val.i = 1; r[3].val.i = 2;

    cmp_1(eq, l, r, 5, 4, NULL,
          "infinite range", "infinite_range", __LINE__);

    // let l = 1 1 1
    // and r = 1 1 ...
    l[1].val.i = 1;
    l[2].type = 'i';
    l[2].val.i = 1;
    r[1].val.r.has_delta = 0;
    r[2].val.i = 1;

    cmp_1(eq, l, r, 3, 3, NULL,
      "numbers", "infinite_range (no delta)", __LINE__);

    // let      l = 1
    // and keep r = 1 1 ...
    cmp_1(eq, l, r, 1, 3, NULL,
      "nothing", "infinite_range (no delta)", __LINE__);

    // let      l = 1 ...
    // and keep r = 1 1 ...
    l[0].type = '-';
    l[0].val.r.num = 0;
    l[0].val.r.has_delta = 0;
    l[1].type = 'i';
    l[1].val.i = 1;
    cmp_1(eq, l, r, 2, 3, NULL,
      "infinite_range (no delta)", "infinite_range (no delta)", __LINE__);


    // keep     l = 1 ...
    // and let  r = 1 ...
    r[0].type = '-';
    r[0].val.r.num = 0;
    r[0].val.r.has_delta = 0;
    r[1].type = 'i';
    r[1].val.i = 1;
    cmp_1(eq, l, r, 2, 2, NULL, "1 ...", "1 ...", __LINE__);
}

void different_types()
{
    l[0].type = 'i';
    l[0].val.i = 0;
    r[0].type = 'h';
    r[0].val.h = 1;

    // 1 > 0, but 'i' > 'h' (alphabetically)
    cmp_gt(l, r, 1, 1, NULL, "int type", "huge int type", __LINE__);
}

void multiple_args()
{
    l[0].type = r[0].type = 'i';
    l[0].val.i = r[0].val.i = 42;
    l[1].type = r[1].type = 'T';
    l[1].val.T = r[1].val.T = 1;
    l[2].type = r[2].type = 's';
    l[2].val.s = "1";
    r[2].val.s = "0"; //different values

    cmp_gt(l, r, 3, 3, NULL, "multiple args 1", "mutliple args 2", __LINE__);

    l[0].type = r[0].type = 't';
    l[0].val.t = r[0].val.t = 1;
    l[1].type = 'f';
    r[1].type = 'd';  //different types
    l[1].val.f = 1.0f;
    r[1].val.d = 1.0;

    cmp_gt(l, r, 2, 2, NULL, "multiple args 3", "multiple args 4", __LINE__);
}

void different_sizes()
{
    l[0].type = l[1].type = 'i';
    l[0].val.i = l[1].val.i = 42;

    cmp_gt(l, l, 2, 1, NULL, "size-2-array", "size-1-array", __LINE__);
}

int main()
{
    ints();
    special();
    floats();
    doubles();
    huge_ints();
    timestamps();
    midi();
    blobs();
    arrays();
    ranges();
    different_types();
    multiple_args();
    different_sizes();

    return test_summary();
}
