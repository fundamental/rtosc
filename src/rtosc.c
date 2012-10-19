#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include <rtosc.h>

const char *arg_str(const char *msg)
{
    assert(msg && *msg);
    while(*++msg); //skip pattern
    while(!*++msg);//skip null
    return msg+1;  //skip comma
}

unsigned nargs(const char *msg)
{
    return strlen(arg_str(msg));
}

static bool has_reserved(char type)
{
    switch(type)
    {
        case 'i':
        case 's':
        case 'b':
        case 'f':
            return 1;
        case 'T':
        case 'F':
        case 'I':
            return 0;
    }

    //Should not happen
    return 0;
}

unsigned nreserved(const char *args)
{
    unsigned res = 0;
    do {
        res += has_reserved(*args);
    } while(*++args);

    return res;
}

char type(const char *msg, unsigned pos)
{
    assert(pos < nargs(msg));
    return arg_str(msg)[pos];
}

unsigned arg_off(const char *msg, unsigned idx)
{
    if(!has_reserved(arg_str(msg)[idx]))
        return 0;

    //Iterate to the right position
    const unsigned char *args = (const unsigned char*) arg_str(msg);
    const unsigned char *arg_pos = args;

    while(*++arg_pos);
    //Alignment
    arg_pos += 4-(arg_pos-(unsigned char*)msg)%4;

    while(idx--) {
        switch(*args++)
        {
            case 'i':
            case 'f':
                arg_pos += 4;
                break;
            case 's':
                while(*++arg_pos);
                arg_pos += 4-(arg_pos-(unsigned char*)msg)%4;
                break;
            case 'T':
            case 'F':
            case 'I':
                ;
        }
    }
    return arg_pos-(unsigned char*)msg;
}

size_t sosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        ...)
{
    va_list va;
    va_start(va, arguments);
    return vsosc(buffer, len, address, arguments, va);
}

//Calculate the size of the message without writing to a buffer
size_t vsosc_null(const char *address,
                  const char *arguments,
                  va_list     ap)
{
    unsigned pos = 0;
    pos += strlen(address);
    pos += 4-pos%4;//get 32 bit alignment
    pos += 1+strlen(arguments);
    pos += 4-pos%4;

    unsigned toparse = nreserved(arguments);

    //Take care of varargs
    while(toparse)
    {
        char arg = *arguments++;
        assert(arg);
        int i;
        const char *s;
        switch(arg) {
            case 'i':
                (void) va_arg(ap, int32_t);
                pos += 4;
                --toparse;
                break;
            case 's':
                s = va_arg(ap, const char*);
                pos += strlen(s);
                pos += 4-pos%4;
                --toparse;
                break;
            case 'b':
                i = va_arg(ap, int32_t);
                pos += 4 + i;
                (void) va_arg(ap, const char *);
                break;
            case 'f':
                (void) va_arg(ap, double);
                pos += 4;
                --toparse;
            default:
                ;
        }
    }

    va_end(ap);

    return pos;
}

size_t vsosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        va_list ap)
{
    va_list _ap;
    va_copy(_ap,ap);
    const size_t total_len = vsosc_null(address, arguments, _ap);

    if(!buffer)
        return total_len;

    memset(buffer, 0, len);

    //Abort if the message cannot fit
    if(total_len>len)
        return 0;

    unsigned pos = 0;
    while(*address)
        buffer[pos++] = *address++;

    //get 32 bit alignment
    pos += 4-pos%4;

    buffer[pos++] = ',';

    const char *args = arguments;
    while(*args)
        buffer[pos++] = *args++;

    pos += 4-pos%4;

    unsigned toparse = nreserved(arguments);
    //Take care of varargs
    while(toparse)
    {
        char arg = *arguments++;
        assert(arg);
        float f;
        int32_t i;
        const char *s;
        switch(arg) {
            case 'i':
                i = va_arg(ap, int32_t);
                buffer[pos++] = ((i>>24) & 0xff);
                buffer[pos++] = ((i>>16) & 0xff);
                buffer[pos++] = ((i>>8) & 0xff);
                buffer[pos++] = (i & 0xff);
                --toparse;
                break;
            case 's':
                s = va_arg(ap, const char*);
                while(*s)
                    buffer[pos++] = *s++;
                pos += 4-pos%4;
                --toparse;
                break;
            case 'b':
                i = va_arg(ap, int32_t);
                buffer[pos++] = ((i>>24) & 0xff);
                buffer[pos++] = ((i>>16) & 0xff);
                buffer[pos++] = ((i>>8) & 0xff);
                buffer[pos++] = (i & 0xff);
                s = va_arg(ap, const char *);
                if(s) {
                    while(i--)
                        buffer[pos++] = *s++;
                }
                else
                    pos += i;
                pos += 4-pos%4;
                --toparse;
                break;
            case 'f':
                f = va_arg(ap, double);
                *(float*)(buffer + pos) = f;
                pos += 4;
                --toparse;
            default:
                ;
        }
        //float
    }

    va_end(ap);

    return pos;
}

arg_t argument(const char *msg, unsigned idx)
{
    arg_t result = {0};
    char type = arg_str(msg)[idx];
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
        const unsigned char *arg_pos = (const unsigned char*)msg+arg_off(msg,idx);
        switch(type)
        {
            case 'i':
                result.i |= (*arg_pos++ << 24);
                result.i |= (*arg_pos++ << 16);
                result.i |= (*arg_pos++ << 8);
                result.i |= (*arg_pos++);
                break;
            case 'b':
                result.b.len |= (*arg_pos++ << 24);
                result.b.len |= (*arg_pos++ << 16);
                result.b.len |= (*arg_pos++ << 8);
                result.b.len |= (*arg_pos++);
                result.b.data = (unsigned char *)arg_pos;
                break;
            case 'f':
                result.f = *(float*)arg_pos;
                break;
        }
    }

    return result;
}
