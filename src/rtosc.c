#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <inttypes.h>

#include <rtosc/rtosc.h>

const char *rtosc_argument_string(const char *msg)
{
    assert(msg && *msg);
    while(*++msg); //skip pattern
    while(!*++msg);//skip null
    return msg+1;  //skip comma
}

unsigned rtosc_narguments(const char *msg)
{
    const char *args = rtosc_argument_string(msg);
    int nargs = 0;
    while(*args++)
        nargs += (*args == ']' || *args == '[') ? 0 : 1;
    return nargs;
}

static int has_reserved(char type)
{
    switch(type)
    {
        case 'i'://official types
        case 's':
        case 'b':
        case 'f':

        case 'h'://unofficial
        case 't':
        case 'd':
        case 'S':
        case 'r':
        case 'm':
        case 'c':
            return 1;
        case 'T':
        case 'F':
        case 'N':
        case 'I':
        case '[':
        case ']':
            return 0;
    }

    //Should not happen
    return 0;
}

static unsigned nreserved(const char *args)
{
    unsigned res = 0;
    for(;*args;++args)
        res += has_reserved(*args);

    return res;
}

char rtosc_type(const char *msg, unsigned nargument)
{
    assert(nargument < rtosc_narguments(msg));
    const char *arg = rtosc_argument_string(msg);
    while(1) {
        if(*arg == '[' || *arg == ']')
            ++arg;
        else if(!nargument || !*arg)
            return *arg;
        else
            ++arg, --nargument;
    }
}

static unsigned arg_start(const char *msg_)
{
    const uint8_t *msg = (const uint8_t*)msg_;
    //Iterate to the right position
    const uint8_t *args = (const uint8_t*) rtosc_argument_string(msg_);
    const uint8_t *aligned_ptr = args-1;
    const uint8_t *arg_pos = args;

    while(*++arg_pos);
    //Alignment
    arg_pos += 4-(arg_pos-aligned_ptr)%4;
    return arg_pos-msg;
}

static unsigned arg_size(const uint8_t *arg_mem, char type)
{
    if(!has_reserved(type))
        return 0;
    const uint8_t  *arg_pos=arg_mem;
    uint32_t blob_length = 0;
    switch(type)
    {
        case 'h':
        case 't':
        case 'd':
            return 8;
        case 'm':
        case 'r':
        case 'f':
        case 'c':
        case 'i':
            return 4;
        case 'S':
        case 's':
            while(*++arg_pos);
            arg_pos += 4-(arg_pos-arg_mem)%4;
            return arg_pos-arg_mem;
        case 'b':
            blob_length |= (*arg_pos++ << 24);
            blob_length |= (*arg_pos++ << 16);
            blob_length |= (*arg_pos++ << 8);
            blob_length |= (*arg_pos++);
            if(blob_length%4)
                blob_length += 4-blob_length%4;
            arg_pos += blob_length;
            return arg_pos-arg_mem;
        default:
            assert("Invalid Type");
    }
    return -1;
}

static unsigned arg_off(const char *msg, unsigned idx)
{
    if(!has_reserved(rtosc_type(msg,idx)))
        return 0;

    //Iterate to the right position
    const uint8_t *args = (const uint8_t*) rtosc_argument_string(msg);
    const uint8_t *aligned_ptr = args-1;
    const uint8_t *arg_pos = args;

    while(*++arg_pos);
    //Alignment
    arg_pos += 4-(arg_pos-((uint8_t*)aligned_ptr))%4;

    //ignore any leading '[' or ']'
    while(*args == '[' || *args == ']')
        ++args;

    while(idx--) {
        char type = *args++;
        if(type == '[' || type == ']')
            idx++;//not a valid arg idx
        else
            arg_pos += arg_size(arg_pos, type);
    }
    return arg_pos-(uint8_t*)msg;
}

size_t rtosc_message(char   *buffer,
                     size_t      len,
                     const char *address,
                     const char *arguments,
                     ...)
{
    va_list va;
    va_start(va, arguments);
    size_t result = rtosc_vmessage(buffer, len, address, arguments, va);
    va_end(va);
    return result;
}

//Calculate the size of the message without writing to a buffer
static size_t vsosc_null(const char        *address,
                         const char        *arguments,
                         const rtosc_arg_t *args)
{
    unsigned pos = 0;
    pos += strlen(address);
    pos += 4-pos%4;//get 32 bit alignment
    pos += 1+strlen(arguments);
    pos += 4-pos%4;

    unsigned toparse = nreserved(arguments);
    unsigned arg_pos = 0;

    //Take care of varargs
    while(toparse)
    {
        char arg = *arguments++;
        assert(arg);
        int i;
        const char *s;
        switch(arg) {
            case 'h':
            case 't':
            case 'd':
                ++arg_pos;
                pos += 8;
                --toparse;
                break;
            case 'm':
            case 'r':
            case 'c':
            case 'f':
            case 'i':
                ++arg_pos;
                pos += 4;
                --toparse;
                break;
            case 's':
            case 'S':
                s = args[arg_pos++].s;
                assert(s && "Input strings CANNOT be NULL");
                pos += strlen(s);
                pos += 4-pos%4;
                --toparse;
                break;
            case 'b':
                i = args[arg_pos++].b.len;
                pos += 4 + i;
                if(pos%4)
                    pos += 4-pos%4;
                --toparse;
                break;
            default:
                ;
        }
    }

    return pos;
}

void rtosc_v2args(rtosc_arg_t* args, size_t nargs, const char* arg_str,
                  va_list_t* ap)
{
    unsigned arg_pos = 0;
    uint8_t *midi_tmp;

    while(arg_pos < nargs)
    {
        switch(*arg_str++) {
            case 'h':
            case 't':
                args[arg_pos++].h = va_arg(ap->a, int64_t);
                break;
            case 'd':
                args[arg_pos++].d = va_arg(ap->a, double);
                break;
            case 'c':
            case 'i':
            case 'r':
                args[arg_pos++].i = va_arg(ap->a, int);
                break;
            case 'm':
                midi_tmp = va_arg(ap->a, uint8_t *);
                args[arg_pos].m[0] = midi_tmp[0];
                args[arg_pos].m[1] = midi_tmp[1];
                args[arg_pos].m[2] = midi_tmp[2];
                args[arg_pos++].m[3] = midi_tmp[3];
                break;
            case 'S':
            case 's':
                args[arg_pos++].s = va_arg(ap->a, const char *);
                break;
            case 'b':
                args[arg_pos].b.len = va_arg(ap->a, int);
                args[arg_pos].b.data = va_arg(ap->a, unsigned char *);
                arg_pos++;
                break;
            case 'f':
                args[arg_pos++].f = va_arg(ap->a, double);
                break;
            case 'T':
            case 'F':
            case 'N':
            case 'I':
                args[arg_pos++].T = arg_str[-1];
                break;
            default:
                ;
        }
    }
}

void rtosc_2args(rtosc_arg_t* args, size_t nargs, const char* arg_str, ...)
{
    va_list_t va;
    va_start(va.a, arg_str);
    rtosc_v2args(args, nargs, arg_str, &va);
    va_end(va.a);
}

void rtosc_v2argvals(rtosc_arg_val_t* args, size_t nargs, const char* arg_str, va_list ap)
{
    va_list_t ap2;
    va_copy(ap2.a, ap);
    for(size_t i=0; i<nargs; ++i, ++arg_str, ++args)
    {
        args->type = *arg_str;
        rtosc_v2args(&args->val, 1, arg_str, &ap2);
    }
}

void rtosc_2argvals(rtosc_arg_val_t* args, size_t nargs, const char* arg_str, ...)
{
    va_list va;
    va_start(va, arg_str);
    rtosc_v2argvals(args, nargs, arg_str, va);
    va_end(va);
}

size_t rtosc_vmessage(char   *buffer,
                      size_t      len,
                      const char *address,
                      const char *arguments,
                      va_list ap)
{
    const unsigned nargs = nreserved(arguments);
    if(!nargs)
        return rtosc_amessage(buffer,len,address,arguments,NULL);

    rtosc_arg_t args[nargs];
    va_list_t ap2;
    va_copy(ap2.a, ap);
    rtosc_v2args(args, nargs, arguments, &ap2);

    return rtosc_amessage(buffer,len,address,arguments,args);
}

size_t rtosc_amessage(char              *buffer,
                      size_t             len,
                      const char        *address,
                      const char        *arguments,
                      const rtosc_arg_t *args)
{
    const size_t total_len = vsosc_null(address, arguments, args);

    if(!buffer)
        return total_len;

    //Abort if the message cannot fit
    if(total_len>len) {
        memset(buffer, 0, len);
        return 0;
    }

    memset(buffer, 0, total_len);

    unsigned pos = 0;
    while(*address)
        buffer[pos++] = *address++;

    //get 32 bit alignment
    pos += 4-pos%4;

    buffer[pos++] = ',';

    const char *arg_str = arguments;
    while(*arg_str)
        buffer[pos++] = *arg_str++;

    pos += 4-pos%4;

    unsigned toparse = nreserved(arguments);
    unsigned arg_pos = 0;
    while(toparse)
    {
        char arg = *arguments++;
        assert(arg);
        int32_t i;
        int64_t d;
        const uint8_t *m;
        const char *s;
        const unsigned char *u;
        rtosc_blob_t b;
        switch(arg) {
            case 'h':
            case 't':
            case 'd':
                d = args[arg_pos++].t;
                buffer[pos++] = ((d>>56) & 0xff);
                buffer[pos++] = ((d>>48) & 0xff);
                buffer[pos++] = ((d>>40) & 0xff);
                buffer[pos++] = ((d>>32) & 0xff);
                buffer[pos++] = ((d>>24) & 0xff);
                buffer[pos++] = ((d>>16) & 0xff);
                buffer[pos++] = ((d>>8) & 0xff);
                buffer[pos++] = (d & 0xff);
                --toparse;
                break;
            case 'r':
            case 'f':
            case 'c':
            case 'i':
                i = args[arg_pos++].i;
                buffer[pos++] = ((i>>24) & 0xff);
                buffer[pos++] = ((i>>16) & 0xff);
                buffer[pos++] = ((i>>8) & 0xff);
                buffer[pos++] = (i & 0xff);
                --toparse;
                break;
            case 'm':
                //TODO verify ordering of spec
                m = args[arg_pos++].m;
                buffer[pos++] = m[0];
                buffer[pos++] = m[1];
                buffer[pos++] = m[2];
                buffer[pos++] = m[3];
                --toparse;
                break;
            case 'S':
            case 's':
                s = args[arg_pos++].s;
                while(*s)
                    buffer[pos++] = *s++;
                pos += 4-pos%4;
                --toparse;
                break;
            case 'b':
                b = args[arg_pos++].b;
                i = b.len;
                buffer[pos++] = ((i>>24) & 0xff);
                buffer[pos++] = ((i>>16) & 0xff);
                buffer[pos++] = ((i>>8) & 0xff);
                buffer[pos++] = (i & 0xff);
                u = b.data;
                if(u) {
                    while(i--)
                        buffer[pos++] = *u++;
                }
                else
                    pos += i;
                if(pos%4)
                    pos += 4-pos%4;
                --toparse;
                break;
            default:
                ;
        }
    }

    return pos;
}

static rtosc_arg_t extract_arg(const uint8_t *arg_pos, char type)
{
    rtosc_arg_t result = {0};
    //trivial case
    if(!has_reserved(type)) {
        switch(type)
        {
            case 'T':
                result.T = true;
                break;
            case 'F':
                result.T = false;
                break;
            default:
                ;
        }
    } else {
        switch(type)
        {
            case 'h':
            case 't':
            case 'd':
                result.t |= (((uint64_t)*arg_pos++) << 56);
                result.t |= (((uint64_t)*arg_pos++) << 48);
                result.t |= (((uint64_t)*arg_pos++) << 40);
                result.t |= (((uint64_t)*arg_pos++) << 32);
                result.t |= (((uint64_t)*arg_pos++) << 24);
                result.t |= (((uint64_t)*arg_pos++) << 16);
                result.t |= (((uint64_t)*arg_pos++) << 8);
                result.t |= (((uint64_t)*arg_pos++));
                break;
            case 'r':
            case 'f':
            case 'c':
            case 'i':
                result.i |= (*arg_pos++ << 24);
                result.i |= (*arg_pos++ << 16);
                result.i |= (*arg_pos++ << 8);
                result.i |= (*arg_pos++);
                break;
            case 'm':
                result.m[0] = *arg_pos++;
                result.m[1] = *arg_pos++;
                result.m[2] = *arg_pos++;
                result.m[3] = *arg_pos++;
                break;
            case 'b':
                result.b.len |= (*arg_pos++ << 24);
                result.b.len |= (*arg_pos++ << 16);
                result.b.len |= (*arg_pos++ << 8);
                result.b.len |= (*arg_pos++);
                result.b.data = (unsigned char *)arg_pos;
                break;
            case 'S':
            case 's':
                result.s = (char *)arg_pos;
                break;
        }
    }

    return result;
}

static const char *advance_past_dummy_args(const char *args)
{
    while(*args == '[' || *args == ']')
        args++;
    return args;
}

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

static const rtosc_print_options* default_print_options
 = &((rtosc_print_options) { true, 2, " ", 80});

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

size_t rtosc_print_arg_val(const rtosc_arg_val_t *arg,
                           char *buffer, size_t bs,
                           const rtosc_print_options* opt,
                           int *cols_used)
{
    size_t wrt = 0;
    if(!opt)
        opt = default_print_options;
    assert(arg);
    const rtosc_arg_t* val = &arg->val;

    switch(arg->type) {
        case 'T':
            assert(bs>4);
            strncpy(buffer, "true", bs);
            wrt = 4;
            break;
        case 'F':
            assert(bs>5);
            strncpy(buffer, "false", bs);
            wrt = 5;
            break;
        case 'N':
            assert(bs>3);
            strncpy(buffer, "nil", bs);
            wrt = 3;
            break;
        case 'I':
            assert(bs>3);
            strncpy(buffer, "inf", bs);
            wrt = 3;
            break;
        case 'h':
            wrt = asnprintf(buffer, bs, "%"PRId64"h", val->h);
            break;
        case 't': // write to ISO 8601 date
        {
            if(val->t == 1)
                wrt = asnprintf(buffer, bs, "immediatelly");
            else
            {
                time_t t = (time_t)(val->t >> 32);
                int32_t secfracs = val->t & (0xffffffff);
                struct tm* m_tm = localtime(&t);

                const char* strtimefmt = (secfracs || m_tm->tm_sec)
                                  ? "%Y-%m-%d %H:%M:%S"
                                  : (m_tm->tm_hour || m_tm->tm_min)
                                    ? "%Y-%m-%d %H:%M"
                                    : "%Y-%m-%d";

                wrt = strftime(buffer, bs, strtimefmt, m_tm);
                assert(wrt);

                if(secfracs)
                {
                    int rd = 0;
                    int prec = opt->floating_point_precision;
                    assert(prec>=0);
                    assert(prec<100);

                    // convert fractions -> float
                    char lossless[16];
                    asnprintf(lossless, 16, "0x%xp-32", secfracs);
                    float flt;
                    sscanf(lossless, "%f%n", &flt, &rd);
                    assert(rd);

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
        case 'S':
        case 's':
        {
            char* b = buffer;
            *b++ = '"';
            for(const char* s = val->s; *s; ++s)
            {
                if(*cols_used >= opt->linelength - 2)
                    break_string(&b, bs, wrt, cols_used);
                assert(bs);
                int as_esc = as_escaped_char(*s, false);
                if(as_esc != -1) {
                    assert(bs-1);
                    *b++ = '\\';
                    *b++ = as_esc;
                    *cols_used += 2;
                    if(as_esc == 'n')
                        break_string(&b, bs, wrt, cols_used);
                }
                else {
                    *b++ = *s;
                    ++*cols_used;
                }
            }
            *b++ = '"';
            *b = 0;
            wrt += (b-buffer);
            break;
        }
        case 'b':
            wrt = asnprintf(buffer, bs, "[%d ", val->b.len);
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
                wrt += asnprintf(buffer, bs, "0x%02x ", val->b.data[i]);
                bs -= 5;
                buffer += 5;
                *cols_used += 5;
            }
            buffer[-1] = ']';
            break;
        default:
            ;
    }

    switch(arg->type)
    {
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
    for(size_t i = 0; i < n; ++i)
    {
        size_t tmp = rtosc_print_arg_val(args++, buffer, bs, opt, &cols_used);

        wrt += tmp;
        buffer += tmp;
        bs -= tmp;
        ++args_written_this_line;
        // did we break the line length,
        // and this is not the first arg written in this line?
        if(cols_used > opt->linelength && (args_written_this_line > 1))
        {
            // insert "\n    "
            *last_sep = '\n';
            assert(bs >= 4);
            memmove(last_sep+5, last_sep+1, tmp);
            last_sep[1] = last_sep[2] = last_sep[3] = last_sep[4] = ' ';
            cols_used = 4 + wrt;
            wrt += 4;
            buffer += 4;
            bs -= 4;
            args_written_this_line = 0;
        }
        if(i<n-1)
        {
            assert(sep_len < bs);
            last_sep = buffer;
            strncpy(buffer, opt->sep, bs);
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
    for(;*end && !isspace(*end) && (*end != ')');++end);

    int exp = end - src;

    // store type byte in the parameter, or in a temporary variable?
    char tmp;
    char* _type = type ? type : &tmp;

    const char* r; // result
    int ok = (r = try_fmt(src, exp, "%*"PRIi64"h%n", _type, 'h'))
          || (r = try_fmt(src, exp, "%*"PRIi32"i%n", _type, 'i'))
          || (r = try_fmt(src, exp, "%*"PRIi32"%n", _type, 'i'))
          || (r = try_fmt(src, exp, "%*lfd%n", _type, 'd'))
          || (r = try_fmt(src, exp, "%*ff%n", _type, 'f'))
          || (r = try_fmt(src, exp, "%*f%n", _type, 'f'));
    (void)ok;
    return r;
}

/** Return the right format string for reading the next numeric value */
static const char* scanf_fmtstr_scan(const char* src, char* bytes8,
                                     char* type)
{
    strncpy(bytes8, scanf_fmtstr(src, type), 8);
    *++bytes8 = '%'; // transform "%*" to "%"
    return bytes8;
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

const char* rtosc_skip_next_printed_arg(const char* src)
{
    switch(*src)
    {
        case 't': skip_fmt_null(&src, "true%n"); break;
        case 'f': skip_fmt_null(&src, "false%n"); break;
        case 'n':
            if(src[1] == 'o')
                skip_fmt_null(&src, "now%n");
            else
                skip_fmt_null(&src, "nil%n");
            break;
        case 'i':
            if(src[1] == 'n')
                skip_fmt_null(&src, "inf%n");
            else
                skip_fmt_null(&src, "immediatelly%n");
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
            break;
        }
        case '"':
            src = end_of_printed_string(src);
            break;
        case 'M':
            skip_fmt_null(&src, "MIDI [ 0x%*x 0x%*x 0x%*x 0x%*x ]%n");
            break;
        case '[':
        {
            int rd = 0, blobsize = 0;
            sscanf(src, "[ %i %n", &blobsize, &rd);
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
            break;
        }
        default:
        {
            // is it a date? (vs a numeric)
            if(skip_fmt(&src, "%*4d-%*1d%*1d-%*1d%*1d%n"))
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
            }
            else
            {
                char type;
                int rd = skip_numeric(&src, &type);
                if(!rd) { src = NULL; break; }
                const char* after_num = src;
                skip_while(&after_num, isspace);
                if(*after_num == '(')
                {
                    if (type == 'f' || type =='d')
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
    return src;
}

int rtosc_count_printed_arg_vals(const char* src)
{
    int num = 0;

    skip_while(&src, isspace);
    while (*src == '%')
        skip_fmt(&src, "%*[^\n] %n");

    if(*src == '/') // skip address?
    {
        for(; *src && !isspace(*src);++src);
        skip_while(&src, isspace);
        while (*src == '%')
            skip_fmt(&src, "%*[^\n] %n");
    }
    for(; src && *src && *src != '/'; ++num)
    {
        src = rtosc_skip_next_printed_arg(src);
        if(src) // parse error
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
    else
        return 0;
}

size_t rtosc_scan_arg_val(const char* src,
                          rtosc_arg_val_t *arg,
                          char* buffer_for_strings, size_t* bufsize)
{
    int rd = 0;
    const char* start = src;
    switch(*src)
    {
        case 't':
        case 'f':
        case 'n':
        case 'i':
            // timestamps "immediatelly" or "now"?
            if(src[1] == 'm' || src[1] == 'o')
            {
                arg->type = 't';
                arg->val.t = 1;
                src += (src[1] == 'm') ? 12 : 3;
            }
            else
            {
                arg->type = arg->val.T = toupper(*src);
                switch(*src)
                {
                    case 'n':
                    case 'i': src += 3; break;
                    case 't': src += 4; break;
                    case 'f': src += 5; break;
                }
            }
            break;
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
            arg->type = 's';
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
            break;
        }
        case 'M':
        {
            arg->type = 'm';
            int32_t tmp[4];
            sscanf(src, "MIDI [ 0x%"PRIx32" 0x%"PRIx32
                              " 0x%"PRIx32" 0x%"PRIx32" ]%n",
                   tmp, tmp + 1, tmp + 2, tmp + 3, &rd); src+=rd;
            for(size_t i = 0; i < 4; ++i)
                arg->val.m[i] = tmp[i]; // copy to 8 bit array
            break;
        }
        case '[': // blob
        {
            arg->type = 'b';
            while( isspace(*++src) ) ;
            sscanf(src, "%"PRIi32" %n", &arg->val.b.len, &rd);
            src +=rd;

            assert(*bufsize >= (size_t)arg->val.b.len);
            *bufsize -= (size_t)arg->val.b.len;

            arg->val.b.data = (uint8_t*)buffer_for_strings;
            for(int32_t i = 0; i < arg->val.b.len; ++i)
            {
                int32_t tmp;
                int rd;
                sscanf(src, "0x%x %n", &tmp,&rd);
                arg->val.b.data[i] = tmp;
                src+=rd;
            }

            ++src; // skip ']'
            break;
        }
        default:
            // "YYYY-" => it's a date
            if(src[0] && src[1] && src[2] && src[3] && src[4] == '-')
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

                // lossless format is appended in parantheses?
                //  => take it directly from there
                if(skip_fmt(&src, "%*f (%n"))
                {

                    sscanf(src, " ... + 0x%8"PRIx64"p-32 s )%n",
                           &arg->val.t, &rd);
                    src += rd;
                }
                // float number, but not lossless?
                //  => convert it to fractions of seconds
                else if(*src == '.')
                {
                    sscanf(src, "%f%n", &secfracsf, &rd);
                    src += rd;

                    // convert float -> secfracs
                    char secfracs_as_hex[16];
                    asnprintf(secfracs_as_hex, 16, "%a", secfracsf);
                    assert(secfracs_as_hex[3]=='.'); // 0x?.
                    secfracs_as_hex[3] = secfracs_as_hex[2]; // remove '.'
                    uint64_t secfracs;
                    int exp;
                    sscanf(secfracs_as_hex + 3,
                           "%"PRIx64"p-%i", &secfracs, &exp);
                    const char* p = strchr(secfracs_as_hex, 'p');
                    assert(p);
                    int lshift = 32-exp-((int)(p-(secfracs_as_hex+4))<<2);
                    assert(lshift > 0);
                    secfracs <<= lshift;
                    assert((secfracs & 0xFFFFFFFF) == secfracs);

                    arg->val.t = secfracs;
                }
                else
                {
                    // no fractional / floating seconds part
                }

                // adjust ranges to be POSIX conform
                m_tm.tm_year -= 1900;
                --m_tm.tm_mon;
                // don't mess around with Daylight Saving Time
                m_tm.tm_isdst = -1;

                arg->val.t |= (((uint64_t)mktime(&m_tm)) << ((uint64_t)32));
                arg->type = 't';
            }
            else
            {
                char bytes8[8];
                char type = arg->type = 0;

                bool repeat_once = false;
                do
                {
                    rd = 0;

                    const char *fmtstr = scanf_fmtstr_scan(src, bytes8,
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
    return (size_t)(src-start);
}

size_t rtosc_scan_arg_vals(const char* src,
                           rtosc_arg_val_t *args, size_t n,
                           char* buffer_for_strings, size_t bufsize)
{
    size_t last_bufsize;
    size_t rd=0;
    for(size_t i = 0; i < n; ++i)
    {
        last_bufsize = bufsize;
        size_t tmp = rtosc_scan_arg_val(src, args + i,
                                        buffer_for_strings, &bufsize);
        src += tmp;
        rd += tmp;

        size_t written = last_bufsize - bufsize;
        buffer_for_strings += written;

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
                          char* msgbuf, size_t msgsize,
                          rtosc_arg_val_t *args, size_t n,
                          char* buffer_for_strings, size_t bufsize)
{
    size_t rd = 0;
    for(;*src && isspace(*src); ++src) ++rd;
    while (*src == '%')
        rd += skip_fmt(&src, "%*[^\n] %n");

    assert(*src == '/');
    for(; *src && !isspace(*src) && rd < msgsize; ++rd)
        *msgbuf++ = *src++;
    assert(rd < msgsize); // otherwise, the address was too long
    *msgbuf = 0;

    for(;*src && isspace(*src); ++src) ++rd;

    rd += rtosc_scan_arg_vals(src, args, n, buffer_for_strings, bufsize);

    return rd;
}

rtosc_arg_itr_t rtosc_itr_begin(const char *msg)
{
    rtosc_arg_itr_t itr;
    itr.type_pos  = advance_past_dummy_args(rtosc_argument_string(msg));
    itr.value_pos = (uint8_t*)(msg+arg_start(msg));

    return itr;
}

rtosc_arg_val_t rtosc_itr_next(rtosc_arg_itr_t *itr)
{
    //current position provides the value
    rtosc_arg_val_t result = {0,{0}};
    result.type = *itr->type_pos;
    if(result.type)
        result.val = extract_arg(itr->value_pos, result.type);

    //advance
    itr->type_pos = advance_past_dummy_args(itr->type_pos+1);
    char type = result.type;
    int size  = arg_size(itr->value_pos, type);
    itr->value_pos += size;


    return result;
}

int rtosc_itr_end(rtosc_arg_itr_t itr)
{
    return !itr.type_pos  || !*itr.type_pos;
}

rtosc_arg_t rtosc_argument(const char *msg, unsigned idx)
{
    char type = rtosc_type(msg, idx);
    uint8_t *arg_mem = (uint8_t*)msg + arg_off(msg, idx);
    return extract_arg(arg_mem, type);
}

static unsigned char deref(unsigned pos, ring_t *ring)
{
    return pos<ring[0].len ? ring[0].data[pos] :
        ((pos-ring[0].len)<ring[1].len ? ring[1].data[pos-ring[0].len] : 0x00);
}

static size_t bundle_ring_length(ring_t *ring)
{
    unsigned pos = 8+8;//goto first length field
    uint32_t advance = 0;
    do {
        advance = deref(pos+0, ring) << (8*3) |
                  deref(pos+1, ring) << (8*2) |
                  deref(pos+2, ring) << (8*1) |
                  deref(pos+3, ring) << (8*0);
        if(advance)
            pos += 4+advance;
    } while(advance);

    return pos <= (ring[0].len+ring[1].len) ? pos : 0;
}

//Zero means no full message present
size_t rtosc_message_ring_length(ring_t *ring)
{
    //Check if the message is a bundle
    if(deref(0,ring) == '#' &&
            deref(1,ring) == 'b' &&
            deref(2,ring) == 'u' &&
            deref(3,ring) == 'n' &&
            deref(4,ring) == 'd' &&
            deref(5,ring) == 'l' &&
            deref(6,ring) == 'e' &&
            deref(7,ring) == '\0')
        return bundle_ring_length(ring);

    //Proceed for normal messages
    //Consume path
    unsigned pos = 0;
    while(deref(pos++,ring));
    pos--;

    //Travel through the null word end [1..4] bytes
    for(int i=0; i<4; ++i)
        if(deref(++pos, ring))
            break;

    if(deref(pos, ring) != ',')
        return 0;

    unsigned aligned_pos = pos;
    int arguments = pos+1;
    while(deref(++pos,ring));
    pos += 4-(pos-aligned_pos)%4;

    unsigned toparse = 0;
    {
        int arg = arguments-1;
        while(deref(++arg,ring))
            toparse += has_reserved(deref(arg,ring));
    }

    //Take care of varargs
    while(toparse)
    {
        char arg = deref(arguments++,ring);
        assert(arg);
        uint32_t i;
        switch(arg) {
            case 'h':
            case 't':
            case 'd':
                pos += 8;
                --toparse;
                break;
            case 'm':
            case 'r':
            case 'c':
            case 'f':
            case 'i':
                pos += 4;
                --toparse;
                break;
            case 'S':
            case 's':
                while(deref(++pos,ring));
                pos += 4-(pos-aligned_pos)%4;
                --toparse;
                break;
            case 'b':
                i = 0;
                i |= (deref(pos++,ring) << 24);
                i |= (deref(pos++,ring) << 16);
                i |= (deref(pos++,ring) << 8);
                i |= (deref(pos++,ring));
                pos += i;
                if((pos-aligned_pos)%4)
                    pos += 4-(pos-aligned_pos)%4;
                --toparse;
                break;
            default:
                ;
        }
    }


    return pos <= (ring[0].len+ring[1].len) ? pos : 0;
}

size_t rtosc_message_length(const char *msg, size_t len)
{
    ring_t ring[2] = {{(char*)msg,len},{NULL,0}};
    return rtosc_message_ring_length(ring);
}

bool rtosc_valid_message_p(const char *msg, size_t len)
{
    //Validate Path Characters (assumes printable characters are sufficient)
    if(*msg != '/')
        return false;
    const char *tmp = msg;
    for(unsigned i=0; i<len; ++i) {
        if(*tmp == 0)
            break;
        if(!isprint(*tmp))
            return false;
        tmp++;
    }

    //tmp is now either pointing to a null or the end of the string
    const size_t offset1 = tmp-msg;
    size_t       offset2 = tmp-msg;
    for(; offset2<len; offset2++) {
        if(*tmp == ',')
            break;
        tmp++;
    }

    //Too many NULL bytes
    if(offset2-offset1 > 4)
        return false;

    if((offset2 % 4) != 0)
        return false;

    size_t observed_length = rtosc_message_length(msg, len);
    return observed_length == len;
}
static uint64_t extract_uint64(const uint8_t *arg_pos)
{
    uint64_t arg = 0;
    arg |= (((uint64_t)*arg_pos++) << 56);
    arg |= (((uint64_t)*arg_pos++) << 48);
    arg |= (((uint64_t)*arg_pos++) << 40);
    arg |= (((uint64_t)*arg_pos++) << 32);
    arg |= (((uint64_t)*arg_pos++) << 24);
    arg |= (((uint64_t)*arg_pos++) << 16);
    arg |= (((uint64_t)*arg_pos++) << 8);
    arg |= (((uint64_t)*arg_pos++));
    return arg;
}

static uint32_t extract_uint32(const uint8_t *arg_pos)
{
    uint32_t arg = 0;
    arg |= (((uint32_t)*arg_pos++) << 24);
    arg |= (((uint32_t)*arg_pos++) << 16);
    arg |= (((uint32_t)*arg_pos++) << 8);
    arg |= (((uint32_t)*arg_pos++));
    return arg;
}

static void emplace_uint64(uint8_t *buffer, uint64_t d)
{
    buffer[0] = ((d>>56) & 0xff);
    buffer[1] = ((d>>48) & 0xff);
    buffer[2] = ((d>>40) & 0xff);
    buffer[3] = ((d>>32) & 0xff);
    buffer[4] = ((d>>24) & 0xff);
    buffer[5] = ((d>>16) & 0xff);
    buffer[6] = ((d>>8)  & 0xff);
    buffer[7] = ((d>>0)  & 0xff);
}

static void emplace_uint32(uint8_t *buffer, uint32_t d)
{
    buffer[0] = ((d>>24) & 0xff);
    buffer[1] = ((d>>16) & 0xff);
    buffer[2] = ((d>>8)  & 0xff);
    buffer[3] = ((d>>0)  & 0xff);
}

size_t rtosc_bundle(char *buffer, size_t len, uint64_t tt, int elms, ...)
{
    char *_buffer = buffer;
    memset(buffer, 0, len);
    strcpy(buffer, "#bundle");
    buffer += 8;
    emplace_uint64((uint8_t*)buffer, tt);
    buffer += 8;
    va_list va;
    va_start(va, elms);
    for(int i=0; i<elms; ++i) {
        const char   *msg  = va_arg(va, const char*);
        //It is assumed that any passed message/bundle is valid
        size_t        size = rtosc_message_length(msg, -1);
        emplace_uint32((uint8_t*)buffer, size);
        buffer += 4;
        memcpy(buffer, msg, size);
        buffer+=size;
    }
    va_end(va);

    return buffer-_buffer;
}


#define POS ((size_t)(((const char *)lengths) - buffer))
size_t rtosc_bundle_elements(const char *buffer, size_t len)
{
    const uint32_t *lengths = (const uint32_t*) (buffer+16);
    size_t elms = 0;
    while(POS < len && extract_uint32((const uint8_t*)lengths)) {
        lengths += extract_uint32((const uint8_t*)lengths)/4+1;

        if(POS > len)
            break;
        ++elms;
    }
    return elms;
}
#undef POS

const char *rtosc_bundle_fetch(const char *buffer, unsigned elm)
{
    const uint32_t *lengths = (const uint32_t*) (buffer+16);
    size_t elm_pos = 0;
    while(elm_pos!=elm && extract_uint32((const uint8_t*)lengths)) {
        ++elm_pos;
        lengths += extract_uint32((const uint8_t*)lengths)/4+1;
    }

    return (const char*) (elm==elm_pos?lengths+1:NULL);
}

size_t rtosc_bundle_size(const char *buffer, unsigned elm)
{
    const uint32_t *lengths = (const uint32_t*) (buffer+16);
    size_t elm_pos = 0;
    size_t last_len = 0;
    while(elm_pos!=elm && extract_uint32((const uint8_t*)lengths)) {
        last_len = extract_uint32((const uint8_t*)lengths);
        ++elm_pos, lengths+=extract_uint32((const uint8_t*)lengths)/4+1;
    }

    return last_len;
}

int rtosc_bundle_p(const char *msg)
{
    return !strcmp(msg,"#bundle");
}

uint64_t rtosc_bundle_timetag(const char *msg)
{
    return extract_uint64((const uint8_t*)msg+8);
}

