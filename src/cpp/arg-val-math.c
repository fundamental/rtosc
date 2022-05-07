#include <assert.h>
#include <rtosc/arg-val-math.h>
#include <rtosc/rtosc.h>

int rtosc_arg_val_null(rtosc_arg_val_t* av, char type)
{
    av->type = type;
    switch(type)
    {
        case 'h': av->val.h = 0; return true;
        case 't': av->val.t = 0; return true;
        case 's':
        case 'S': av->val.s = NULL; return true;
        case 'd': av->val.d = 0.0; return true;
        case 'f': av->val.f = 0.0f; return true;
        case 'c':
        case 'i':
        case 'r': av->val.i = 0; return true;
        case 'T':
        case 'F': av->type = 'F'; av->val.T = 0; return true;
        default: return false;
    }
}

int rtosc_arg_val_from_int(rtosc_arg_val_t* av, char type, int number)
{
    av->type = type;
    switch(type)
    {
        case 'h': av->val.h = number; return true;
        case 'd': av->val.d = number; return true;
        case 'f': av->val.f = number; return true;
        case 'c':
        case 'i': av->val.i = number; return true;
        case 'F':
        case 'T':
            // note: we discard av->type here!
            //       it's clear that the decision should be based on "number",
            //       not on the current type
            av->val.T = (number != 0.0);
            av->type = av->val.T ? 'T' : 'F';
            return true;
        default: return false;
    }
}

int rtosc_arg_val_from_double(rtosc_arg_val_t* av, char type, double number)
{
    av->type = type;
    switch(type)
    {
        case 'h': av->val.h = number; return true;
        case 'd': av->val.d = number; return true;
        case 'f': av->val.f = number; return true;
        case 'c':
        case 'i': av->val.i = number; return true;
        case 'F':
        case 'T':
            // note: we discard av->type here!
            //       it's clear that the decision should be based on "number",
            //       not on the current type
            av->val.T = (number != 0.0);
            av->type = av->val.T ? 'T' : 'F';
            return true;
        default: return false;
    }
}

int rtosc_arg_val_negate(rtosc_arg_val_t* av)
{
    switch(av->type)
    {
        case 'h': av->val.h = -av->val.h; return true;
        case 'd': av->val.d = -av->val.d; return true;
        case 'f': av->val.f = -av->val.f; return true;
        case 'c':
        case 'i': av->val.i = -av->val.i; return true;
        case 'T': av->val.T = 0; av->type = 'F'; return true;
        case 'F': av->val.T = 1; av->type = 'T'; return true;
        default: return false;
    }
}

int rtosc_arg_val_round(rtosc_arg_val_t* av)
{
    int tmp;
    switch(av->type)
    {
        case 'd':
            tmp = (int)(av->val.d);
            av->val.d = tmp + (int)(av->val.d - tmp >= 0.999);
            return true;
        case 'f':
            tmp = (int)(av->val.f);
            av->val.f = tmp + (int)(av->val.f - tmp >= 0.999f);
            return true;
        case 'h':
        case 'c':
        case 'i':
        case 'T':
        case 'F':
            return true;
        default:
            return false;
    }
}

int rtosc_arg_val_add(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                      rtosc_arg_val_t* res)
{
    if(lhs->type != rhs->type)
    {
        if((lhs->type == 'F' && rhs->type == 'T') ||
           (lhs->type == 'T' && rhs->type == 'F'))
        {
            res->type = 'T';
            res->val.T = 1;
            return true;
        }
        else return false;
    }
    else
    {
        res->type = lhs->type;
        switch(lhs->type)
        {
            case 'd': res->val.d = lhs->val.d + rhs->val.d; return true;
            case 'f': res->val.f = lhs->val.f + rhs->val.f; return true;
            case 'h': res->val.h = lhs->val.h + rhs->val.h; return true;
            case 'c':
            case 'i': res->val.i = lhs->val.i + rhs->val.i; return true;
            case 'T':
            case 'F': res->type = 'F'; res->val.T = 0; return true;
            default: return false;
        }
    }
}

int rtosc_arg_val_sub(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                      rtosc_arg_val_t* res)
{
    if(lhs->type != rhs->type)
        return rtosc_arg_val_add(lhs, rhs, res);
    res->type = lhs->type;
    switch(lhs->type)
    {
        case 'd': res->val.d = lhs->val.d - rhs->val.d; return true;
        case 'f': res->val.f = lhs->val.f - rhs->val.f; return true;
        case 'h': res->val.h = lhs->val.h - rhs->val.h; return true;
        case 'c':
        case 'i': res->val.i = lhs->val.i - rhs->val.i; return true;
        case 'T':
        case 'F': res->type = 'F'; res->val.T = 0; return true;
        default: return false;
    }
}

int rtosc_arg_val_mult(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                       rtosc_arg_val_t* res)
{
    if(lhs->type != rhs->type)
    {
        if((lhs->type == 'F' && rhs->type == 'T') ||
           (lhs->type == 'T' && rhs->type == 'F'))
        {
            res->type = 'F';
            res->val.T = 0;
            return true;
        }
        else return false;
    }
    else
    {
        res->type = lhs->type;
        switch(lhs->type)
        {
            case 'd': res->val.d = lhs->val.d * rhs->val.d; return true;
            case 'f': res->val.f = lhs->val.f * rhs->val.f; return true;
            case 'h': res->val.h = lhs->val.h * rhs->val.h; return true;
            case 'c':
            case 'i': res->val.i = lhs->val.i * rhs->val.i; return true;
            case 'T': res->type = 'T'; res->val.T = 1; return true;
            case 'F': res->type = 'F'; res->val.T = 0; return true;
            default: return false;
        }
    }
}

int rtosc_arg_val_div(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                      rtosc_arg_val_t* res)
{
    if(lhs->type != rhs->type)
        return false;
    res->type = lhs->type;
    switch(lhs->type)
    {
        case 'd': res->val.d = lhs->val.d / rhs->val.d; return true;
        case 'f': res->val.f = lhs->val.f / rhs->val.f; return true;
        case 'h': res->val.h = lhs->val.h / rhs->val.h; return true;
        case 'c':
        case 'i': res->val.i = lhs->val.i / rhs->val.i; return true;
        case 'T': res->type = 'T'; res->val.T = 1; return true;
        case 'F': assert(false); return false; // divide by 0
        default: return false;
    }
}

int rtosc_arg_val_to_int(const rtosc_arg_val_t* av, int* res)
{
    switch(av->type)
    {
        case 'd': *res = av->val.d; return true;
        case 'f': *res = av->val.f; return true;
        case 'h': *res = av->val.h; return true;
        case 'c':
        case 'i': *res = av->val.i; return true;
        case 'T':
        case 'F':
            *res = av->val.T; return true;
        default: return false;
    }
}

rtosc_arg_val_t* rtosc_arg_val_range_arg(const rtosc_arg_val_t *range_arg,
                                         int ith, rtosc_arg_val_t* result)
{
    rtosc_arg_val_t n_as_arg_val, mult;
    rtosc_arg_val_from_int(&n_as_arg_val, range_arg[1].type, ith);
    rtosc_arg_val_mult(&n_as_arg_val, range_arg+1, &mult);
    rtosc_arg_val_add(range_arg+2, &mult, result);
    return result;
}
