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
execute_process(COMMAND pkg-config jack --exists
    RESULT_VARIABLE JACK_MISSING)
set(JACK_FOUND NOT JACK_MISSING)

if(FLTK_FOUND)
    execute_process(COMMAND fltk-config --api-version
        OUTPUT_VARIABLE FLTK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif(FLTK_FOUND)

if(JACK_FOUND AND FLTK_FOUND AND (FLTK_VERSION STREQUAL "1.3"))
    add_executable(simple-example example/simple/ui.cpp example/simple/synth.cpp)
    target_link_libraries(simple-example m rtosc ${FLTK_LIBRARIES} jack)

    add_executable(complex-example example/complex/audio.cpp
        example/complex/synth.cpp example/complex/window.cpp)
    target_link_libraries(complex-example m rtosc ${FLTK_LIBRARIES} jack)
    #TODO add complex example (ie klatter (an unplublished project for those reading))
endif()

# Testing code
enable_testing()
macro(maketest fname)
    add_executable(${fname} test/${fname}.c)
    add_test(${fname} ${fname})
    target_link_libraries(${fname} rtosc gcov)
endmacro(maketest)

maketest(simple-messages)
maketest(null-messages)
maketest(bundles)
maketest(patterns)

if(JACK_FOUND)
    add_executable(headerlib test/headerlib.cpp)
    add_test(headerlib headerlib)
    target_link_libraries(headerlib rtosc jack gcov)
endif()

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


#Installation
install(FILES include/rtosc/rtosc.h include/rtosc/thread-link.h
    DESTINATION include/rtosc)
install(TARGETS rtosc
    DESTINATION lib)


#Describe overall configuration
if(JACK_FOUND)
    message("-- Found JACK...")
else()
    message("-- Did NOT find JACK...")
endif()

if(FLTK_FOUND)
    if(FLTK_VERSION STREQUAL "1.3")
        message("-- Found FLTK 1.3...")
    else()
        message("-- Found unexpected FLTK version...")
    endif()
else()
    message("-- Did NOT find fltk...")
endif()
