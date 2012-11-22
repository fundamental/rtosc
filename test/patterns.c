#include <stdlib.h>
#include <rtosc/rtosc.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

int match(const char *pattern, const char *msg)
{
    const char *_msg = msg;
    bool path_flag      = false;

normal:; //Match character by character or hop to speical cases

    //Check for special characters
    if(*pattern == ':') {
        ++pattern;
        goto args;
    }

    if(*pattern == '#') {
        ++pattern;
        goto number;
    }

    if(*pattern == '/' && *msg == '/') {
        path_flag  = 1;
        ++pattern;
        if(*pattern == ':') {
            ++pattern;
            goto args;
        }
        else
            return 3;
    }

    //Verify they are still the same and return if both are fully parsed
    if((*pattern == *msg)) {
        if(*msg)
            ++pattern, ++msg;
        else
            return (path_flag<<1)|1;
        goto normal;
    } else
        return false;

number:; //Match the number

    //Verify both hold digits
    if(!isdigit(*pattern) || !isdigit(*msg))
        return false;

    //Read in both numeric values
    const unsigned max = atoi(pattern);
    const unsigned val = atoi(msg);

    //Match iff msg number is strictly less than pattern
    if(val < max) {

        //Advance pointers
        while(isdigit(*pattern))++pattern;
        while(isdigit(*msg))++msg;

        goto normal;
    } else
        return false;

args:; //Match the arg string or fail

    const char *arg_str = rtosc_argument_string(_msg);

    bool arg_match = *pattern || *pattern == *arg_str;
    while(*pattern) {
        if(*pattern==':') {
            if(arg_match && !*arg_str)
                return (path_flag<<1)|1;
            else {
                ++pattern;
                goto args; //retry
            }
        }
        arg_match &= (*pattern++==*arg_str++);
    }

    if(arg_match)
        return (path_flag<<1)|1;
    return false;
}

const char paths[9][32] = {
    "/",
    "#24:",
    "#20:ff",
    "path",
    "path#234/:ff",
    "path#1asdf",
    "foobar#123/::ff",
    "blam/"
};

int error = 0;

#define work(col, val, name, ...) rtosc_message(buffer, 256, name, __VA_ARGS__); \
    for(int i=0; i<8; ++i) {\
        int matches = match(paths[i], buffer); \
        if(((col == i) && (matches != val))) {\
            printf("Failure to match '%s' to '%s'\n", name, paths[i]);\
            error = 1; \
        } else if((col != i) && matches) { \
            printf("False positive match on '%s' to '%s'\n", name, paths[i]); \
            error = 1; \
        } \
    }

int main()
{
    char buffer[256];
    work(0,3,"/",          "");
    work(1,1,"19",         "");
    work(2,1,"14",         "ff", 1.0,2.0);
    work(3,1,"path",       "");
    work(4,3,"path123/",   "ff", 1.0, 2.0);
    work(5,1,"path0asdf",  "");
    work(6,3,"foobar23/",  "");
    work(6,0,"foobar123/", "");
    work(6,3,"foobar122/", "");
    work(7,3,"blam/",      "");
    work(7,3,"blam/blam",  "");
    work(7,0,"blam",       "");

    return error;
}
