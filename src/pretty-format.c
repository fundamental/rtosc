#include "util.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <rtosc/rtosc.h>
#include <rtosc/pretty-format.h>
#include <rtosc/rtosc-time.h>
#include <rtosc/arg-val-math.h>
#include <rtosc/arg-val-cmp.h>

/**
    @file pretty-format.c
    @brief Implementation of pretty printer and scanner

    Most arguments are read into single arg_val_t elements
    exceptions:
     * arrays: "[arg1 arg2 ... argn]" <=> ('a', <type>, <size>) arg1 ... argn
     * ranges with delta: "arg1 ... argn"
        <=> ('-', <num>, <has_delta=1>) delta_arg arg1
     * ranges without delta: "nxarg"
        <=> ('-', <num>, <has_delta=0>) arg1
*/

static const rtosc_print_options* default_print_options
 = &((rtosc_print_options) { true, 2, " ", 80, true});

/** Call snprintf and assert() that it did fit into the buffer */
static int asnprintf(char* str, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int written = vsnprintf(str, size, format, args);
    assert(written >= 0); // no error
    // snprintf(3) says that a return value of size or more bytes means
    // that the output has been truncated
    assert((size_t)written < size);
    va_end(args);
    return written;
}

//! return the offset of the next arg from cur, arrays seen as one arg and
//! delta args (from ranges) seen as @p additional_for_delta args
static int next_arg_offset(const rtosc_arg_val_t* cur)
{
    return (cur->type == 'a' || cur->type == ' ')
           ? cur->val.a.len + 1
           : (cur->type == '-')
               ? 1                              /* range arg */
                 + next_arg_offset(cur + 1)     /* start arg */
                 + (int) cur->val.r.has_delta   /* delta arg */
               : 1;
}

/**
 * Return the char that represents the escape sequence
 *
 * @param c The escape sequence, e.g. '\n'
 * @param chr True if the character appears in a single character
 *    (vs in a string)
 * @return The character describing the escape sequence, e.g. 'n';
 *   -1 if none exists
 */
static int as_escaped_char(int c, int chr)
{
    switch(c)
    {
        case '\a': return 'a';
        case '\b': return 'b';
        case '\t': return 't';
        case '\n': return 'n';
        case '\v': return 'v';
        case '\f': return 'f';
        case '\r': return 'r';
        case '\\': return '\\';
        default:
            if(chr && c == '\'')
                return '\'';
            else if(!chr && c == '"')
                return '"';
            else return -1;
    }
}

// internal function for rtosc_print_arg_val
static void break_string(char** buffer, size_t bs, int wrt, int* cols_used)
{
    // unquoted, this means: "\<newline>    "
    int tmp = asnprintf(*buffer, bs, "\"\\\n    \"");
    wrt += tmp;
    *buffer += tmp;
    *cols_used = 5;
}

// helper function to break lines after an argument has been written
// which broke the line length
static void linebreak_check_after_write(int* cols_used, size_t* wrt,
                                        char* last_sep,
                                        char** buffer, size_t* bs,
                                        size_t inc, int* args_written_this_line,
                                        int linelength)
{

    ++*args_written_this_line;
    // did we break the line length,
    // and this was not the first arg written in this line?
    if(*cols_used > linelength && (*args_written_this_line > 1))
    {
        // insert "\n    "
        *last_sep = '\n';
        assert(*bs >= 4);
        memmove(last_sep+5, last_sep+1, 1+inc);
        last_sep[1] = last_sep[2] = last_sep[3] = last_sep[4] = ' ';
        *cols_used = 4 + inc;
        *wrt += 4;
        *buffer += 4;
        *bs -= 4;
        *args_written_this_line = 1;
    }
}

const size_t range_min = 5;

#define COUNT_UP(number) { int _n1 = number; bs -= _n1; buffer += _n1; \
                           wrt += _n1; }
#define COUNT_UP_COL(number) { int _n2 = number; \
                               COUNT_UP(_n2); *cols_used += _n2; }
#define COUNT_UP_WRITE(c) { assert(bs); \
                            *buffer++ = c; --bs; ++wrt; ++*cols_used; }

static int rtosc_print_range(const rtosc_arg_val_t* arg,
                             char* buffer, size_t bs,
                             const rtosc_print_options* opt, int* cols_used)
{
    size_t wrt = 0;
    int start;
    const rtosc_arg_t* val = &arg->val;

    // prepare for the loop
    if(opt->compress_ranges || !val->r.num)
    {
        if(val->r.has_delta || !val->r.num)
        {
            // delta or endless range (endless has no delta)

            const rtosc_arg_val_t* first_arg = arg+1+!!val->r.has_delta;
            int tmp = rtosc_print_arg_val(first_arg, buffer, bs,
                                          opt, cols_used);
            COUNT_UP(tmp);

            if(val->r.has_delta)
            {
                rtosc_arg_val_t one, m_one;
                rtosc_arg_val_from_int(  &one, first_arg->type,  1);
                rtosc_arg_val_from_int(&m_one, first_arg->type, -1);
                if(   !rtosc_arg_vals_eq_single(arg+1,   &one, NULL)
                   && !rtosc_arg_vals_eq_single(arg+1, &m_one, NULL))
                {
                    asnprintf(buffer, bs, " ");
                    COUNT_UP_COL(1);

                    rtosc_arg_val_t second;
                    rtosc_arg_val_range_arg(arg, 1, &second);
                    tmp = rtosc_print_arg_val(&second, buffer, bs,
                                              opt, cols_used);
                    COUNT_UP(tmp);
                }
            }

            asnprintf(buffer, bs, " ... ");
            COUNT_UP_COL(5);

            // print only the last value
            // (or nothing if the range is infinite)
            start = val->r.num - (!!val->r.num);
        }
        else
        {
            int tmp;
            // print "nx..."
            tmp = asnprintf(buffer, bs, "%dx", val->r.num);
            COUNT_UP_COL(tmp);
            tmp = rtosc_print_arg_val(arg+1, buffer, bs,
                                      opt, cols_used);
            COUNT_UP(tmp);

            // skip loop below
            start = val->r.num;
        }

    }
    else {
            start = 0; // print the whole range
    }

    // loop over all args of the range
    char* last_sep = buffer - 1;
    int args_written_this_line = (cols_used) ? 1 : 0;

    for(int i = start; i < val->r.num; ++i)
    {
        rtosc_arg_val_t tmparg;
        const rtosc_arg_val_t *cur;
        if(val->r.has_delta)
        {
            rtosc_arg_val_range_arg(arg, i, &tmparg);
            cur = &tmparg;
        }
        else
            cur = arg + 1;

        size_t tmp = rtosc_print_arg_val(cur, buffer, bs,
                                         opt, cols_used);
        COUNT_UP(tmp);

        linebreak_check_after_write(cols_used, &wrt,
                                    last_sep, &buffer, &bs,
                                    tmp, &args_written_this_line,
                                    opt->linelength);

        last_sep = buffer;
        COUNT_UP_WRITE(' ');
    }

    if(start < val->r.num) {
        // => remove last separator, that we have written too much
        buffer[-1] = 0;
        --wrt; // we have overridden space with '\0', and '\0' doesn't count
    }

    return wrt;
}

// only used in the function below (print_arg_val_range)
static size_t incsize(const rtosc_arg_val_t* av) {
    return av->type == 'a' ? av->val.a.len + 1 : 1;
}

//! insert a range block at the arg at position arg
//! arg and lhsarg may be the same pointer
static void insert_arg_range(rtosc_arg_val_t* arg, int32_t num,
                             const rtosc_arg_val_t* lhsarg,
                             int has_delta, const rtosc_arg_val_t* delta,
                             int lhs_overlap,
                             int delta_less_is_endless)
{
    rtosc_arg_val_t* first_arg = arg;
    if(has_delta)
        *++arg = *delta;
    if(lhs_overlap)
    {
        if(lhsarg->type == 'a')
        {
            // we inserted an 'a' element, so we need to
            // shift more than one element
            // the shifting amount is always one since array ranges
            // are delta-less
            // we have: '-'  a  a a a
            // =>       '-' 'a' a a a a
            for(int32_t i = lhsarg->val.a.len; i > 0; --i)
                arg[i+1] = arg[i];
        }
        *++arg = *lhsarg;
    }
    else
    {
        memcpy(++arg, lhsarg, incsize(lhsarg) * sizeof(rtosc_arg_val_t));
    }
    first_arg->type = '-';
    first_arg->val.r.num = (!has_delta && delta_less_is_endless) ? 0 : num;
    first_arg->val.r.has_delta = has_delta;
}

static const char* numeric_range_types() { return "cihfdTF"; }

static const char* numeric_range_convertible_types()
{
    // note: floats can not be converted to counting ranges safely
    //       (at least, not that I knew):
    // 0.00 0.33 0.67 0.10 => is it a counting range?
    return "cihTF";
}

//! tries to convert all args starting at @a arg into
//! an arg val range - if possible
//! @param arg_out array, output which must have the size of arg or more;
//!        may be the same as arg
//! @return The number of really skipped argument values
static int32_t rtosc_convert_to_range(const rtosc_arg_val_t* const arg,
                                      size_t size,
                                      rtosc_arg_val_t* arg_out,
                                      const rtosc_print_options* opt)
{
    if(size < range_min || arg[0].type == '-' || !opt->compress_ranges)
        return 0;
    char type = arg->type;
    size_t num_common = 0;
    for(size_t i = 0; i < size; i += incsize(arg+i), ++num_common)
    {
        if(type != arg[i].type)
            break;
    }
    if(num_common < range_min)
        return 0;

    int32_t skipped;
    num_common = 1;
    int has_delta;
    rtosc_arg_val_t delta, added;

    if(rtosc_arg_vals_eq_single(arg, arg + incsize(arg), NULL))
        has_delta = 0;
    else if(strchr(numeric_range_convertible_types(), arg->type)) {
        has_delta = 1;
        rtosc_arg_val_sub(arg+1, arg, &delta);
    }
    else return 0;

    {
        int go_on = 1;
        size_t next;
        for(skipped = incsize(arg); go_on; skipped = next, ++num_common)
        {
            next = skipped + incsize(arg+skipped);

            if(has_delta)
                rtosc_arg_val_add(arg+skipped, &delta, &added);

            if(next >= size || !rtosc_arg_vals_eq_single(has_delta ? &added
                                                                   : arg,
                                                         arg+next, NULL))
                go_on = false;
        }
    }

    if(num_common >= range_min)
    {
        insert_arg_range(arg_out, num_common, arg, has_delta, &delta,
                         false, false);
        const rtosc_arg_val_t* old_arg_out = arg_out;
        arg_out += 1 + has_delta;
        arg_out += incsize(arg);
        arg_out->type = ' ';
        arg_out->val.a.len = skipped - (arg_out - old_arg_out) - 1;
        return skipped;
    }
    else
        return 0;
}

size_t rtosc_print_arg_val(const rtosc_arg_val_t *arg,
                           char *buffer, size_t bs,
                           const rtosc_print_options* opt, int *cols_used)
{
    size_t wrt = 0;
    if(!opt)
        opt = default_print_options;
    assert(arg);
    const rtosc_arg_t* val = &arg->val;

    switch(arg->type)
    {
        case 'T':
            assert(bs>4);
            fast_strcpy(buffer, "true", bs);
            wrt = 4;
            break;
        case 'F':
            assert(bs>5);
            fast_strcpy(buffer, "false", bs);
            wrt = 5;
            break;
        case 'N':
            assert(bs>3);
            fast_strcpy(buffer, "nil", bs);
            wrt = 3;
            break;
        case 'I':
            assert(bs>3);
            fast_strcpy(buffer, "inf", bs);
            wrt = 3;
            break;
        case 'h':
            wrt = asnprintf(buffer, bs, "%"PRId64"h", val->h);
            break;
        case 't': // write to ISO 8601 date
        {
            if(rtosc_arg_val_is_immediatelly(arg))
                wrt = asnprintf(buffer, bs, "immediately");
            else
            {
                struct tm* m_tm = rtosct_params_from_arg_val(arg);
                int32_t secfracs = rtosct_secfracs_from_arg_val(arg);

                const char* strtimefmt = (secfracs || m_tm->tm_sec)
                                  ? "%Y-%m-%d %H:%M:%S"
                                  : (m_tm->tm_hour || m_tm->tm_min)
                                    ? "%Y-%m-%d %H:%M"
                                    : "%Y-%m-%d";

                wrt = strftime(buffer, bs, strtimefmt, m_tm);
                assert(wrt);

                if(secfracs)
                {
                    int prec = opt->floating_point_precision;
                    assert(prec>=0);
                    assert(prec<100);

                    // convert fractions -> float
                    float flt = rtosc_secfracs2float(secfracs);

                    // append float
                    char fmtstr[8];
                    asnprintf(fmtstr, 5, "%%.%df", prec);
                    int lastwrt = wrt;
                    wrt += asnprintf(buffer + wrt, bs - wrt,
                                     fmtstr, flt);
                    // snip part before separator
                    const char* sep = strchr(buffer + lastwrt, '.');
                    assert(sep);
                    memmove(buffer + lastwrt, sep, strlen(sep)+1);
                    wrt -= (sep - (buffer + lastwrt));

                    if(opt->lossless)
                        wrt += asnprintf(buffer + wrt, bs - wrt,
                                         " (...+%as)", flt);
                } // if secfracs
            } // else
            break;
        }
        case 'r':
            wrt = asnprintf(buffer, bs, "#%02x%02x%02x%02x",
                      (val->i >> 24) & 0xff,
                      (val->i >> 16) & 0xff,
                      (val->i >>  8) & 0xff,
                       val->i        & 0xff
                      );
            break;
        case 'd':
        case 'f':
        {
            int prec = opt->floating_point_precision;
            assert(prec>=0);
            assert(prec<100);
            char fmtstr[9];
            if(arg->type == 'f')
            {
                // e.g. "%.42f" or "%.0f.":
                asnprintf(fmtstr, 6, "%%#.%df", prec);
                wrt = asnprintf(buffer, bs, fmtstr, val->f);
                if(opt->lossless)
                    wrt += asnprintf(buffer + wrt, bs - wrt,
                                     " (%a)", val->f);
            }
            else
            {
                // e.g. "%.42lfd" or "%.0lf.d"
                asnprintf(fmtstr, 8, "%%#.%dlfd", prec);
                wrt = asnprintf(buffer, bs, fmtstr, val->d);
                if(opt->lossless)
                    wrt += asnprintf(buffer + wrt, bs - wrt,
                                     " (%la)", val->d);
            }
            break;
        }
        case 'c':
        {
            int is_esc = (as_escaped_char(val->i, true) != -1);
            wrt = asnprintf(buffer, bs, "'%s%c'",
                            is_esc ? "\\" : "",
                            is_esc ? as_escaped_char(val->i, true)
                                   : val->i);
            break;
        }
        case 'i':
            wrt = asnprintf(buffer, bs, "%"PRId32, val->i);
            break;
        case 'm':
            wrt = asnprintf(buffer, bs, "MIDI [0x%02x 0x%02x 0x%02x 0x%02x]",
                            val->m[0], val->m[1], val->m[2], val->m[3]);
            break;
        case 's':
        case 'S':
        {
            bool plain; // e.g. without quotes
            if(arg->type == 'S')
            {
                plain = true; // "Symbol": are quotes required?
                if(*val->s != '_' && !isalpha(*val->s))
                    plain = false;
                else for(const char* s = val->s + 1; *s && plain; ++s)
                    plain = (*s == '_' || (isalnum(*s)));
            }
            else plain = false;

            char* b = buffer;
            if(!plain)
            {
                *b++ = '"';
                ++*cols_used;
            }
            for(const char* s = val->s; *s; ++s)
            {
                // "3": 2 quote signs and escaping backslash
                if(!plain && *cols_used > opt->linelength - 3)
                    break_string(&b, bs, wrt, cols_used);
                assert(bs);
                int as_esc = as_escaped_char(*s, false);
                if(as_esc != -1) {
                    assert(bs-1);
                    *b++ = '\\';
                    *b++ = as_esc;
                    *cols_used += 2;
                    if(!plain && as_esc == 'n')
                        break_string(&b, bs, wrt, cols_used);
                }
                else {
                    *b++ = *s;
                    ++*cols_used;
                }
            }

            if(plain)
                assert(bs);
            else
            {
                assert(bs >= 2);
                *b++ = '"';
                ++*cols_used;
                if(arg->type == 'S') {
                    assert(bs >= 2);
                    *b++ = 'S';
                }
            }
            *b = 0;
            wrt += (b-buffer);
            break;
        }
        case 'b':
            wrt = asnprintf(buffer, bs, "BLOB [%d ", val->b.len);
            *cols_used += wrt;
            buffer += wrt;
            for(int32_t i = 0; i < val->b.len; ++i)
            {
                if(*cols_used >= opt->linelength - 6)
                {
                    int tmp = asnprintf(buffer-1, bs+1, "\n    ");
                    wrt += (tmp-1);
                    buffer += (tmp-1);
                    *cols_used = 4;
                }
                asnprintf(buffer, bs, "0x%02x ", val->b.data[i]);
                COUNT_UP_COL(5);
            }
            buffer[-1] = ']';
            break;
        case 'a':
        {
            char* last_sep = buffer - 1;
            int args_written_this_line = (cols_used) ? 1 : 0;
            rtosc_arg_val_t args_converted[val->a.len]; // range conversion

            COUNT_UP_WRITE('[');
            if(val->a.len)
            for(int32_t i = 1; i <= val->a.len; )
            {
                int32_t conv = rtosc_convert_to_range(arg+i, val->a.len+1-i, args_converted, opt);
                const rtosc_arg_val_t* input = conv ? args_converted : arg+i;

                size_t tmp = rtosc_print_arg_val(input, buffer, bs,
                                                 opt, cols_used);
                i += conv ? conv : next_arg_offset(arg+i);
                COUNT_UP(tmp);

                linebreak_check_after_write(cols_used, &wrt,
                                            last_sep, &buffer, &bs,
                                            tmp, &args_written_this_line,
                                            opt->linelength);

                last_sep = buffer;
                COUNT_UP_WRITE(' ');
            }
            else
                COUNT_UP_WRITE(' ');

            assert(bs);
            buffer[-1] = ']';
            *buffer = 0;
            ++*cols_used;
            --bs;
            break;
        }
        case '-':
        {
            wrt += rtosc_print_range(arg, buffer, bs, opt, cols_used);
            break;
        }
        default:
            ;
    }

    switch(arg->type)
    {
        case '-':
        case 'a':
        case 's':
        case 'S':
        case 'b':
            // these can break the line, so they compute *cols_used themselves
            break;
        default:
            *cols_used += wrt;
    }

    return wrt;
}
#undef COUNT_UP_COL
#undef COUNT_UP
#undef COUNT_UP_WRITE

size_t rtosc_print_arg_vals(const rtosc_arg_val_t *args, size_t n,
                            char *buffer, size_t bs,
                            const rtosc_print_options *opt, int cols_used)
{
    size_t wrt=0;
    int args_written_this_line = (cols_used) ? 1 : 0;
    if(!opt)
        opt = default_print_options;
    size_t sep_len = strlen(opt->sep);
    char* last_sep = buffer - 1;
    rtosc_arg_val_t args_converted[n]; // only used for range conversion

    for(size_t i = 0; i < n;)
    {
        int32_t conv = rtosc_convert_to_range(args, n-i, args_converted, opt);
        const rtosc_arg_val_t* input = conv ? args_converted : args;

        size_t tmp = rtosc_print_arg_val(input, buffer, bs, opt, &cols_used);
        wrt += tmp;
        buffer += tmp;
        bs -= tmp;

        // these compute the newlines themselves
        if(! strchr("-asb", args->type))
        linebreak_check_after_write(&cols_used, &wrt, last_sep, &buffer, &bs,
                                    tmp, &args_written_this_line,
                                    opt->linelength);

        size_t inc = conv ? conv : next_arg_offset(args);
        i += inc;
        args += inc;
        if(i<n)
        {
            assert(sep_len < bs);
            last_sep = buffer;
            fast_strcpy(buffer, opt->sep, bs);
            cols_used += sep_len;
            wrt += sep_len;
            buffer += sep_len;
            bs -= sep_len;
        }
    }
    return wrt;
}

size_t rtosc_print_message(const char* address,
                           const rtosc_arg_val_t *args, size_t n,
                           char *buffer, size_t bs,
                           const rtosc_print_options* opt,
                           int cols_used)
{
    size_t wrt = asnprintf(buffer, bs, "%s ", address);
    cols_used += wrt;
    buffer += wrt;
    bs -= wrt;
    wrt += rtosc_print_arg_vals(args, n, buffer, bs, opt, cols_used);
    return wrt;
}

/**
 * Increase @p s while property @property is true and the end has not yet
 * been reached
 */
static void skip_while(const char** s, int (*property)(int))
{
    for(;**s && (*property)(**s);++*s);
}

/**
 * Parse the string pointed to by @p src conforming to the format string @p
 * @param src Pointer to the input string
 * @param fmt Format string for sscanf(). Must suppress all assignments,
 *   except the last one, which must be "%n" and be at the string's end.
 * @return The number of bytes skipped from the string pointed to by @p src
 */
static int skip_fmt(const char** src, const char* fmt)
{
    assert(!strncmp(fmt + strlen(fmt) - 2, "%n", 2));
    int rd = 0;
    sscanf(*src, fmt, &rd);
    *src += rd;
    return rd;
}

/**
 * Behave like skip_fmt() , but set *src to NULL if the string didn't match
 * @see skip_fmt
 */
static int skip_fmt_null(const char** src, const char* fmt)
{
    int result = skip_fmt(src, fmt);
    if(!result)
        *src = NULL;
    return result;
}

/** Helper function for scanf_fmtstr() */
static const char* try_fmt(const char* src, int exp, const char* fmt,
                           char* typesrc, char type)
{
    int rd = 0;
    sscanf(src, fmt, &rd);
    if(rd == exp)
    {
        *typesrc = type;
        return fmt;
    }
    else
        return NULL;
}

/**
 * Return the right format string for skipping the next numeric value
 *
 * This can be used to find out how to sscanf() ints, floats and doubles.
 * If the string needs to be read, too, see scanf_fmtstr_scan() below.
 * @param src The beginning of the numeric value to scan
 * @param type If non-NULL, the corresponding rtosc argument character
 *   (h,i,f,d) will be written here
 * @return The format string to use or NULL
 */
static const char* scanf_fmtstr(const char* src, char* type)
{
    const char* end = src;
    // skip to string end, word end or a closing paranthesis
    for(; *end && !isspace(*end) && (*end != ')') && (*end != ']')
               && strncmp(end, "...", 3); ++end);

    int exp = end - src;

    // store type byte in the parameter, or in a temporary variable?
    char tmp;
    char* _type = type ? type : &tmp;

    const char i32[] = "%*"PRIi32"%n";

    const char* r; // result
    int ok = (r = try_fmt(src, exp, "%*"PRIi64"h%n", _type, 'h'))
          || (r = try_fmt(src, exp, "%*d%n", _type, 'i'))
          || (r = try_fmt(src, exp, "%*"PRIi32"i%n", _type, 'i'))
          || (r = try_fmt(src, exp, i32, _type, 'i'))
          || (r = try_fmt(src, exp, "%*lfd%n", _type, 'd'))
          || (r = try_fmt(src, exp, "%*ff%n", _type, 'f'))
          || (r = try_fmt(src, exp, "%*f%n", _type, 'f'));
    (void)ok;
    if(r == i32)
        r = "%*x%n";
    return r;
}

/** Return the right format string for reading the next numeric value */
static const char* scanf_fmtstr_scan(const char* src, char* bytes16, char* type)
{
    const char *buf = scanf_fmtstr(src, type);
    assert(buf);
    assert(bytes16);
    fast_strcpy(bytes16, buf, 16);
    *++bytes16 = '%'; // transform "%*" to "%"
    return bytes16;
}

/** Skip the next numeric at @p src */
static size_t skip_numeric(const char** src, char* type)
{
    const char* scan_str = scanf_fmtstr(*src, type);
    if(!scan_str) return 0;
    else return skip_fmt(src, scan_str);
}

/**
 * Return the escape sequence that's generated with the given character
 * @param c The character, e.g. 'n'
 * @param chr True if the character appears in a single character
 *   (vs in a string)
 * @return The escape sequence generated by the character, e.g. '\n';
 *   0 if none exists
 */
static char get_escaped_char(char c, int chr)
{
    switch(c)
    {
        case 'a': return '\a';
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case '\\': return '\\';
        default:
            if(chr && c == '\'')
                return '\'';
            else if(!chr && c == '"')
                return '"';
            else
                return 0;
    }
}

/** Called inside a string at @p src, skips until after the closing quote */
static const char* end_of_printed_string(const char* src)
{
    bool escaped = false;
    ++src;
    bool cont;
    do
    {
        for(; *src && (escaped || *src != '"') ; ++src)
        {
            if(escaped) // last char introduced an escape sequence?
            {
                if(!get_escaped_char(*src, false))
                    return NULL; // bad escape sequence
            }
            escaped = (*src == '\\') ? (!escaped) : false;
        }
        if(*src == '"' && src[1] == '\\') {
            skip_fmt_null(&src, "\"\\ \"%n");
            cont = true;
        }
        else
            cont = false;
    } while(cont);
    if(!src || !*src)
        return NULL;
    return ++src;
}

/**
 * Skips string @p exp at the current string pointed to by @p str,
 * but only if it's a separated word, i.e. if there's a char after the string,
 * it must be a slash or whitespace.
 *
 * @return The position after the word, or NULL if the word was not present
 *   at @p str .
 */
static const char* skip_word(const char* exp, const char** str)
{
    size_t explen = strlen(exp);
    const char* cur = *str;
    int match = (!strncmp(exp, cur, explen) &&
                 (   !cur[explen]
                  || cur[explen] == '/' || cur[explen] == ']'
                  || cur[explen] == '.'
                  || isspace(cur[explen])));
    if(match) {
        *str += explen;
        return *str;
    }
    else return NULL;
}

/**
 * Tries to skip the next identifier beginning at @p str
 *
 * @return The position after the identifier, or NULL if there's no identifier
 *   at @p str .
 */
static const char* skip_identifier(const char* str)
{
    if(!isalpha(*str) && *str != '_')
        return NULL;
    else
    {
        ++str;
        for(; isalnum(*str) || *str == '_'; ++str) ;
        return str;
    }
}

int32_t delta_from_arg_vals(const rtosc_arg_val_t* llhsarg,
                            const rtosc_arg_val_t* lhsarg,
                            const rtosc_arg_val_t* rhsarg,
                            rtosc_arg_val_t* delta,
                            int must_be_unity)
{
    /*
        compute delta
    */
    int cmp;
    if(must_be_unity)
    {
        cmp = rtosc_arg_vals_cmp(lhsarg, rhsarg, 1, 1, NULL);
        rtosc_arg_val_from_int(delta, rhsarg->type, 1);
        if(cmp > 0)
            rtosc_arg_val_negate(delta);
    }
    else
    {
        rtosc_arg_val_sub(lhsarg, llhsarg, delta);
        rtosc_arg_val_t null_val;
        rtosc_arg_val_null(&null_val, delta->type);
        cmp = rtosc_arg_vals_cmp(delta, &null_val, 1, 1, NULL);
    }

    if(!cmp)
        return -1;

    /*
     * check if delta matches,
     * i.e. there's an "n" s. t. lhsarg + n*delta = rhsarg
     */
    int res;
    if(rhsarg)
    {
        rtosc_arg_val_t width, div, width2;
        rtosc_arg_val_sub(rhsarg, lhsarg, &width);
        rtosc_arg_val_div(&width, delta, &div);
        rtosc_arg_val_round(&div);
        rtosc_arg_val_mult(&div, delta, &width2);

        const rtosc_cmp_options cmp_options
         = ((rtosc_cmp_options) { 0.001 });
        if( ! rtosc_arg_vals_eq(&width, &width2, 1, 1, &cmp_options))
            return -1;

        rtosc_arg_val_to_int(&div, &res);
    }
    else
        res = -1; // return 0 to say "infinite range"
    return res + 1;
}

static int is_range_multiplier(const char* s)
{
    int rval;
    if(isdigit(*s) && *s != '0')
    {
        while(isdigit(*++s)) ;
        rval = (*s == 'x');
    }
    else
        rval = 0;
    return rval;
}

int types_match(char type1, char type2)
{
    return (type1 == type2) ||
           (type1 == 'T' && type2 == 'F') ||
           (type1 == 'F' && type2 == 'T');
}

int arraytypes_match(char type1, char type2)
{
    return type1 == '-' || type2 == '-' || // ranges match everything
                                           // (TODO: actually a bug)
           types_match(type1, type2);
}

const char* rtosc_skip_next_printed_arg(const char* src, int* skipped,
                                        char* type, const char* llhssrc,
                                        int follow_ellipsis, int inside_bundle)
{
    char dummy;
    if(!type)
        type = &dummy;
    assert(skipped);
    assert(src);
    *skipped = 1; // in almost all cases

    char deltaless_range_type = 0;

    const char* old_src = src;
    switch(*src)
    {
        case 't':
            if(skip_word("true", &src))
                *type = 'T';
            else {
                src = skip_identifier(src);
                *type = 'S';
            }
            break;
        case 'f':
            if(skip_word("false", &src))
                *type = 'F';
            else {
                src = skip_identifier(src);
                *type = 'S';
            }
            break;
        case 'n':
            if(skip_word("nil", &src))
                *type = 'N';
            else if(skip_word("now", &src))
                *type = 't';
            else {
                src = skip_identifier(src);
                *type = 'S';
            }
            break;
        case 'i':
            if(skip_word("inf", &src))
                *type = 'I';
            else if(skip_word("immediately", &src))
                *type = 't';
            else {
                src = skip_identifier(src);
                *type = 'S';
            }
            break;
        case '#':
            for(size_t i = 0; i<8; ++i)
            {
                ++src;
                if(!isxdigit(*src)) {
                    src = NULL;
                    i = 8;
                }
            }
            if(src) ++src;
            *type = 'r';
            break;
        case '\'':
        {
            int esc = -1;
            if(strlen(src) < 3)
                return NULL;
            // type 1: '<noslash>' => normal char
            // type 2: '\<noquote>' => escaped char
            // type 3: '\'' => escaped quote
            // type 4: '\' => mistyped backslash
            if(src[1] == '\\') {
                if(src[2] == '\'' && (!src[3] || isspace(src[3])))
                {
                    // type 4
                    // the user inputs '\', which is wrong, but
                    // we accept it as a backslash anyways
                }
                else
                {
                    ++src; // type 2 or 3
                    esc = get_escaped_char(src[1], 1);
                }
            }
            // if the last char was no single quote,
            // or we had an invalid escape sequence, return NULL
            src = (!esc || src[2] != '\'') ? NULL : (src + 3);
            *type = 'c';
            break;
        }
        case '"':
            src = end_of_printed_string(src);
            if(src && *src == 'S') {
                ++src;
                *type = 'S';
            }
            else
                *type = 's';
            break;
        case 'M':
            if(!strncmp("MIDI", src, 4) && (isspace(src[4]) || src[4] == '['))
            {
                skip_fmt_null(&src, "MIDI [ 0x%*x 0x%*x 0x%*x 0x%*x ]%n");
                *type = 'm';
            }
            else {
                src = skip_identifier(src);
                *type = 'S';
            }
            break;
        case '[':
        {
            while(isspace(*++src));
            char arraytype = 0;
            const char* recent_src = NULL;
            while(src && *src && *src != ']')
            {
                char arraytype_cur = 20;
                int skipped2;
                const char* newsrc = rtosc_skip_next_printed_arg(src, &skipped2,
                                                                 &arraytype_cur,
                                                                 recent_src, 1,
                                                                 true);
                recent_src = src;
                src = newsrc;

                if(src)
                    for( ; isspace(*src); ++src) ;
                if(!arraytype)
                    arraytype = arraytype_cur;
                else if(!arraytypes_match(arraytype, arraytype_cur))
                    src = NULL;
                *skipped += skipped2;
            }
            if(src)
            {
                if(*src) // => ']'
                    ++src;
                else
                    src = NULL;
            }
            *type = 'a';
            break;
        }
        case 'B':
        {
            const char* oldsrc = src;
            skip_fmt_null(&src, "BLOB [ %n");
            if(src)
            {
                *type = 'b';
                int rd = 0, blobsize = 0;
                sscanf(src, "%i %n", &blobsize, &rd);
                src = rd ? (src + rd) : NULL;
                for(;src && *src == '0';) // i.e. 0x...
                {
                    skip_fmt_null(&src, "0x%*x %n");
                    blobsize--;
                }
                if(blobsize)
                    src = NULL;
                if(src)
                    src = (*src == ']') ? (src + 1) : NULL;
            }
            else
            {
                *type = 'S';
                src = skip_identifier(oldsrc);
            }
            break;
        }
        default:
        {
            // nx... ? (n > 0)
            if(is_range_multiplier(src))
            {
                while(*++src != 'x') ;
                ++src;

                int skipped2;
                src = rtosc_skip_next_printed_arg(src, &skipped2,
                                                  &deltaless_range_type,
                                                  NULL, 0, inside_bundle);
                if(src) {
                    *type = '-';
                    *skipped += skipped2;
                }
            }
            // is it an identifier?
            else if(*src == '_' || isalpha(*src))
            {
                for(; *src == '_' || isalnum(*src); ++src) ;
                *type = 'S';
            }
            // is it a date? (vs a numeric)
            else if(skip_fmt(&src, "%*4d-%*1d%*1d-%*1d%*1d%n"))
            {
                if(skip_fmt(&src, " %*2d:%*1d%*1d%n"))
                if(skip_fmt(&src, ":%*1d%*1d%n"))
                if(skip_fmt(&src, ".%*d%n"))
                {
                    if(skip_fmt(&src, " ( ... + 0x%n"))
                    {
                        skip_fmt(&src, "%*x.%n");
                        if(skip_fmt(&src, "%*xp%n"))
                        {
                            int rd = 0, expm;
                            sscanf(src, "-%d s )%n", &expm, &rd);
                            if(rd && expm > 0 && expm <= 32)
                            {
                                // ok
                                src += rd;
                            }
                            else
                                src = NULL;

                        }
                        else
                            src = NULL;
                    }
                }
                *type = 't';
            }
            else
            {
                int rd = skip_numeric(&src, type);
                if(!rd) { src = NULL; break; }
                const char* after_num = src;
                skip_while(&after_num, isspace);
                if(*after_num == '(')
                {
                    if (*type == 'f' || *type == 'd')
                    {
                        // skip lossless representation
                        src = ++after_num;
                        skip_while(&src, isspace);
                        rd = skip_numeric(&src, NULL);
                        if(!rd) { src = NULL; break; }
                        skip_fmt_null(&src, " )%n");
                    }
                    else
                        src = NULL;
                }
            }
        }
    }

    if(follow_ellipsis && src)
    {
        // is the argument being followed by an ellipsis?
        const char* src2 = src;
        for(; isspace(*src2); ++src2) ;

        if(!strncmp(src2, "...", 3))
        {
            src = src2;

            // What we need to do is only checking for correctness:
            //  * lhs (left hand sign) and rhs are given
            //  * lhs and rhs must be of the same type
            //  * there must be an "n" such that lhs + n*delta = rhs
            //    => therefore, compute delta
            //  * such "n" must not be 0

            const char* rhssrc = src;
            const char* ellipsis = src;
            src = NULL;
            const char* lhssrc;

            do
            {
                lhssrc = old_src;
                if(is_range_multiplier(lhssrc))
                    lhssrc = strchr(lhssrc, 'x') + 1;

                rtosc_arg_val_t llhsarg, lhsarg, rhsarg;
                char lhstype = deltaless_range_type ? deltaless_range_type
                                                    : *type,
                     llhstype, rhstype[2] = "x";

                *type = '-'; // TODO: bug? return scanned type instead,
                             //       to avoid [0.1 1 ...5]

                size_t zero = 0;
                int llhsskipped, rhsskipped;

                rtosc_arg_val_t delta;

                /*
                 * in all cases, check rhs
                 */
                rhssrc += 2;
                while(isspace(*++rhssrc)) ;

                bool numeric_range  = strchr(numeric_range_types(), lhstype),
                     infinite_range = false;
                const char* endsrc;
                if(*rhssrc == ']')
                {
                    assert(inside_bundle);
                    // we're still not finished. find out if delta-less or not
                    *rhstype = lhstype;
                    endsrc = rhssrc;
                    infinite_range = true;
                }
                else
                {
                    // non-infinite ranges are numeric, so check the type
                    if(!numeric_range)
                        break;

                    endsrc = rtosc_skip_next_printed_arg(rhssrc, &rhsskipped,
                                                         rhstype, NULL, 0,
                                                         inside_bundle);
                    if(!endsrc)
                        break;

                    rtosc_scan_arg_val(rhssrc, &rhsarg, 1, NULL, &zero, 0, 0);
                }

                /*
                 * do not check lhs; it has been checked above
                 */
                if(lhstype != *rhstype)
                {
                    // types must be equal
                    break;
                }

                if(numeric_range)
                    rtosc_scan_arg_val(lhssrc, &lhsarg, 1,
                                       NULL, &zero, 0, 0);

                /*
                 * is llhs given and useful?
                 */
                bool llhsarg_is_useless = false;
                if(llhssrc)
                {
                    const char* next_ellipsis_from_llhssrc =
                            strstr(llhssrc, "...");
                    if(next_ellipsis_from_llhssrc < ellipsis)
                    {
                        llhssrc = next_ellipsis_from_llhssrc + 2;
                        while(isspace(*++llhssrc)) ;
                    }
                    else if(is_range_multiplier(llhssrc))
                    {
                        llhssrc = strchr(llhssrc, 'x') + 1;
                    }

                    rtosc_skip_next_printed_arg(llhssrc,
                                                &llhsskipped, &llhstype,
                                                NULL, 0, inside_bundle);
                    if(types_match(llhstype, lhstype))
                    {
                        rtosc_scan_arg_val(llhssrc, &llhsarg, 1,
                                           NULL, &zero, 0, 0);
                        // hint: use rtosc_arg_val_range_arg here if
                        // overlapping ranges (1 ... 5 ... 9) shall be
                        // implemented
                    }
                    else
                    {
                        llhsarg_is_useless = true;
                    }
                }
                else {
                    llhsarg_is_useless = true;
                }

                bool has_delta = true;
                /*
                    compute delta and check if it matches
                */
                if(infinite_range && (llhsarg_is_useless || !numeric_range))
                {
                    has_delta = false;
                }
                else
                {
                    int32_t num = delta_from_arg_vals(&llhsarg, &lhsarg,
                                                      infinite_range ? NULL
                                                                     : &rhsarg,
                                                      &delta,
                                                      llhsarg_is_useless);

                    if(num == -1)
                    {
                        if(infinite_range)
                        {
                            has_delta = false;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // if we made it until here, set src to non-NULL
                src = endsrc;

                // ellipsis skips 3 arg vals: range, delta, start
                if(has_delta)
                    ++*skipped;
                ++*skipped;
            } while(false);
        }
    }

    return src;
}

int rtosc_count_printed_arg_vals(const char* src)
{
    int num = 0;

    skip_while(&src, isspace);
    while (*src == '%')
        skip_fmt(&src, "%*[^\n] %n");

    int skipped_now = 0;
    const char* recent_src = NULL;
    for(; src && *src && *src != '/'; num += skipped_now)
    {
        const char* newsrc = rtosc_skip_next_printed_arg(src, &skipped_now,
                                                         NULL, recent_src, 1,
                                                         0);
        recent_src = src;
        src = newsrc;

        if(src) // no parse error
        {
            skip_while(&src, isspace);
            if(*src && !isspace(*src))
            {
                while (*src == '%')
                    skip_fmt(&src, "%*[^\n] %n");
            }
        }
    }
    return src ? num : -num;
}

int rtosc_count_printed_arg_vals_of_msg(const char* msg)
{
    skip_while(&msg, isspace);
    while (*msg == '%')
        skip_fmt(&msg, "%*[^\n] %n");

    if (*msg == '/') {
        for(; *msg && !isspace(*msg); ++msg);
        return rtosc_count_printed_arg_vals(msg);
    }
    else if(!*msg)
        return INT_MIN;
    else
        return -1;
}

//! Tries to parse an identifier at @p src and stores it in @p arg
const char* parse_identifier(const char* src, rtosc_arg_val_t *arg,
                             char* buffer_for_strings,
                             size_t* bufsize)
{
    if(*src == '_' || isalpha(*src))
    {
        arg->type = 'S';
        arg->val.s = buffer_for_strings;
        for(; *src == '_' || isalnum(*src); ++src)
        {
            assert(*bufsize);
            (*bufsize)--;
            *buffer_for_strings = *src;
            ++buffer_for_strings;
        }
        assert(*bufsize);
        (*bufsize)--;
        *buffer_for_strings = 0;
        ++buffer_for_strings;
    }
    return src;
}

size_t rtosc_scan_arg_val(const char* src,
                          rtosc_arg_val_t *arg, size_t nargs,
                          char* buffer_for_strings, size_t* bufsize,
                          size_t args_before, int follow_ellipsis)
{
    int rd = 0;
    const char* start = src;
    assert(nargs);
    --nargs;
    switch(*src)
    {
        case 't':
        case 'f':
        case 'n':
        case 'i':
        {
            const char* src_backup = src;
            // timestamps "immediately" or "now"?
            if(skip_word("immediately", &src) || skip_word("now", &src))
            {
                rtosc_arg_val_immediatelly(arg);
            }
            else if(skip_word("true", &src)  ||
                    skip_word("false", &src) )
            {
                arg->type = toupper(*src_backup);
                arg->val.T = (src[-2] == 'u');
            }
            else if(skip_word("nil", &src)   ||
                    skip_word("inf", &src))
            {
                arg->type = toupper(*src_backup);
            }
            else
            {
                // no reserved keyword => identifier
                src = parse_identifier(src, arg, buffer_for_strings, bufsize);
            }
            break;
        }
        case '#':
        {
            arg->type = 'r';
            sscanf(++src, "%x", &arg->val.i);
            src+=8;
            break;
        }
        case '\'':
            // type 1: '<noslash>' => normal char
            // type 2: '\<noquote>' => escaped char
            // type 3: '\'' => escaped quote
            // type 4: '\'<isspace> => mistyped backslash
            arg->type = 'c';
            if(*++src == '\\')
            {
                if(src[2] && !isspace(src[2])) // escaped and 4 chars
                    arg->val.i = get_escaped_char(*++src, true);
                else // escaped, but only 3 chars: type 4
                    arg->val.i = '\\';
            }
            else // non-escaped
                arg->val.i = *src;
            src+=2;
            break;
        case '"':
        {
            ++src; // skip obligatory '"'
            char* dest = buffer_for_strings;
            bool cont;
            do
            {
                while(*src != '"')
                {
                    (*bufsize)--;
                    assert(*bufsize);
                    if(*src == '\\') {
                        *dest++ = get_escaped_char(*++src, false);
                        ++src;
                    }
                    else
                        *dest++ = *src++;
                }
                if(src[1] == '\\')
                {
                    skip_fmt(&src, "\"\\ \"%n");
                    cont = true;
                }
                else
                    cont = false;
            } while (cont);
            *dest = 0;
            ++src; // skip final '"'
            (*bufsize)--;
            arg->val.s = buffer_for_strings;
            if(*src == 'S')
            {
                ++src;
                (*bufsize)--;
                arg->type = 'S';
            }
            else
                arg->type = 's';
            break;
        }
        case 'M':
        {
            if(!strncmp("MIDI", src, 4) && (isspace(src[4]) || src[4] == '['))
            {
                arg->type = 'm';
                int32_t tmp[4];
                sscanf(src, "MIDI [ 0x%"PRIx32" 0x%"PRIx32
                                  " 0x%"PRIx32" 0x%"PRIx32" ]%n",
                       tmp, tmp + 1, tmp + 2, tmp + 3, &rd); src+=rd;
                for(size_t i = 0; i < 4; ++i)
                    arg->val.m[i] = tmp[i]; // copy to 8 bit array
            }
            else
                src = parse_identifier(src, arg, buffer_for_strings, bufsize);
            break;
        }
        case '[':
        {
            while(isspace(*++src));
            int32_t num_read = 0;

            rtosc_arg_val_t* start_arg = arg++;

            size_t last_bufsize;

            char arrtype = ' ';
            for(size_t i = 0; src && *src && *src != ']'; ++i)
            {
                last_bufsize = *bufsize;

                src += rtosc_scan_arg_val(src, arg, nargs,
                                          buffer_for_strings, bufsize, i, 1);
                arrtype = arg->type;
                if(arrtype == '-')
                    arrtype = arg->val.r.has_delta ? arg[2].type : arg[1].type;

                size_t args_scanned = next_arg_offset(arg);
                nargs -= args_scanned;
                arg += args_scanned;
                num_read += args_scanned;

                for( ; isspace(*src); ++src) ;

                buffer_for_strings += (last_bufsize - *bufsize);
                // TODO: allow comments in arrays, blobs, midi?
            }

            ++src; // ']'
            start_arg->type = 'a';
            start_arg->val.a.type = arrtype;
            start_arg->val.a.len = num_read;

            arg = start_arg; // invariant: arg is back at the start
            break;
        }
        case 'B': // blob
        {
            arg->type = 'b';
            sscanf(src, "BLOB [ %"PRIi32" %n", &arg->val.b.len, &rd);
            if(rd)
            {
                src +=rd;

                assert(*bufsize >= (size_t)arg->val.b.len);
                *bufsize -= (size_t)arg->val.b.len;
                arg->val.b.data = (uint8_t*)buffer_for_strings;
                for(int32_t i = 0; i < arg->val.b.len; ++i)
                {
                    int32_t tmp;
                    int rd;
                    sscanf(src, "0x%x %n", &tmp, &rd);
                    arg->val.b.data[i] = tmp;
                    src+=rd;
                }

                ++src; // skip ']'
            }
            else
                src = parse_identifier(src, arg, buffer_for_strings, bufsize);
            break;
        }
        default:
            // nx... ? (n > 0)
            if(is_range_multiplier(src))
            {
                // collect information for range_arg
                int multiplier, rd = 0;
                sscanf(src, "%dx%n", &multiplier, &rd);
                src += rd;
                arg->type = '-';
                arg->val.r.num = multiplier;
                arg->val.r.has_delta = 0;

                ++arg;
                // read value-arg
                size_t old_bufsize = *bufsize;
                size_t tmp = rtosc_scan_arg_val(src, arg, nargs,
                                                buffer_for_strings, bufsize,
                                                0, 0);
                buffer_for_strings += old_bufsize - *bufsize;
                int args_scanned = next_arg_offset(arg);
                nargs -= args_scanned;
                arg += args_scanned;
                src += tmp;
            }
            // is it an identifier?
            else if(*src == '_' || isalpha(*src))
            {
                arg->type = 'S';
                arg->val.s = buffer_for_strings;
                for(; *src == '_' || isalnum(*src); ++src)
                {
                    assert(*bufsize);
                    (*bufsize)--;
                    *buffer_for_strings = *src;
                    ++buffer_for_strings;
                }
                assert(*bufsize);
                (*bufsize)--;
                *buffer_for_strings = 0;
                ++buffer_for_strings;
            }
            // "YYYY-" => it's a date
            else if(src[0] && src[1] && src[2] && src[3] && src[4] == '-')
            {
                arg->val.t = 0;

                struct tm m_tm;
                m_tm.tm_hour = 0;
                m_tm.tm_min = 0;
                m_tm.tm_sec = 0;
                sscanf(src, "%4d-%2d-%2d%n",
                       &m_tm.tm_year, &m_tm.tm_mon, &m_tm.tm_mday, &rd);
                src+=rd;
                float secfracsf;

                rd = 0;
                sscanf(src, " %2d:%2d%n", &m_tm.tm_hour, &m_tm.tm_min, &rd);
                if(rd)
                 src+=rd;

                rd = 0;
                sscanf(src, ":%2d%n", &m_tm.tm_sec, &rd);
                if(rd)
                 src+=rd;

                uint64_t secfracs;

                // lossless format is appended in parantheses?
                //  => take it directly from there
                if(skip_fmt(&src, "%*f (%n"))
                {
                    sscanf(src, " ... + 0x%8"PRIx64"p-32 s )%n",
                           &secfracs, &rd);
                    src += rd;
                }
                // float number, but not lossless?
                //  => convert it to fractions of seconds
                else if(*src == '.')
                {
                    sscanf(src, "%f%n", &secfracsf, &rd);
                    src += rd;

                    secfracs = rtosc_float2secfracs(secfracsf);
                }
                else
                {
                    // no fractional / floating seconds part
                    secfracs = 0;
                }

                rtosc_arg_val_from_params(arg, &m_tm, secfracs);
            }
            else
            {
                char bytes16[16];
                char type = arg->type = 0;

                bool repeat_once = false;
                do
                {
                    rd = 0;

                    const char *fmtstr = scanf_fmtstr_scan(src, bytes16,
                                                           &type);
                    if(!arg->type) // the first occurence determins the type
                     arg->type = type;

                    switch(type)
                    {
                        case 'h':
                            sscanf(src, fmtstr, &arg->val.h, &rd); break;
                        case 'i':
                            sscanf(src, fmtstr, &arg->val.i, &rd); break;
                        case 'f':
                            sscanf(src, fmtstr, &arg->val.f, &rd); break;
                        case 'd':
                            sscanf(src, fmtstr, &arg->val.d, &rd); break;
                    }
                    src += rd;

                    if(repeat_once)
                    {
                        // we have read the lossless part. skip spaces and ')'
                        skip_fmt(&src, " )%n");
                        repeat_once = false;
                    }
                    else
                    {
                        // is a lossless part appended in parantheses?
                        const char* after_num = src;
                        skip_while(&after_num, isspace);
                        if(*after_num == '(') {
                            ++after_num;
                            skip_while(&after_num, isspace);
                            src = after_num;
                            repeat_once = true;
                        }
                    }
                } while(repeat_once);
            } // date vs integer
        // case ident
    } // switch

    // is the argument being followed by an ellipsis?
    const char* src2 = src;
    for(; isspace(*src2); ++src2) ;

    if(follow_ellipsis && !strncmp(src2, "...", 3))
    {
        src = src2;
        rtosc_arg_val_t delta, rhs;
        size_t zero;

        // lhsarg has already been read
        rtosc_arg_val_t lhsarg = *arg;

        // skip ellipsis and read rhs
        src += 2;
        while(isspace(*++src)) ;

        int infinite_range = (*src == ']');

        if(!infinite_range)
            src += rtosc_scan_arg_val(src, &rhs, 1, NULL, &zero, 0, 0);

        /*
            these shall be conforming to delta_from_arg_vals,
            i.e. if ranges,    the last elements of the ranges
                 if no ranges, the elements themselves
         */
        // find llhs position
        rtosc_arg_val_t tmp;
        // argument "-2" could be a delta arg
        rtosc_arg_val_t* llhsarg = (args_before > 2 &&
                                    arg[-3].type == '-' &&
                                    arg[-3].val.r.has_delta)
                      // -2 is a delta arg (followin a range arg)?
                      ? rtosc_arg_val_range_arg(arg-3, arg[-3].val.r.num-1,
                                                &tmp)
                      : (args_before > 1 && arg[-2].type == '-')
                      // -2 is a range arg (without delta)?
                      ? arg-1
                      : arg-1; // normal case

        bool llhsarg_is_useless =
            (args_before < 1 ||
            lhsarg.type == '-' || !types_match(llhsarg->type, lhsarg.type)
            /* this includes llhsarg == '-' */ );

        bool has_delta = true;
        int32_t num;
        if(infinite_range && llhsarg_is_useless)
        {
            has_delta = false;
            num = 0; // suppress compiler warnings
        }
        else
        {
            num = delta_from_arg_vals(llhsarg, &lhsarg,
                                      infinite_range ? NULL : &rhs,
                                      &delta, llhsarg_is_useless);

            assert(infinite_range || num > 0);
            if(infinite_range && num == -1)
            {
                has_delta = false;
            }
        }

        insert_arg_range(arg, num, &lhsarg, has_delta, &delta, true, true);
    }

    return (size_t)(src-start);
}

size_t rtosc_scan_arg_vals(const char* src,
                           rtosc_arg_val_t *args, size_t n,
                           char* buffer_for_strings, size_t bufsize)
{
    size_t last_bufsize;
    size_t rd=0;
    for(size_t i = 0; i < n; )
    {
        last_bufsize = bufsize;

        size_t tmp = rtosc_scan_arg_val(src, args, n-i,
                                        buffer_for_strings, &bufsize, i, 1);
        src += tmp;
        rd += tmp;
        size_t length = next_arg_offset(args);
        i += length;
        args += length;

        size_t scanned = last_bufsize - bufsize;
        buffer_for_strings += scanned;

        do
        {
            rd += skip_fmt(&src, " %n");
            while(*src == '%')
                rd += skip_fmt(&src, "%*[^\n]%n");
        } while(isspace(*src));
    }
    return rd;
}

size_t rtosc_scan_message(const char* src,
                          char* address, size_t adrsize,
                          rtosc_arg_val_t *args, size_t n,
                          char* buffer_for_strings, size_t bufsize)
{
    size_t rd = 0;
    for(;*src && isspace(*src); ++src) ++rd;
    while (*src == '%')
        rd += skip_fmt(&src, "%*[^\n] %n");

    assert(*src == '/');
    for(; *src && !isspace(*src) && rd < adrsize; ++rd)
        *address++ = *src++;
    assert(rd < adrsize); // otherwise, the address was too long
    *address = 0;

    for(;*src && isspace(*src); ++src) ++rd;

    rd += rtosc_scan_arg_vals(src, args, n, buffer_for_strings, bufsize);

    return rd;
}

