/*
 * Copyright (c) 2012 Mark McCurry
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

#ifndef RTOSC_MATCH
#define RTOSC_MATCH

#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

static bool rtosc_match_number(const char **pattern, const char **msg)
{
    //Verify both hold digits
    if(!isdigit(**pattern) || !isdigit(**msg))
        return false;

    //Read in both numeric values
    unsigned max = atoi(*pattern);
    unsigned val = atoi(*msg);

    ////Advance pointers
    while(isdigit(**pattern))++*pattern;
    while(isdigit(**msg))++*msg;

    //Match iff msg number is strictly less than pattern
    return val < max;
}
static const char *rtosc_match_path(const char *pattern, const char *msg)
{
    while(1) {
        //Check for special characters
        if(*pattern == ':' && !*msg)
            return pattern;
        else if(*pattern == '/' && *msg == '/')
            return ++pattern;
        else if(*pattern == '#') {
            ++pattern;
            if(!rtosc_match_number(&pattern, &msg))
                return NULL;
        } else if((*pattern == *msg)) { //verbatim compare
            if(*msg)
                ++pattern, ++msg;
            else
                return pattern;
        } else
            return NULL;
    }
}

//Match the arg string or fail
static bool rtosc_match_args(const char *pattern, const char *msg)
{
    //match anything if now arg restriction is present (ie the ':')
    if(*pattern++ != ':')
        return true;

    const char *arg_str = rtosc_argument_string(msg);
    bool      arg_match = *pattern || *pattern == *arg_str;

    while(*pattern && *pattern != ':')
        arg_match &= (*pattern++==*arg_str++);

    if(*pattern==':') {
        if(arg_match && !*arg_str)
            return true;
        else
            return rtosc_match_args(pattern, msg); //retry
    }

    return arg_match;
}

/**
 * This is a non-compliant pattern matcher for dispatching OSC messages
 *
 * Overall the pattern specification is
 *   (normal-path)(\#digit-specifier)?(/)?(:argument-restrictor)*
 *
 * @param pattern The pattern string stored in the Port
 * @param msg     The OSC message to be matched
 * @returns true if a normal match and false if unmatched
 */
static bool rtosc_match(const char *pattern, const char *msg)
{
    const char *arg_pattern = rtosc_match_path(pattern, msg);
    if(!arg_pattern)
        return false;
    else if(*arg_pattern == ':')
        return rtosc_match_args(arg_pattern, msg);
    return true;
}
#endif
