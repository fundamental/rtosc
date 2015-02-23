cmake_minimum_required(VERSION 2.8)
project(RTOSC)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)

SET(BUILD_RTOSC_EXAMPLES FALSE CACHE BOOL
    "Build RTOSC Example Programs")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    include(GNUInstallDirs)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT WIN32)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -O2 -std=c99   -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -std=c++11 -Wall -Wextra -Wno-unused-parameter")
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
    src/cpp/thread-link.cpp
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
    set(GUI_LIBRARIES ${NTK_LDFLAGS})
    include_directories(${NTK_INCLUDE_DIRS})
elseif(FLTK_FOUND AND (FLTK_VERSION STREQUAL "1.3"))
    set(GUI_FOUND TRUE)
    set(GUI_LIBRARIES ${FLTK_LIBRARIES})
else()
    set(GUI_FOUND FALSE)
endif()


if(JACK_FOUND AND GUI_FOUND AND COMPILER_USABILITY AND BUILD_RTOSC_EXAMPLES)
    add_executable(simple-example example/simple/ui.cpp example/simple/synth.cpp)
    target_link_libraries(simple-example m rtosc ${GUI_LIBRARIES}
        ${JACK_LDFLAGS})

    add_executable(complex-example example/complex/audio.cpp
        example/complex/synth.cpp example/complex/window.cpp
        example/complex/Fl_Osc_Dial.cpp example/complex/Fl_Osc_Slider.cpp
        example/complex/Fl_Osc_Button.cpp)
    target_link_libraries(complex-example m rtosc rtosc-cpp ${GUI_LIBRARIES}
        ${JACK_LDFLAGS})
    #TODO add complex example (ie klatter (an unplublished project for those reading))
endif()

if(COMPILER_USABILITY AND BUILD_RTOSC_EXAMPLES)
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
endif(COMPILER_USABILITY AND BUILD_RTOSC_EXAMPLES)


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
    target_link_libraries(${fname} rtosc-cpp rtosc)
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
    include_directories(${LIBLO_INCLUDE_DIRS})
    maketest(liblo)
    target_link_libraries(liblo ${LIBLO_LDFLAGS})
endif()

if(JACK_FOUND)
    include_directories(${JACK_INCLUDE_DIRS})
endif()

maketestcpp(metadata)
maketestcpp(headerlib)
maketestcpp(test-walker)
maketestcpp(performance)

if(COMPILER_USABILITY)
    maketestcpp(undo-test)
    maketestcpp(serializer)
    maketestcpp(sugar)
    maketestcpp(typed-template-test)
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
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
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
        include/rtosc/typed-message.h
        DESTINATION include/rtosc)
    install(TARGETS rtosc rtosc-cpp
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()


#Describe overall configuration
message(STATUS)
message(STATUS "Overall configuration")
message(STATUS "=====================")
message(STATUS)

if(PKG_CONFIG_FOUND)
    message(STATUS "PkgConfig enabled -- package found")
else()
    message(STATUS "PkgConfig disabled -- package NOT found")
endif()
if(LIBLO_FOUND)
    message(STATUS "Liblo enabled -- package found")
else()
    message(STATUS "Liblo disabled -- package NOT found")
endif()
if(JACK_FOUND)
    message(STATUS "JACK enabled -- package found")
else()
    message(STATUS "JACK disabled -- package NOT found")
endif()
if(NTK_FOUND)
    message(STATUS "NTK enabled -- package found")
else()
    message(STATUS "NTK disabled -- package NOT found")
endif()

if(FLTK_FOUND)
    if(FLTK_VERSION STREQUAL "1.3")
        message(STATUS "FLTK enabled -- package found")
    else()
        message(STATUS "FLTK disabled -- package found in unsupported version (expected 1.3)")
    endif()
else()
    message(STATUS "FLTK disabled -- package NOT found")
endif()
