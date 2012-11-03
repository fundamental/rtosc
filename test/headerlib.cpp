#include <rtosc.h>
#include <thread-link.h>
#include <string>
#include <iostream>

std::string resultA;
int resultB = 0;
Ports<3,void*> ports{{{
    Port<void*>("setstring:s", "", [](msg_t msg,void*) {resultA = argument(msg,0).s;}),
    Port<void*>("setint:i",    "", [](msg_t msg,void*) {resultB = argument(msg,0).i;}),
    Port<void*>("echo:ss",     "", [](msg_t,void*) {})
}}};

ThreadLink<2048,100> tlink;

int main()
{
    for(int j=0; j<100; ++j) {
        for(int i=0; i<100; ++i){
            tlink.write("badvalue",  "");
            tlink.write("setstring", "s", "testing");
            tlink.write("setint",    "i", 123);
            tlink.write("setint",    "s", "dog");
            tlink.write("echo",      "ss", "hello", "rtosc");
        }

        while(tlink.hasNext())
            ports.dispatch(tlink.read(),NULL);
    }
    tlink.write("echo", "ss", "hello", "rtosc");

    std::cout << OSC_Message(tlink.read()) << std::endl;

    return !(resultB==123 && resultA=="testing");
}
