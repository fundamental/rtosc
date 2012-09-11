#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rtosc.h>

char buffer[256];

void check(int b, const char *msg)
{
    if(!b) {
        fprintf(stderr, "%s\n", msg);
        exit(1);
    }
}

int main()
{
    //Check the creation of a simple no arguments message
    check(sosc(buffer, 256, "/page/poge", "TIF") == 20,
            "Incorrect message length");

    check(!memcmp(buffer, "/pag""e/po""ge\0\0"",TIF""\0\0\0", 20),
            "Incorrect message contents");

    //Verify that it can be read properly
    check(nargs(buffer) == 3,
            "Incorrect number of arguments");

    check(type(buffer, 0) == 'T',
            "Incorrect truth argument");

    check(type(buffer, 1) == 'I',
            "Incorrect infinity argument");

    check(type(buffer, 2) == 'F',
            "Incorrect false argument");

    //Check the creation of a more complex message
    check(sosc(buffer, 256, "/testing", "is", 23, "this string") == 32,
            "Incorrect message length");

    check(!memcmp(buffer,"/tes""ting""\0\0\0\0"",is\0""\0\0\0\x17""this"" str""ing", 32),
            "Invalid OSC message");
    
    return 0;
}
