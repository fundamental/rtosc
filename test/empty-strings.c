#include <rtosc/rtosc.h>
#include <stdio.h>
#include <err.h>

char buffer[1024];

//verify that empty strings can be retreived
int main()
{
    size_t length = rtosc_message(buffer, 1024, "/path", "sss", "", "", "");
    // /pat h000 ,sss 0000 0000 0000 0000
    if(length != 28)
        errx(1, "Bad length for empty strings...");
    if(!rtosc_argument(buffer, 0).s ||
            !rtosc_argument(buffer, 1).s ||
            !rtosc_argument(buffer, 2).s)
        errx(1, "Failure to retreive string pointer...");
    printf("%p %p %p\n",
            rtosc_argument(buffer, 0).s,
            rtosc_argument(buffer, 1).s,
            rtosc_argument(buffer, 2).s);
    return 0;
}
