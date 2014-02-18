#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>


class Object
{
    public:
        int foo;
        int bar;
};

#define rObject Object


rtosc::Ports p = {
    rOption(foo, rOpt(0,red) rOpt(1,blue) rOpt(2,green) rOpt(3,teal), "various options"),
    rOption(bar, rOptions(red,blue,green,teal), "various options")
};

int main()
{
    //Assuming this test even compiles that implies some level of sanity to the
    //syntaxtual sugar
    return 0;
}
