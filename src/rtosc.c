#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

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
        case 'c':
            return 1;
        case 'T':
        case 'F':
        case 'N':
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
    const uint8_t *args = (const uint8_t*) rtosc_argument_string(msg);
    const uint8_t *arg_pos = args;

    while(*++arg_pos);
    //Alignment
    arg_pos += 4-(arg_pos-(uint8_t*)msg)%4;

    while(idx--) {
        switch(*args++)
        {
            case 'f':
            case 'c':
            case 'i':
                arg_pos += 4;
                break;
            case 's':
                while(*++arg_pos);
                arg_pos += 4-(arg_pos-(uint8_t*)msg)%4;
                break;
            case 'T':
            case 'F':
            case 'I':
                ;
        }
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
    return rtosc_vmessage(buffer, len, address, arguments, va);
}

//Calculate the size of the message without writing to a buffer
static size_t vsosc_null(const char  *address,
                         const char  *arguments,
                         const arg_t *args)
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
            case 'c':
            case 'f':
            case 'i':
                ++arg_pos;
                pos += 4;
                --toparse;
                break;
            case 's':
                s = args[arg_pos++].s;
                pos += strlen(s);
                pos += 4-pos%4;
                --toparse;
                break;
            case 'b':
                i = args[arg_pos++].b.len;
                pos += 4 + i;
                --toparse;
                break;
            default:
                ;
        }
    }

    return pos;
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

    arg_t args[nargs];

    unsigned arg_pos = 0;
    const char *arg_str = arguments;
    while(arg_pos < nargs)
    {
        switch(*arg_str++) {
            case 'c':
            case 'i':
                args[arg_pos++].i = va_arg(ap, int);
                break;
            case 's':
                args[arg_pos++].s = va_arg(ap, const char *);
                break;
            case 'b':
                args[arg_pos].b.len = va_arg(ap, int);
                args[arg_pos].b.data = va_arg(ap, unsigned char *);
                arg_pos++;
                break;
            case 'f':
                args[arg_pos++].f = va_arg(ap, double);
                break;
            default:
                ;
        }
    }

    return rtosc_amessage(buffer,len,address,arguments,args);
}

size_t rtosc_amessage(char        *buffer,
                      size_t       len,
                      const char  *address,
                      const char  *arguments,
                      const arg_t *args)
{
    const size_t total_len = vsosc_null(address, arguments, args);

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
        const char *s;
        const unsigned char *u;
        blob_t b;
        switch(arg) {
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
                pos += 4-pos%4;
                --toparse;
                break;
            default:
                ;
        }
    }

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
            case 'f':
            case 'c':
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
        }
    }

    return result;
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
        advance = deref(pos+0, ring) << (8*0) |
                  deref(pos+1, ring) << (8*1) |
                  deref(pos+2, ring) << (8*2) |
                  deref(pos+3, ring) << (8*3);
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
            case 'c':
            case 'f':
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

size_t rtosc_bundle(char *buffer, size_t len, uint64_t tt, int elms, ...)
{
    char *_buffer = buffer;
    memset(buffer, 0, len);
    strcpy(buffer, "#bundle");
    buffer += 8;
    (*(uint64_t*)buffer) = tt;
    buffer +=8;
    va_list va;
    va_start(va, elms);
    for(int i=0; i<elms; ++i) {
        const char   *msg  = va_arg(va, const char*);
        //It is assumed that any passed message/bundle is valid
        size_t        size = rtosc_message_length(msg, -1);
        *(uint32_t*)buffer = size;
        buffer += 4;
        memcpy(buffer, msg, size);
        buffer+=size;
    }

    return buffer-_buffer;
}

#define POS ((size_t)(((const char *)lengths) - buffer))
size_t rtosc_bundle_elements(const char *buffer, size_t len)
{
    const uint32_t *lengths = (const uint32_t*) (buffer+16);
    size_t elms = 0;
    //TODO
    while(POS < len && *lengths) {
        lengths += *lengths/4+1;

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
    while(elm_pos!=elm && *lengths) ++elm_pos, lengths+=*lengths/4+1;

    return (const char*) (elm==elm_pos?lengths+1:NULL);
}

int rtosc_bundle_p(const char *msg)
{
    return !strcmp(msg,"#bundle");
}

uint64_t rtosc_bundle_timetag(const char *msg)
{
    return *(uint64_t*)(msg+8);
}
