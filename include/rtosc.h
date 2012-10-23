
#ifndef RTOSC_H
#define RTOSC_H
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t len;
    uint8_t *data;
} blob_t;

typedef union {
    int32_t i;
    char T;
    float f;
    const char *s;
    blob_t b;
} arg_t;

size_t sosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        ...);

size_t vsosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        va_list va);

unsigned nargs(const char *msg);
char type(const char *msg, unsigned i);

arg_t argument(const char *msg, unsigned i);

//Finds the offset of a given argument or 0 for invalid requests
unsigned arg_off(const char *msg, unsigned i);

size_t msg_len(const char *msg);

typedef struct {
    char *data;
    size_t len;
} ring_t;

size_t msg_len_ring(ring_t *ring);

const char *arg_str(const char *msg);

#ifdef __cplusplus
};
#endif
#endif
