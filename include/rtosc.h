/** @file rtosc.h */
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
 * When buffer is NULL, the function returns the size of the buffer required to
 * store the message
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * //Example messages
 * char buffer[128];
 * rtosc_message(buffer,128,"/path","TFI");
 * rtosc_message(buffer,128,"/path","s","foobar");
 * rtosc_message(buffer,128,"/path","i",128);
 * rtosc_message(buffer,128,"/path","f",128.0);
 * const char blob[4] = {'a','b','c','d'};
 * rtosc_message(buffer,128,"/path","b",4,blob);
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @param buffer    Memory to write to
 * @param len       Length of buffer
 * @param address   OSC pattern to send message to
 * @param arguments String consisting of the types of the following arguments
 * @param ...       OSC arguments to pass forward
 * @returns length of resulting message or zero if bounds exceeded
 */
size_t rtosc_message(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        ...);

/**
 * @see rtosc_message()
 */
size_t rtosc_vmessage(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        va_list va);

/**
 * @see rtosc_message()
 */
size_t rtosc_amessage(char        *buffer,
                      size_t       len,
                      const char  *address,
                      const char  *arguments,
                      const arg_t *args);

/**
 * Returns the number of arguments found in a given message
 *
 * @param msg well formed OSC message
 * @returns number of arguments in message
 */
unsigned rtosc_narguments(const char *msg);

/**
 * @param msg well formed OSC message
 * @param i   index of argument
 * @returns the type of the ith argument in msg
 */
char rtosc_type(const char *msg, unsigned i);

/**
 * Blob data may be safely written to
 * @param msg OSC message
 * @param i   index of argument
 * @returns an argument by value via the arg_t union
 */
arg_t rtosc_argument(const char *msg, unsigned i);

/**
 * @param msg OSC message
 * @returns the size of a message given a chunk of memory.
 */
size_t rtosc_message_length(const char *msg);

typedef struct {
    char *data;
    size_t len;
} ring_t;

/**
 * Finds the length of the next message inside a ringbuffer structure.
 * 
 * @param ring The addresses and lengths of the split buffer, in a compatible
 *             format to jack's ringbuffer
 * @returns size of message stored in ring datastructure
 */
size_t rtosc_message_ring_length(ring_t *ring);

/**
 * @param OSC message
 * @returns the argument string of a given message
 */
const char *rtosc_argument_string(const char *msg);

/**
 * Generate a bundle from sub-messages
 *
 * @param buffer Destination buffer
 * @param len    Length of buffer
 * @param tt     OSC time tag
 * @param elms   Number of sub messages
 * @param ...    Messages
 * @returns legnth of generated bundle or zero on failure
 */
size_t rtosc_bundle(char *buffer, size_t len, uint64_t tt, int elms, ...);

/**
 * Find the elements in a bundle
 *
 * @param msg OSC bundle
 * @returns The number of messages contained within the bundle
 */
size_t rtosc_bundle_elements(const char *msg);

/**
 * Fetch a message within the bundle
 *
 * @param buffer OSC bundle
 * @param i      index of sub message
 * @returns The ith message within the bundle
 */
const char *rtosc_bundle_fetch(const char *msg, unsigned i);

/**
 * Test if the buffer contains a bundle
 *
 * @param msg OSC message
 * @returns true if message is a bundle
 */
int rtosc_bundle_p(const char *msg);

/**
 * @returns Time Tag for a bundle
 */
uint64_t rtosc_bundle_timetag(const char *msg);

#ifdef __cplusplus
};
#endif
#endif
