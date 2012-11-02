
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
    double f;
    const char *s;
    blob_t b;
} arg_t;

/**
 * Write OSC message to fixed length buffer
 *
 * On error, buffer will be zeroed.
 *
 * @param buffer    Memory to write to
 * @param len       Length of buffer
 * @param address   OSC pattern to send message to
 * @param arguments String consisting of the types of the following arguments
 * @param ...       OSC arguments to pass forward
 * @returns length of resulting message or zero if bounds exceeded
 */
size_t sosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        ...);

/**
 * See sosc()
 */
size_t vsosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        va_list va);

/**
 * Returns the number of arguments found in a given message
 */
unsigned nargs(const char *msg);

/**
 * Returns the type of the ith argument in msg
 */
char type(const char *msg, unsigned i);

arg_t argument(const char *msg, unsigned i);

//Finds the offset of a given argument or 0 for invalid requests
unsigned arg_off(const char *msg, unsigned i);

/**
 * Returns the size of a message given a chunk of memory.
 */
size_t msg_len(const char *msg);

typedef struct {
    char *data;
    size_t len;
} ring_t;

/**
 * Finds the length of the next message inside a ringbuffer structure.
 * 
 * @param ring The addresses and lengths of the split buffer, in a compatible
 *             format to jack's ringbuffer
 */
size_t msg_len_ring(ring_t *ring);

/**
 * Returns the argument string of a given message
 */
const char *arg_str(const char *msg);

#ifdef __cplusplus
};
#endif
#endif
