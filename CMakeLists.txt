cmake_minimum_required(VERSION 2.8)
project(RTOSC)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)

include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT WIN32)
    set(CMAKE_C_FLAGS "-O2 -std=c99 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "-O2 -std=c++0x -Wall -Wextra")
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
        OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL
            4.7)
        message(STATUS "GCC Version >= 4.7")
        set(COMPILER_USABILITY TRUE)
    else()
        message(STATUS "GCC Version Is Too Old For rtosc/port-sugar.h")
        message(STATUS "WILL NOT Build Examples or sugar test")
        set(COMPILER_USABILITY FALSE)
    endif()
else()
    set(COMPILER_USABILITY TRUE)
endif()

add_library(rtosc SHARED src/rtosc.c src/dispatch.c)
add_library(rtosc-cpp SHARED src/cpp/ports.cpp src/cpp/miditable.cpp
    src/cpp/midimapper.cpp
    src/cpp/undo-history.cpp
    src/cpp/subtree-serialize.cpp)

# Example Code
find_package(FLTK)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(NTK ntk)
    pkg_check_modules(JACK jack)
    pkg_check_modules(LIBLO liblo)
endif()

if(FLTK_FOUND)
    execute_process(COMMAND fltk-config --api-version
        OUTPUT_VARIABLE FLTK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif(FLTK_FOUND)

if(NTK_FOUND)
    #Default to NTK, as everything looks better there
    set(GUI_FOUND TRUE)
    set(GUI_LIBRARIES ${NTK_LIBRARIES})
    include_directories(${NTK_INCLUDE_DIRS})
elseif(FLTK_FOUND AND (FLTK_VERSION STREQUAL "1.3"))
    set(GUI_FOUND TRUE)
    set(GUI_LIBRARIES ${FLTK_LIBRARIES})
else()
    set(GUI_FOUND FALSE)
endif()


if(JACK_FOUND AND GUI_FOUND AND COMPILER_USABILITY)
    add_executable(simple-example example/simple/ui.cpp example/simple/synth.cpp)
    target_link_libraries(simple-example m rtosc ${GUI_LIBRARIES} jack)

    add_executable(complex-example example/complex/audio.cpp
        example/complex/synth.cpp example/complex/window.cpp
        example/complex/Fl_Osc_Dial.cpp example/complex/Fl_Osc_Slider.cpp
        example/complex/Fl_Osc_Button.cpp)
    target_link_libraries(complex-example m rtosc rtosc-cpp ${GUI_LIBRARIES}
        jack)
    #TODO add complex example (ie klatter (an unplublished project for those reading))
endif()

if(COMPILER_USABILITY)
    add_executable(modular-example
        example/modular/Echo.cpp
        example/modular/EffectMgr.cpp
        example/modular/LFO.cpp
        example/modular/main.cpp
        example/modular/Oscil.cpp
        example/modular/Synth.cpp
        example/modular/util.cpp
        )
    target_link_libraries(modular-example m rtosc rtosc-cpp)
endif(COMPILER_USABILITY)


# Testing code
enable_testing()
macro(maketest fname)
    add_executable(${fname} test/${fname}.c)
    add_test(${fname} ${fname})
    target_link_libraries(${fname} rtosc)
endmacro(maketest)

macro(maketestcpp fname)
    add_executable(${fname} test/${fname}.cpp)
    add_test(${fname} ${fname})
    target_link_libraries(${fname} rtosc-cpp rtosc jack)
endmacro(maketestcpp)

maketest(osc-spec)
maketest(simple-messages)
maketest(null-messages)
maketest(bundles)
maketest(patterns)
maketest(nested-bundles)
maketest(fat-message)
maketest(empty-strings)
maketest(message-alignment)
if(LIBLO_FOUND)
    maketest(liblo)
    target_link_libraries(liblo lo)
endif()

if(JACK_FOUND)
    maketestcpp(metadata)
    maketestcpp(headerlib)
    maketestcpp(test-walker)
endif()
maketestcpp(performance)

if(COMPILER_USABILITY)
    maketestcpp(undo-test)
    maketestcpp(serializer)
    maketestcpp(sugar)
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
if(PKG_CONFIG_FOUND)
    configure_file(librtosc.pc.cmake
        ${CMAKE_SOURCE_DIR}/librtosc.pc @ONLY)
    install(FILES ${CMAKE_SOURCE_DIR}/librtosc.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
endif()
install(FILES include/rtosc/rtosc.h
    include/rtosc/thread-link.h
    include/rtosc/ports.h
    include/rtosc/miditable.h
    include/rtosc/port-sugar.h
    include/rtosc/undo-history.h
    include/rtosc/subtree-serialize.h
    DESTINATION include/rtosc)
install(TARGETS rtosc rtosc-cpp
    DESTINATION ${CMAKE_INSTALL_LIBDIR})


#Describe overall configuration
if(PKG_CONFIG_FOUND)
    message("-- Found PkgConfig")
else()
    message("-- Did NOT find PkgConfig")
endif()
if(LIBLO_FOUND)
    message("-- Found Liblo...")
else()
    message("-- Did NOT find Liblo...")
endif()
if(JACK_FOUND)
    message("-- Found JACK...")
else()
    message("-- Did NOT find JACK...")
endif()
if(NTK_FOUND)
    message("-- Found NTK...")
else()
    message("-- Did NOT find NTK...")
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
