cmake_minimum_required(VERSION 2.8)
project(RTOSC)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_PATCH 0)

option(PERF_TEST "Run Performance Timing Tests" OFF)
option(RTOSC_INCLUDE_WHAT_YOU_USE "Check for useless includes" OFF)
mark_as_advanced(FORCE RTOSC_INCLUDE_WHAT_YOU_USE)

include("cmake/ColorMessage.cmake")
SET(BUILD_RTOSC_EXAMPLES FALSE CACHE BOOL
    "Build RTOSC Example Programs")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    include(GNUInstallDirs)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.c.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CXX11_FLAG "-std=c++11")
set(FPIC_FLAG "-fPIC")
set(OPT_FLAG "-O2")
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(OPT_FLAG "")
endif()
if(WIN32)
    #code is position-independent by default on Windows
    set(FPIC_FLAG "")
endif()
if(NOT WIN32 OR CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS   "${OPT_FLAG} ${CMAKE_C_FLAGS} -fPIC -std=c99   -Wall -Wextra")
    set(CPP_FLAGS       "${OPT_FLAG} ${FPIC_FLAG} -Wall -Wextra -Wno-unused-parameter")
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
        OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL
            4.7)
        message(STATUS "Found GCC Version >= 4.7")
    else()
        message(WARNING "GCC Version '${GCC_VERSION}' Is Quite Old, Please Consider Upgrading")
        set(CXX11_FLAG "-std=c++0x")
    endif()
endif()

if(${CMAKE_VERSION} VERSION_LESS "3.3.0")
    set(IWYU_ERR "disabled (cmake < 3.3.0)")
else()
    if(RTOSC_INCLUDE_WHAT_YOU_USE)
        find_program(RTOSC_IWYU_PATH NAMES include-what-you-use iwyu)
        if(NOT RTOSC_IWYU_PATH)
            set(IWYU_ERR "package NOT found")
        endif()
    else()
        set(IWYU_ERR "disabled (RTOSC_INCLUDE_WHAT_YOU_USE=OFF)")
    endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPP_FLAGS} ${CXX11_FLAG}")

add_library(rtosc STATIC src/rtosc.c src/dispatch.c src/pretty-format.c
            src/arg-val-math.c src/arg-val-cmp.c src/rtosc-time.c src/util.c
            ${CMAKE_CURRENT_BINARY_DIR}/version.c)
add_library(rtosc-cpp STATIC src/cpp/ports.cpp src/cpp/ports-runtime.cpp
    src/cpp/default-value.cpp src/cpp/savefile.cpp
    src/cpp/miditable.cpp
    src/cpp/automations.cpp
    src/cpp/midimapper.cpp
    src/cpp/thread-link.cpp
    src/cpp/undo-history.cpp
    src/cpp/subtree-serialize.cpp)
target_link_libraries(rtosc-cpp rtosc)

if(IWYU_ERR)
    message (STATUS "Include what you use: ${IWYU_ERR}")
else()
    set(RTOSC_IWYU_PATH_AND_OPTIONS
        ${RTOSC_IWYU_PATH}
        -Xiwyu
        --no_comments)
    set_property(TARGET rtosc PROPERTY C_INCLUDE_WHAT_YOU_USE
                 ${RTOSC_IWYU_PATH_AND_OPTIONS})
    set_property(TARGET rtosc-cpp PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                 ${RTOSC_IWYU_PATH_AND_OPTIONS})
endif()


# Example Code
find_package(FLTK)
if(NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Windows"))
    find_package(PkgConfig)
endif()
if(PKG_CONFIG_FOUND)
    pkg_check_modules(NTK ntk)
    pkg_check_modules(JACK jack)
    pkg_check_modules(LIBLO liblo>=0.28)
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


if(JACK_FOUND AND GUI_FOUND AND BUILD_RTOSC_EXAMPLES)
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

if(BUILD_RTOSC_EXAMPLES)
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
endif(BUILD_RTOSC_EXAMPLES)


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

maketest(version)
maketest(util)
maketest(osc-spec)
maketest(arg-val-cmp)
maketest(arg-val-math)
maketest(pretty-format)
maketest(simple-messages)
maketest(null-messages)
maketest(bundles)
maketest(patterns)
maketest(nested-bundles)
maketest(fat-message)
maketest(empty-strings)
maketest(message-alignment)
maketest(test-arg-iter)
if(LIBLO_FOUND)
    add_definitions(-DHAVE_LIBLO)
    include_directories(${LIBLO_INCLUDE_DIRS})
    maketest(liblo)
    target_link_libraries(liblo ${LIBLO_LDFLAGS})
endif()

if(JACK_FOUND)
    include_directories(${JACK_INCLUDE_DIRS})
endif()

maketestcpp(test-midi-mapper)
maketestcpp(metadata)
maketestcpp(default-value)
maketestcpp(headerlib)
maketestcpp(test-walker)

if(PERF_TEST)
    maketestcpp(performance)
    if(LIBLO_FOUND)
        target_link_libraries(performance ${LIBLO_LDFLAGS})
    endif()
    maketestcpp(serializer)
endif(PERF_TEST)

maketestcpp(undo-test)
maketestcpp(sugar)
if(CXX11_FLAG STREQUAL "-std=c++11")
    maketestcpp(typed-template-test)
endif()

maketestcpp(path-collapse)

maketestcpp(test-automation)

find_package(Ruby 1.8)
if(LIBLO_FOUND AND RUBY_FOUND)
    add_executable(port-checker test/port-checker.cpp test/port-checker-main.cpp)
    target_link_libraries(port-checker rtosc-cpp rtosc ${LIBLO_LDFLAGS})
    add_executable(port-checker-tester test/port-checker.cpp test/port-checker-tester.cpp)
    target_link_libraries(port-checker-tester rtosc-cpp rtosc ${LIBLO_LDFLAGS})
    add_executable(port-checker-testapp test/port-checker-testapp.cpp)
    target_link_libraries(port-checker-testapp rtosc-cpp rtosc ${LIBLO_LDFLAGS})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test/test-port-checker.rb
                   ${CMAKE_CURRENT_BINARY_DIR}/test-port-checker.rb COPYONLY)
    add_test(test-port-checker test-port-checker.rb)
endif()

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # configuration variables
    set(CMAKE_DOXYGEN_INPUT_LIST ${CMAKE_CURRENT_SOURCE_DIR}/include/rtosc/)
    set(DOXYGEN_OUTPUT_DIR html)
    
    configure_file(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    add_custom_command(OUTPUT ${DOXYGEN_OUTPUT_DIR}
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        DEPENDS rtosc-cpp ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM)
    add_custom_target(rtosc-doc DEPENDS html)
endif()


#Installation
if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT DEFINED RTOSC_NO_INSTALL)
    if(PKG_CONFIG_FOUND)
        configure_file(librtosc.pc.cmake
            ${CMAKE_BINARY_DIR}/librtosc.pc @ONLY)
        install(FILES ${CMAKE_BINARY_DIR}/librtosc.pc
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
        include/rtosc/rtosc-version.h
        DESTINATION include/rtosc)
    install(TARGETS rtosc rtosc-cpp
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()


#Describe overall configuration
message(STATUS)
message(STATUS "${ColorBold}Rtosc Build Configuration")
message(STATUS             "=========================${ColorReset}")
message(STATUS)

if(PKG_CONFIG_FOUND)
    message(STATUS "PkgConfig enabled  -- ${Green}package found${ColorReset}")
else()
    message(STATUS "PkgConfig disabled -- ${Yellow}package NOT found${ColorReset}")
endif()
if(LIBLO_FOUND)
    message(STATUS "Liblo enabled      -- ${Green}package found${ColorReset}")
else()
    message(STATUS "Liblo disabled     -- ${Yellow}package NOT found${ColorReset}")
endif()
if(JACK_FOUND)
    message(STATUS "JACK enabled       -- ${Green}package found${ColorReset}")
else()
    message(STATUS "JACK disabled      -- ${Yellow}package NOT found${ColorReset}")
endif()
if(NTK_FOUND)
    message(STATUS "NTK enabled        -- ${Green}package found${ColorReset}")
else()
    message(STATUS "NTK disabled       -- ${Yellow}package NOT found${ColorReset}")
endif()

if(PERF_TEST)
    message(STATUS "Perf Test(s) enabled")
else()
    message(STATUS "Perf Test(s) disabled")
endif()

if(FLTK_FOUND)
    if(FLTK_VERSION STREQUAL "1.3")
        message(STATUS "FLTK enabled       -- ${Green}package found${ColorReset}")
    else()
        message(STATUS "FLTK disabled      -- ${Yellow}package found in unsupported version (expected 1.3)${ColorReset}")
    endif()
else()
    message(STATUS "FLTK disabled      -- ${Yellow}package NOT found${ColorReset}")
endif()
