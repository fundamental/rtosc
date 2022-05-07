#include <string.h>
#include "util.h"

char *fast_strcpy(char *dest, const char *src, size_t buffersize)
{
    *dest = 0;
    return strncat(dest, src, buffersize-1);
}
