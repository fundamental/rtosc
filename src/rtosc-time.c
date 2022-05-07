#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rtosc/rtosc-time.h>

rtosc_arg_val_t *rtosc_arg_val_from_time_t(rtosc_arg_val_t *dest,
                                           time_t time, uint64_t secfracs)
{
    dest->val.t = secfracs;
    dest->val.t |= (((uint64_t)time) << ((uint64_t)32));
    dest->type = 't';

    return dest;
}

rtosc_arg_val_t *rtosc_arg_val_current_time(rtosc_arg_val_t *dest)
{
    time_t now;
    return rtosc_arg_val_from_time_t(dest, time(&now), 0);
}

rtosc_arg_val_t *rtosc_arg_val_from_params(rtosc_arg_val_t *dest,
                                           struct tm *m_tm, uint64_t secfracs)
{
    // adjust ranges to be POSIX conform
    m_tm->tm_year -= 1900;
    --m_tm->tm_mon;
    // don't mess around with Daylight Saving Time
    m_tm->tm_isdst = -1;

    uint64_t time = (uint64_t)mktime(m_tm);
    return rtosc_arg_val_from_time_t(dest, time, secfracs);
}

uint64_t rtosc_float2secfracs(float secfracsf)
{
    char secfracs_as_hex[16];
    // print float in hex representation (lossless)
    int written = snprintf(secfracs_as_hex, 16, "%a", secfracsf);
    // examples:
    // 0.85 => 0x1.b33334p-1
    // 0.51 => 0x1.051eb8p-1
    // 0.5  => 0x8p-4
    assert(written >= 0); // no error
    assert(written < 16); // size suffices
    (void) written;
    int scanpos;
    if(secfracs_as_hex[3]=='.') // 0x?.???
    {
        secfracs_as_hex[3] = secfracs_as_hex[2]; // remove '.'
        scanpos = 3;
    }
    else
    {
        scanpos = 2;
    }
    uint64_t secfracs;
    int exp;
    const char* plus;
    if((plus = strstr(secfracs_as_hex, "+0")) && !plus[2])
    {
        // exponent with plus sign, does not match the scanf below
        secfracs = exp = 0;
    }
    else
    {
        int scanned = sscanf(secfracs_as_hex + scanpos,
                             "%"PRIx64"p-%i" /* expands to like "%lxp-%i" */,
                             &secfracs, &exp);
        assert(scanned == 2);
    }
    const char* p = strchr(secfracs_as_hex, 'p');
    assert(p);
    int hexdigits_after_comma = p-(secfracs_as_hex+scanpos+1);

    /*
       Remember that
       (1) shifting a comma over one hex digits mulitplies/divides by 4
       (2) that secfracsf was scanned as "%a"
       (3) 0x00000001 counts as one secfrac and 0x100000000 would equal 1,
           which means one secfrac is 2^-32
       Then, we get:
                      base_without_comma*2^(-exp-4*hexdigits_after_comma)
       (with (1))   = base_with_comma*2^-exp
       (with (2))   = secfracsf
       (with (3))   = secfracs*2^-32
       <=> secfracs = base_without_comma * 2^(32-exp-4*hexdigits_after_comma)
    */
    int lshift = 32-exp-(hexdigits_after_comma<<2);
    assert(lshift > 0);
    secfracs <<= lshift;
    assert((secfracs & 0xFFFFFFFF) == secfracs);

    return secfracs;
}


rtosc_arg_val_t *rtosc_arg_val_immediatelly(rtosc_arg_val_t *arg)
{
    arg->type = 't';
    arg->val.t = 1;
    return arg;
}

time_t rtosct_time_t_from_arg_val(const rtosc_arg_val_t *arg)
{
    return (time_t)(arg->val.t >> 32);
}

struct tm* rtosct_params_from_arg_val(const rtosc_arg_val_t *arg)
{
    time_t t = rtosct_time_t_from_arg_val(arg);
    return localtime(&t);
}

uint64_t rtosct_secfracs_from_arg_val(const rtosc_arg_val_t *arg)
{
    return arg->val.t & (0xffffffff);
}

bool rtosc_arg_val_is_immediatelly(const rtosc_arg_val_t *arg)
{
    return arg->type == 't' && arg->val.t == 1;
}

float rtosc_secfracs2float(uint64_t secfracs)
{
    char as_hex_str[16];
    // print as hex string (lossless)
    int written = snprintf(as_hex_str, 16, "0x%xp-32", (unsigned)secfracs);
    assert(written >= 0); // no error
    assert(written < 16); // size suffices
    (void) written;

    float flt;
    int rd = 0;
    // scan hex string as float (can have loss)
    sscanf(as_hex_str, "%f%n", &flt, &rd);
    assert(rd);
    return flt;
}
