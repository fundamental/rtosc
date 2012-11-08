#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include <rtosc.h>

typedef unsigned char uchar;

const char *rtosc_argument_string(const char *msg)
{
    assert(msg && *msg);
    while(*++msg); //skip pattern
    while(!*++msg);//skip null
    return msg+1;  //skip comma
}

unsigned rtosc_narguments(const char *msg)
{
    return strlen(rtosc_argument_string(msg));
}

static int has_reserved(char type)
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

static unsigned nreserved(const char *args)
{
    unsigned res = 0;
    for(;*args;++args)
        res += has_reserved(*args);

    return res;
}

char rtosc_type(const char *msg, unsigned pos)
{
    assert(pos < rtosc_narguments(msg));
    return rtosc_argument_string(msg)[pos];
}

//TODO make aligment issues a bit less messy here
static unsigned arg_off(const char *msg, unsigned idx)
{
    if(!has_reserved(rtosc_argument_string(msg)[idx]))
        return 0;

    //Iterate to the right position
    const uchar *args = (const uchar*) rtosc_argument_string(msg);
    const uchar *arg_pos = args;

    while(*++arg_pos);
    //Alignment
    arg_pos += 4-(arg_pos-(uchar*)msg)%4;

    while(idx--) {
        switch(*args++)
        {
            case 'f':
                arg_pos += 4;
                break;
            case 'i':
                arg_pos += 4;
                break;
            case 's':
                while(*++arg_pos);
                arg_pos += 4-(arg_pos-(uchar*)msg)%4;
                break;
            case 'T':
            case 'F':
            case 'I':
                ;
        }
    }
    return arg_pos-(uchar*)msg;
}

size_t rtosc_message(char   *buffer,
                     size_t      len,
                     const char *address,
                     const char *arguments,
                     ...)
{
    va_list va;
    va_start(va, arguments);
    return rtosc_vmessage(buffer, len, address, arguments, va);
}

//Calculate the size of the message without writing to a buffer
static size_t vsosc_null(const char *address,
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
                --toparse;
                break;
            case 'f':
                (void) va_arg(ap, double);
                pos += 4;
                --toparse;
                break;
            default:
                ;
        }
    }

    va_end(ap);

    return pos;
}

size_t rtosc_vmessage(char   *buffer,
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
    }

    va_end(ap);

    return pos;
}

arg_t rtosc_argument(const char *msg, unsigned idx)
{
    //hack to allow reading of arguments from truncated message
    msg-=(msg-(const char *)0)%4;

    arg_t result = {0};
    char type = rtosc_argument_string(msg)[idx];
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
            case 's':
                result.s = (char *)arg_pos;
                break;
            case 'f':
                result.f = *(float*)arg_pos;
                break;
        }
    }

    return result;
}

//TODO Add further error detection
static unsigned char deref(unsigned pos, ring_t *ring)
{
    return pos<ring[0].len?ring[0].data[pos]:ring[1].data[pos-ring[0].len];
}

//Zero means no full message present
size_t rtosc_message_ring_length(ring_t *ring)
{
    unsigned pos = 0;
    while(deref(pos++,ring));
    pos--;
    pos += 4-pos%4;//get 32 bit alignment
    int arguments = pos+1;
    while(deref(++pos,ring));
    pos += 4-pos%4;

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
        int i;
        switch(arg) {
            case 'i':
                pos += 4;
                --toparse;
                break;
            case 's':
                while(deref(++pos,ring));
                pos += 4-pos%4;
                --toparse;
                break;
            case 'b':
                i = 0;
                i |= (deref(pos++,ring) << 24);
                i |= (deref(pos++,ring) << 16);
                i |= (deref(pos++,ring) << 8);
                i |= (deref(pos++,ring));
                pos += i;
                pos += 4-pos%4;
                --toparse;
                break;
            case 'f':
                pos += 4;
                --toparse;
            default:
                ;
        }
    }


    return pos;
}

size_t rtosc_message_length(const char *msg)
{
    ring_t ring[2] = {{(char*)msg,-1},{NULL,0}};
    return rtosc_message_ring_length(ring);
}

size_t rtosc_bundle(char *buffer, size_t len, uint64_t tt, int elms, ...)
{
    char *_buffer = buffer;
    memset(buffer, 0, len);
    strcpy(buffer, "#bundle");
    buffer += 8;
    va_list va;
    va_start(va, elms);
    for(int i=0; i<elms; ++i) {
        const char   *msg  = va_arg(va, const char*);
        size_t        size = rtosc_message_length(msg);
        *(uint32_t*)buffer = size;
        buffer += 4;
        memcpy(buffer, msg, size);
        buffer+=size;
    }

    return 4+buffer-_buffer;
}

size_t rtosc_bundle_elements(const char *buffer)
{
    const int *lengths = (const int*) (buffer+8);
    size_t elms = 0;
    while(*lengths) ++elms, lengths+=*lengths/4+1;
    return elms;
}

const char *rtosc_bundle_fetch(const char *buffer, unsigned elm)
{
    const int *lengths = (const int*) (buffer+8);
    size_t elm_pos = 0;
    while(elm_pos!=elm && *lengths) ++elm_pos, lengths+=*lengths/4+1;

    return (const char*) (elm==elm_pos?lengths+1:NULL);
}

int rtosc_bundle_p(const char *msg)
{
    return !strcmp(msg,"#bundle");
}

