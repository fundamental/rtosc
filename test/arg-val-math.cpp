#include <assert.h>
#include <rtosc/rtosc.h>
#include <rtosc/arg-val-math.h>
#include <rtosc/arg-val-cmp.h>
#include "common.h"

typedef int (*unary_arg_func_ptr)(rtosc_arg_val_t*);
typedef int (*binary_arg_func_ptr)(const rtosc_arg_val_t *,
                                   const rtosc_arg_val_t *,
                                   rtosc_arg_val_t*);

const rtosc_cmp_options tolerance = { 0.00000000001 };

/*
    helper functions
*/
void test_binary(char type, binary_arg_func_ptr op, char opchar,
                 double arg1, double arg2, double exp)
{
    rtosc_arg_val_t a1, a2, res, e;
    rtosc_arg_val_from_double(&a1, type, arg1);
    rtosc_arg_val_from_double(&a2, type, arg2);
    rtosc_arg_val_from_double(&e,  type,  exp);

    (*op)(&a1, &a2, &res);
    int cmp_res = rtosc_arg_vals_eq(&e, &res, 1, 1, &tolerance);

    char str[16];
    snprintf(str, 16, "'%c' %c '%c'", type, opchar, type);
    assert_int_eq(1, cmp_res, str, __LINE__);
}

void test_unary(char type, unary_arg_func_ptr op, char opchar,
                double arg, double exp)
{
    rtosc_arg_val_t a, e;
    rtosc_arg_val_from_double(&a, type, arg);
    rtosc_arg_val_from_double(&e, type, exp);

    (*op)(&a);
    int cmp_res = rtosc_arg_vals_eq(&e, &a, 1, 1, &tolerance);

    char str[16];
    snprintf(str, 16, "%c'%c'", opchar, type);
    assert_int_eq(1, cmp_res, str, __LINE__);
}

void test_add(char type, double arg1, double arg2, double exp)
{
    test_binary(type, rtosc_arg_val_add, '+', arg1, arg2, exp);
    test_binary(type, rtosc_arg_val_sub, '-', exp, arg1, arg2);
}

void test_mult(char type, double arg1, double arg2, double exp)
{
    test_binary(type, rtosc_arg_val_mult, '*', arg1, arg2, exp);
    if(arg2 != 0.0)
        test_binary(type, rtosc_arg_val_div, '/', exp, arg1, arg2);
}

void test_to_int(char type, double arg)
{
    rtosc_arg_val_t a;
    int res;

    rtosc_arg_val_from_int(&a, type, arg);
    rtosc_arg_val_to_int(&a, &res);

    char str[20];
    snprintf(str, 20, "(int)'%c'", type);
    assert_int_eq(arg, res, str, __LINE__);

    rtosc_arg_val_from_double(&a, type, arg);
    rtosc_arg_val_to_int(&a, &res);

    snprintf(str, 20, "(int)(double)'%c'", type);
    assert_int_eq(arg, res, str, __LINE__);
}

void test_null(char type)
{
    rtosc_arg_val_t a, e;
    rtosc_arg_val_null(&a, type);
    rtosc_arg_val_from_double(&e, type, 0.0);

    int cmp_res = rtosc_arg_vals_eq(&e, &a, 1, 1, NULL);

    char str[16];
    snprintf(str, 16, "(0)'%c'", type);
    assert_int_eq(1, cmp_res, str, __LINE__);
}

void special_null_test()
{
    rtosc_arg_val_t a;
    rtosc_arg_val_null(&a, 't');
    assert_int_eq(0, a.val.t, "(0)'t'", __LINE__);

    rtosc_arg_val_null(&a, 's');
    assert_null(a.val.s, "(0)'s'", __LINE__);

    rtosc_arg_val_null(&a, 'r');
    assert_int_eq(0, a.val.i, "(0)'r'", __LINE__);

    rtosc_arg_val_null(&a, 'T');
    assert_int_eq(a.val.T, 0, "(0)'T'", __LINE__);
    assert_char_eq(a.type, 'F', "(0)'T' (type)", __LINE__);

    rtosc_arg_val_null(&a, 'S');
    assert_null(a.val.s, "(0)'S'", __LINE__);

    rtosc_arg_val_null(&a, 'F');
    assert_int_eq(a.val.T, 0, "(0)'F'", __LINE__);
    assert_char_eq(a.type, 'F', "(0)'F' (type)", __LINE__);
}

/*
    all tests
*/
int main()
{
    test_add('T', 0, 0, 0);
    test_add('T', 0, 1, 1);
    test_add('T', 1, 0, 1);
    test_add('T', 1, 1, 0);
    test_add('i', 42, -123, -81);
    test_add('c', 'A', ' ', 'a');
    test_add('h', 4000000000, 4000000000, 8000000000);
    test_add('f', 1.23, -100, -98.77);
    test_add('d', 1.234523452345, -1.111111111111, 0.1234123412341234);

    // avoid division by '0' in test_mult('T', 0, 0, 0):
    test_binary('T', rtosc_arg_val_mult, '*', 0, 0, 0);
    test_binary('T', rtosc_arg_val_mult, '*', 0, 1, 0);
    test_mult('T', 1, 0, 0);
    test_mult('T', 1, 1, 1);
    test_mult('i', -6, -7, 42);
    test_mult('c', '\t', '\a', '?');
    test_mult('h', 3000000000, 2, 6000000000);
    test_mult('f', 0.01, 100, 1);
    test_mult('f', 1000000000000000000, -0.000000000000000001, -1);
    test_mult('d', 0.1111111, 0.1111111, 0.01234567654321);

    unary_arg_func_ptr op = rtosc_arg_val_negate;
    test_unary('T', op, '-', 1, 0);
    test_unary('T', op, '-', 0, 1);
    test_unary('F', op, '-', 1, 0);
    test_unary('F', op, '-', 0, 1);
    test_unary('i', op, '-', 123, -123);
    test_unary('h', op, '-', 8000000000, -8000000000);
    test_unary('f', op, '-', -0.1234, 0.1234);
    test_unary('f', op, '-', 0.0f, 0.0f);
    test_unary('d', op, '-', 0.1234123412341234, -0.1234123412341234);

    op = rtosc_arg_val_round;
    test_unary('T', op, '~', 1, 1);
    test_unary('T', op, '~', 0, 0);
    test_unary('F', op, '~', 1, 1);
    test_unary('F', op, '~', 0, 0);
    test_unary('i', op, '~', 123, 123);
    test_unary('c', op, '~', -123, -123);
    test_unary('h', op, '~', -8000000000, -8000000000);
    test_unary('f', op, '~', -0.1234, 0);
    test_unary('f', op, '~', 9.99f, 9);
    test_unary('f', op, '~', 9.9999f, 10);
    test_unary('d', op, '~', 1234.1234123412341234, 1234);
    test_unary('d', rtosc_arg_val_round, '~', 9999.9999, 10000);

    test_to_int('T', 0);
    test_to_int('T', 1);
    test_to_int('F', 0);
    test_to_int('F', 1);
    test_to_int('i', 42);
    test_to_int('c', ' ');
    test_to_int('h', -80);
    test_to_int('f', .123f);
    test_to_int('d', -.123f);

    test_null('i');
    test_null('c');
    test_null('h');
    test_null('f');
    test_null('d');

    special_null_test();

    // rtosc_arg_val_range_arg is being (indirectly)
    // tested in the pretty-format tests

    return test_summary();
}

