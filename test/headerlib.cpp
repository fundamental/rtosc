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

ThreadLink<2048,100> link;

int main()
{
    for(int j=0; j<100; ++j) {
        for(int i=0; i<100; ++i){
            link.write("badvalue",  "");
            link.write("setstring", "s", "testing");
            link.write("setint",    "i", 123);
            link.write("setint",    "s", "dog");
            link.write("echo",      "ss", "hello", "rtosc");
        }

        while(link.hasNext())
            ports.dispatch(link.read(),NULL);
    }
    link.write("echo", "ss", "hello", "rtosc");

    std::cout << OSC_Message(link.read()) << std::endl;

    return !(resultB==123 && resultA=="testing");
}
