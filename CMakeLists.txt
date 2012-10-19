cmake_minimum_required(VERSION 2.8)
project(RTOSC)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CMAKE_C_FLAGS "-std=c99 -Wall -Wextra")
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
add_library(rtosc SHARED src/rtosc.c)

# Example Code
find_package(FLTK)
add_executable(simple-example example/simple/ui.cpp example/simple/synth.cpp)
target_link_libraries(simple-example m rtosc ${FLTK_LIBRARIES} jack)

add_executable(complex-example example/complex/audio.cpp example/complex/osc.cpp
    example/complex/synth.cpp example/complex/window.cpp)
target_link_libraries(complex-example m rtosc ${FLTK_LIBRARIES} jack)
#TODO add complex example (ie klatter (an unplublished project for those reading))

# Testing code
enable_testing()

add_executable(simple-messages test/simple-messages.c)
add_test(simple-messages simple-messages)
target_link_libraries(simple-messages rtosc)

add_executable(null-messages test/null-messages.c)
add_test(null-messages null-messages)
target_link_libraries(null-messages rtosc)
