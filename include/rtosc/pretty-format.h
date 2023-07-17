/*
 * Copyright (c) 2017 Johannes Lorenz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @file pretty-format.h
 * Header for pretty printer and scanner
 *
 * @test pretty-format.c
 */

#ifndef RTOSC_PRETTY_FORMAT
#define RTOSC_PRETTY_FORMAT

#include <rtosc/rtosc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool lossless; //!< will add hex notation behind floats
    int floating_point_precision;
    const char* sep; //!< separator for multiple argument values
    int linelength;
    int compress_ranges;
} rtosc_print_options;

/**
 * Pretty-print rtosc_arg_val_t structure into buffer.
 * Ranges are not being compressed (use rtosc_print_arg_vals instead).
 *
 * @param arg Pointer to the structure that shall be printed
 * @param buffer The buffer to write to
 * @param buffersize The maximum size to write to, includin a trailing 0 byte
 * @param opt Printer options, NULL for default options
 * @param cols_used How many columns have been used for writing in this line
 *   (will be updated by this function)
 * @param prev_arg_if_range Optional hint in case of ranges: previous arg
 * @return The number of bytes written, excluding the null byte
 */
size_t rtosc_print_arg_val(const rtosc_arg_val_t* arg, char* buffer,
                           size_t buffersize, const rtosc_print_options* opt,
                           int* cols_used, const rtosc_arg_val_t* prev_arg_if_range);

/**
 * Pretty-print rtosc_arg_val_t array into buffer.
 * Ranges are being compressed if opt->compress_ranges is true.
 *
 * @see rtosc_print_message
 * @warning in case of possible line breaks (almost always), buffer[-1] must
 *   be accessible and be whitespace (since it can be converted to a newline)
 */
size_t rtosc_print_arg_vals(const rtosc_arg_val_t *args, size_t n,
                            char *buffer, size_t bs,
                            const rtosc_print_options* opt,
                            int cols_used);

/**
 * Pretty-print OSC message into string buffer
 *
 * Ranges are being compressed if opt->compress_ranges is true.
 * A newline will be appended.
 *
 * @param address OSC pattern to send message to
 * @param args The array to print
 * @param n Number of args from the array that should be printed
 * @param buffer The buffer to write to
 * @param bs The maximum size to write to, includin a trailing 0 byte
 * @param opt Printer options, NULL for default options
 * @param cols_used How many columns have been used for writing in this line
 * @return The number of bytes written, excluding the null byte
 */
size_t rtosc_print_message(const char* address,
                           const rtosc_arg_val_t *args, size_t n,
                           char *buffer, size_t bs,
                           const rtosc_print_options* opt,
                           int cols_used);

/**
 * Skip characters from a string until one argument value
 *   would have been scanned
 * @param src The current position
 * @param skipped The number of arg_vals that would be skipped when scanning,
 *   typically 1 (though not for arrays)
 * @param type The skipped type
 * @param llhssrc The recent reading position before this
 *   (it will get the left of left hand sign of this argument)
 * @param follow_ellipsis Whether an argument followed by an ellipsis is
 *   interpreted as a range start or as only the argument itself
 * @param inside_bundle Whether the current position is inside a bundle
 * @return The first character after that argument value, or NULL if a
 *   parsing error occurred
 */
const char* rtosc_skip_next_printed_arg(const char* src, int* skipped,
                                        char* type, const char* llhssrc,
                                        int follow_ellipsis, int inside_bundle);

/**
 * Count arguments that would be scanned and do a complete syntax check
 *
 * This function should be run before rtosc_scan_arg_vals() in order
 * to know the number of argument values. Also, rtosc_scan_arg_vals() does
 * no complete syntax check.
 *
 * @param src The string to scan from
 * @return The number of arguments that can be scanned (>0), or if the nth arg
 *   (range 1...) can not be scanned, -n. Array arguments and array start each
 *   count as one argument. This function never returns 0
 */
int rtosc_count_printed_arg_vals(const char* src);

/**
 * Count arguments of a message that would be scanned and
 *   do a complete syntax check
 *
 * @param msg The message to scan from
 * @return -1 if the address could not be scanned,
 *   INT_MIN if the whole string is whitespace,
 *   otherwise @see rtosc_count_printed_arg_vals
 */
int rtosc_count_printed_arg_vals_of_msg(const char* msg);

/**
 * Scan one argument value from a string
 *
 * This function does no complete syntaxcheck. Call
 * rtosc_count_printed_arg_vals() before.
 *
 * @param src The string
 * @param arg Pointer to an array where to store the argument value
 * @param n Size of the array @p arg. Since only one arg is being scanned, for
 *   almost all cases (except arrays), n being 1 is sufficient.
 * @param buffer_for_strings A buffer with enough space for scanned
 *   strings and blobs
 * @param bufsize Size of @p buffer_for_strings , will be shrunk to the
 *   bufferbytes left after the scan
 * @param args_before Number of arguments scanned before this one
 * @param follow_ellipsis Whether an argument followed by an ellipsis is
 *   interpreted as a range start or as only the argument itself
 * @return The number of bytes scanned
 */
size_t rtosc_scan_arg_val(const char* src,
                          rtosc_arg_val_t *arg, size_t n,
                          char* buffer_for_strings, size_t* bufsize,
                          size_t args_before, int follow_ellipsis);

/**
 * Scan a fixed number of argument values from a string
 *
 * This function does no complete syntaxcheck. Call
 * rtosc_count_printed_arg_vals() before. This will also give you the @p n
 * parameter.
 *
 * @see rtosc_scan_message
 */
size_t rtosc_scan_arg_vals(const char* src,
                           rtosc_arg_val_t *args, size_t n,
                           char* buffer_for_strings, size_t bufsize);

/**
 * Scan an OSC message from a string
 *
 * This function does no complete syntaxcheck. Call
 * rtosc_count_printed_arg_vals() before. This will also give you the @p n
 * parameter. Preceding and trailing whitespace will be consumed.
 *
 * @param src The string
 * @param address A buffer where the port address will be written
 * @param adrsize Size of buffer @p address
 * @param args Pointer to an array of argument values; the output will be
 *    written here
 * @param n The amount of argument values to scan
 * @param buffer_for_strings A buffer with enough space for scanned
 *   strings and blobs
 * @param bufsize Size of @p buffer_for_strings
 * @return The number of bytes scanned
 */
size_t rtosc_scan_message(const char* src,
                          char* address, size_t adrsize,
                          rtosc_arg_val_t *args, size_t n,
                          char* buffer_for_strings, size_t bufsize);

#ifdef __cplusplus
}
#endif
#endif // RTOSC_PRETTY_FORMAT
