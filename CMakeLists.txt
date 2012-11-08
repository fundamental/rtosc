cmake_minimum_required(VERSION 2.8)
project(RTOSC)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CMAKE_C_FLAGS "-O2 -std=c99 -Wall -Wextra -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS "-O2 -std=c++0x -Wall -Wextra -fprofile-arcs -ftest-coverage")
set(CMAKE_SHARED_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")
add_library(rtosc SHARED src/rtosc.c)

# Example Code
find_package(FLTK)
add_executable(simple-example example/simple/ui.cpp example/simple/synth.cpp)
target_link_libraries(simple-example m rtosc ${FLTK_LIBRARIES} jack)

add_executable(complex-example example/complex/audio.cpp
    example/complex/synth.cpp example/complex/window.cpp)
target_link_libraries(complex-example m rtosc ${FLTK_LIBRARIES} jack)
#TODO add complex example (ie klatter (an unplublished project for those reading))

# Testing code
enable_testing()

add_executable(simple-messages test/simple-messages.c)
add_test(simple-messages simple-messages)
target_link_libraries(simple-messages rtosc gcov)

add_executable(null-messages test/null-messages.c)
add_test(null-messages null-messages)
target_link_libraries(null-messages rtosc gcov)

add_executable(header-test test/headerlib.cpp)
add_test(header-test header-test)
target_link_libraries(header-test rtosc -ljack gcov)

add_executable(bundles test/bundles.c)
add_test(bundles bundles)
target_link_libraries(bundles rtosc gcov)

# Documentation
find_package(Doxygen)
SET(Documentation FALSE CACHE BOOL "Enable building documentation")

if(DOXYGEN_FOUND AND Documentation)
    set(CMAKE_DOXYGEN_INPUT_LIST ${CMAKE_SOURCE_DIR}/include/)
    SET(DOXYGEN_OUTPUT_DIR html)
    CONFIGURE_FILE(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    SET(HTML_TARGET “html” )
    ADD_CUSTOM_TARGET(${HTML_TARGET} ALL
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif()
