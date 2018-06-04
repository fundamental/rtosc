#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <rtosc/arg-val-cmp.h>
#include <rtosc/rtosc.h>

static const rtosc_cmp_options default_cmp_options = { 0.0 };

// abort only if one side was finished, or if both are infinite ranges
int rtosc_arg_vals_cmp_has_next(const rtosc_arg_val_itr* litr,
                                const rtosc_arg_val_itr* ritr,
                                size_t lsize, size_t rsize)
{
    return     (litr->i < lsize) && (ritr->i < rsize)
            && (litr->av->type != '-' || ritr->av->type != '-' ||
                litr->av->val.r.num || ritr->av->val.r.num);
}

// arrays are equal by now, but is one longer?
// each array must have either been completely passed, or it must
// have reached an infinite range
int rtosc_arg_vals_eq_after_abort(const rtosc_arg_val_itr* litr,
                                  const rtosc_arg_val_itr* ritr,
                                  size_t lsize, size_t rsize)
{
    return    (litr->i == lsize ||
                  (litr->av->type == '-' && !litr->av->val.r.num))
           && (ritr->i == rsize ||
                  (ritr->av->type == '-' && !ritr->av->val.r.num));
}

// compare single elements, ranges excluded
int rtosc_arg_vals_eq_single(const rtosc_arg_val_t* _lhs,
                             const rtosc_arg_val_t* _rhs,
                             const rtosc_cmp_options* opt)
{
#define mfabs(val) (((val) >= 0) ? (val) : -(val))
    int rval;

    if(!opt)
        opt = &default_cmp_options;

    if(_lhs->type == _rhs->type)
    switch(_lhs->type)
    {
        case 'i':
        case 'c':
        case 'r':
            rval = _lhs->val.i == _rhs->val.i;
            break;
        case 'I':
        case 'T':
        case 'F':
        case 'N':
            rval = 1;
            break;
        case 'f':
            rval = (opt->float_tolerance == 0.0)
                   ? _lhs->val.f == _rhs->val.f
                   : mfabs(_lhs->val.f - _rhs->val.f) <=
                   (float)opt->float_tolerance;
            break;
        case 'd':
            rval = (opt->float_tolerance == 0.0)
                   ? _lhs->val.d == _rhs->val.d
                   : mfabs(_lhs->val.d - _rhs->val.d) <=
                   opt->float_tolerance;
            break;
        case 'h':
            rval = _lhs->val.h == _rhs->val.h;
            break;
        case 't':
            rval = _lhs->val.t == _rhs->val.t;
            break;
        case 'm':
            rval = 0 == memcmp(_lhs->val.m, _rhs->val.m, 4);
            break;
        case 's':
        case 'S':
            rval = (_lhs->val.s == NULL || _rhs->val.s == NULL)
                 ? _lhs->val.s == _rhs->val.s
                 : (0 == strcmp(_lhs->val.s, _rhs->val.s));
            break;
        case 'b':
        {
            int32_t lbs = _lhs->val.b.len,
                    rbs = _rhs->val.b.len;
            rval = lbs == rbs;
            if(rval)
                rval = 0 == memcmp(_lhs->val.b.data, _rhs->val.b.data, lbs);
            break;
        }
        case 'a':
        {
            if(     _lhs->val.a.type != _rhs->val.a.type
               && !(_lhs->val.a.type == 'T' && _rhs->val.a.type == 'F')
               && !(_lhs->val.a.type == 'F' && _rhs->val.a.type == 'T'))
                rval = 0;
            else
                rval = rtosc_arg_vals_eq(_lhs+1, _rhs+1,
                                         _lhs->val.a.len, _rhs->val.a.len,
                                         opt);
            break;
        }
        default:
            rval = -1;
            // no recovery: the programmer did not pass the right args, and
            // we don't have a function to compare ranges here
            assert(false);
            exit(1);
            break;
    }
    else
    {
        rval = 0;
    }
    return rval;
#undef mfabs
}

int rtosc_arg_vals_eq(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                      size_t lsize, size_t rsize,
                      const rtosc_cmp_options* opt)
{
    // used if the value of lhs or rhs is range-computed:
    rtosc_arg_val_t rlhs, rrhs;

    rtosc_arg_val_itr litr, ritr;
    rtosc_arg_val_itr_init(&litr, lhs);
    rtosc_arg_val_itr_init(&ritr, rhs);

    int rval = 1;

    if(!opt)
        opt = &default_cmp_options;

    for( ; rtosc_arg_vals_cmp_has_next(&litr, &ritr, lsize, rsize) && rval;
        rtosc_arg_val_itr_next(&litr),
        rtosc_arg_val_itr_next(&ritr))
    {
        rval = rtosc_arg_vals_eq_single(rtosc_arg_val_itr_get(&litr, &rlhs),
                                        rtosc_arg_val_itr_get(&ritr, &rrhs),
                                        opt);
    }

    return rval
        ? rtosc_arg_vals_eq_after_abort(&litr, &ritr, lsize, rsize)
        : rval;
}

// compare single elements, ranges excluded
int rtosc_arg_vals_cmp_single(const rtosc_arg_val_t* _lhs,
                              const rtosc_arg_val_t* _rhs,
                              const rtosc_cmp_options* opt)
{
#define cmp_3way(val1,val2) (((val1) == (val2)) \
                            ? 0 \
                            : (((val1) > (val2)) ? 1 : -1))
#define mfabs(val) (((val) >= 0) ? (val) : -(val))

    int rval;

    if(!opt)
        opt = &default_cmp_options;

    if(_lhs->type == _rhs->type)
    switch(_lhs->type)
    {
        case 'i':
        case 'c':
        case 'r':
            rval = cmp_3way(_lhs->val.i, _rhs->val.i);
            break;
        case 'I':
        case 'T':
        case 'F':
        case 'N':
            rval = 0;
            break;
        case 'f':
            rval = (opt->float_tolerance == 0.0)
                   ? cmp_3way(_lhs->val.f, _rhs->val.f)
                   : (mfabs(_lhs->val.f - _rhs->val.f)
                      <= (float)opt->float_tolerance)
                      ? 0
                      : ((_lhs->val.f > _rhs->val.f) ? 1 : -1);
            break;
        case 'd':
            rval = (opt->float_tolerance == 0.0)
                   ? cmp_3way(_lhs->val.d, _rhs->val.d)
                   : (mfabs(_lhs->val.d - _rhs->val.d)
                      <= opt->float_tolerance)
                      ? 0
                      : ((_lhs->val.d > _rhs->val.d) ? 1 : -1);
            break;
        case 'h':
            rval = cmp_3way(_lhs->val.h, _rhs->val.h);
            break;
        case 't':
            // immediately is considered lower than everything else
            // this means if you send two events to a client,
            // one being "immediately" and one being different, the
            // immediately-event has the higher priority, event if the
            // other one is in the past
            rval = (_lhs->val.t == 1)
                   ? (_rhs->val.t == 1)
                     ? 0
                     : -1 // _lhs has higher priority => _lhs < _rhs
                   : (_rhs->val.t == 1)
                     ? 1
                     : cmp_3way(_lhs->val.t, _rhs->val.t);
            break;
        case 'm':
            rval = memcmp(_lhs->val.m, _rhs->val.m, 4);
            break;
        case 's':
        case 'S':
            rval = (_lhs->val.s == NULL || _rhs->val.s == NULL)
                 ? cmp_3way(_lhs->val.s, _rhs->val.s)
                 : strcmp(_lhs->val.s, _rhs->val.s);
            break;
        case 'b':
        {
            int32_t lbs = _lhs->val.b.len,
                    rbs = _rhs->val.b.len;
            int32_t minlen = (lbs < rbs) ? lbs : rbs;
            rval = memcmp(_lhs->val.b.data, _rhs->val.b.data, minlen);
            if(lbs != rbs && !rval)
            {
                // both equal until here
                // the string that ends here is lexicographically smaller
                rval = (lbs > rbs)
                       ? _lhs->val.b.data[minlen]
                       : -_rhs->val.b.data[minlen];
            }

            break;
        }
        case 'a':
        {
            int32_t llen = _lhs->val.a.len, rlen = _rhs->val.a.len;
            if(     _lhs->val.a.type != _rhs->val.a.type
               && !(_lhs->val.a.type == 'T' && _rhs->val.a.type == 'F')
               && !(_lhs->val.a.type == 'F' && _rhs->val.a.type == 'T'))
                rval = (_lhs->val.a.type > _rhs->val.a.type) ? 1 : -1;
            else
            {
                // the arg vals differ in this array => compare and return
                rval = rtosc_arg_vals_cmp(_lhs+1, _rhs+1, llen, rlen, opt);
            }
            break;
        }
        case '-':
            rval = -1;
            // no recovery: the programmer did not pass the right args, and
            // we don't have a function to compare ranges here
            assert(false);
            exit(1);
            break;
    }
    else
    {
        rval = (_lhs->type > _rhs->type) ? 1 : -1;
    }

    return rval;

#undef mfabs
#undef cmp_3way
}

int rtosc_arg_vals_cmp(const rtosc_arg_val_t* lhs, const rtosc_arg_val_t* rhs,
                       size_t lsize, size_t rsize,
                       const rtosc_cmp_options* opt)
{
    // used if the value of lhs or rhs is range-computed:
    rtosc_arg_val_t rlhs, rrhs;

    rtosc_arg_val_itr litr, ritr;
    rtosc_arg_val_itr_init(&litr, lhs);
    rtosc_arg_val_itr_init(&ritr, rhs);

    int rval = 0;

    if(!opt)
        opt = &default_cmp_options;

    for( ; rtosc_arg_vals_cmp_has_next(&litr, &ritr, lsize, rsize) && !rval;
        rtosc_arg_val_itr_next(&litr),
        rtosc_arg_val_itr_next(&ritr))
    {
        rval = rtosc_arg_vals_cmp_single(rtosc_arg_val_itr_get(&litr, &rlhs),
                                         rtosc_arg_val_itr_get(&ritr, &rrhs),
                                         opt);
    }

    return rval ? rval
                : (rtosc_arg_vals_eq_after_abort(&litr, &ritr, lsize, rsize))
                    ? 0
                    // they're equal until here, so one array must have
                    // elements left:
                    : (lsize-(litr.i) > rsize-(ritr.i))
                        ? 1
                        : -1;
}

