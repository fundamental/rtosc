#include <lo/lo.h>
#include <rtosc/rtosc.h>
#include <string.h>

char buffer[128];

int good = 1;
int main()
{
    //clean buffer
    memset(buffer, 0, sizeof(buffer));


    //generate liblo message
    size_t len = 128;
    lo_message message = lo_message_new();
    lo_message_add_float(message, 24.0);
    lo_message_add_int32(message, 42);
    lo_message_serialise(message, "/path", buffer, &len);

    good &= !strcmp("/path", buffer);
    good &= 24.0 == rtosc_argument(buffer, 0).f;
    good &= 42   == rtosc_argument(buffer, 1).i;
    good &= len  == rtosc_message_length(buffer, 128);

    return !good;
}
