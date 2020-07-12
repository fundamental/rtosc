cmake_minimum_required(VERSION 3.3)
project(RTOSC)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_PATCH 0)

#Find packages

find_package(Doxygen)
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(FLTK)
mark_as_advanced(FLTK_CONFIG_SCRIPT FLTK_DIR FLTK_FLUID_EXECUTABLE
                 FLTK_INCLUDE_DIR FLTK_MATH_LIBRARY)
if(FLTK_FOUND)
    execute_process(COMMAND fltk-config --api-version
        OUTPUT_VARIABLE FLTK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if((FLTK_VERSION STREQUAL "1.3"))
        set(GUI_FOUND TRUE)
    else()
        set(GUI_FOUND FALSE)
    endif()
else()
    set(GUI_FOUND FALSE)
endif(FLTK_FOUND)

if(NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Windows"))
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(JACK jack)
        pkg_check_modules(LIBLO liblo>=0.28)
    endif()
endif()

find_package(Ruby 1.8)
find_program(RTOSC_IWYU_PATH NAMES include-what-you-use iwyu)

#User Configuration

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()
option(RTOSC_INCLUDE_WHAT_YOU_USE "Check for useless includes" OFF)
mark_as_advanced(FORCE RTOSC_INCLUDE_WHAT_YOU_USE)

set(BUILD_RTOSC_EXAMPLES FALSE CACHE BOOL
    "Build RTOSC Example Programs")


if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    include(GNUInstallDirs)
endif()

set(POSITION_INDEPENDENT_CODE ON)

#Ignore the type narrowing conversion warnings
set(MSVC_CNV_WARN "/wd4244 /wd4267 /wd4305")
#Ignore the whole non-portable string*_s function recommendations 
set(MSVC_SAFE_WARN "/wd4996")

if(MSVC)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${CNV_WARN} ${SAFE_WARN}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CNV_WARN} ${SAFE_WARN}")
else()
    set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
endif(MSVC)

if(RTOSC_INCLUDE_WHAT_YOU_USE)
    if(NOT RTOSC_IWYU_PATH)
        set(IWYU_ERR "package NOT found")
    endif()
else()
    set(IWYU_ERR "disabled (RTOSC_INCLUDE_WHAT_YOU_USE=OFF)")
endif()

add_library(rtosc STATIC SHARED
    src/rtosc.c
    src/dispatch.c
    src/pretty-format.c
    src/util.c)

add_library(rtosc-cpp STATIC SHARED
    src/cpp/ports.cpp
    src/cpp/ports-runtime.cpp
    src/cpp/default-value.cpp
    src/cpp/savefile.cpp
    src/cpp/miditable.cpp
    src/cpp/automations.cpp
    src/cpp/midimapper.cpp
    src/cpp/thread-link.cpp
    src/cpp/undo-history.cpp
    src/cpp/subtree-serialize.cpp)
target_link_libraries(rtosc-cpp   PUBLIC rtosc)
target_compile_features(rtosc-cpp PUBLIC cxx_std_11)
target_include_directories(rtosc  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(rtosc      PRIVATE
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c99>")

if(IWYU_ERR)
    message (STATUS "Include what you use: ${IWYU_ERR}")
else()
    set(RTOSC_IWYU_PATH_AND_OPTIONS
        ${RTOSC_IWYU_PATH}
        -Xiwyu
        --no_comments)
    set_property(TARGET rtosc
        PROPERTY C_INCLUDE_WHAT_YOU_USE
                 ${RTOSC_IWYU_PATH_AND_OPTIONS})
    set_property(TARGET rtosc-cpp
        PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                 ${RTOSC_IWYU_PATH_AND_OPTIONS})
endif()


# Example Code


if(JACK_FOUND AND GUI_FOUND AND BUILD_RTOSC_EXAMPLES)
    add_executable(simple-example
        example/simple/ui.cpp example/simple/synth.cpp)
    target_link_libraries(simple-example PRIVATE
        m rtosc ${FLTK_LIBRARIES} ${JACK_LDFLAGS})

    add_executable(complex-example example/complex/audio.cpp
        example/complex/synth.cpp example/complex/window.cpp
        example/complex/Fl_Osc_Dial.cpp example/complex/Fl_Osc_Slider.cpp
        example/complex/Fl_Osc_Button.cpp)
    target_link_libraries(complex-example PRIVATE
        m rtosc-cpp ${FLTK_LIBRARIES} ${JACK_LDFLAGS})
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
        example/modular/util.cpp)
    target_link_libraries(modular-example m rtosc-cpp)
    target_include_directories(modular-example PRIVATE ${JACK_INCLUDE_DIRS})
endif(BUILD_RTOSC_EXAMPLES)


# Testing code
enable_testing()
macro(maketest fname)
    add_executable(${fname} test/${fname}.c)
    add_test(${fname} ${fname})
    target_link_libraries(${fname} PRIVATE rtosc)
endmacro(maketest)

macro(maketestcpp fname)
    add_executable(${fname} test/${fname}.cpp)
    add_test(${fname} ${fname})
    target_link_libraries(${fname} PRIVATE rtosc-cpp)
endmacro(maketestcpp)

maketest(util)
maketest(osc-spec)
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
    maketest(liblo)
    target_include_directories(liblo PRIVATE ${LIBLO_INCLUDE_DIRS})
    target_link_libraries(liblo      PRIVATE ${LIBLO_LDFLAGS})
endif()

maketestcpp(test-midi-mapper)
maketestcpp(metadata)
maketestcpp(headerlib)
maketestcpp(test-walker)
maketestcpp(serializer)
maketestcpp(undo-test)
maketestcpp(path-collapse)

maketestcpp(performance)
if(LIBLO_FOUND)
    target_link_libraries(performance      PRIVATE ${LIBLO_LDFLAGS})
    target_compile_definitions(performance PRIVATE HAVE_LIBLO)
endif()

if(NOT MSVC)
    maketest(pretty-format)
    maketestcpp(default-value)
    maketestcpp(sugar)
    maketestcpp(typed-template-test)
    maketestcpp(test-automation)
endif()

if(LIBLO_FOUND AND RUBY_FOUND)
    add_executable(port-checker test/port-checker.cpp test/port-checker-main.cpp)
    target_link_libraries(port-checker PRIVATE rtosc-cpp ${LIBLO_LDFLAGS})
    add_executable(port-checker-tester test/port-checker.cpp test/port-checker-tester.cpp)
    target_link_libraries(port-checker-tester PRIVATE rtosc-cpp ${LIBLO_LDFLAGS})
    add_executable(port-checker-testapp test/port-checker-testapp.cpp)
    target_link_libraries(port-checker-testapp PRIVATE rtosc-cpp ${LIBLO_LDFLAGS})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test/test-port-checker.rb
                   ${CMAKE_CURRENT_BINARY_DIR}/test-port-checker.rb COPYONLY)
    add_test(test-port-checker test-port-checker.rb)
endif()

# Documentation
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

    install(FILES
        include/rtosc/rtosc.h
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
include("cmake/ColorMessage.cmake")
message(STATUS)
message(STATUS "${ColorBold}Rtosc Build Configuration")
message(STATUS             "=========================${ColorReset}")
message(STATUS)

package_status(PKG_CONFIG_FOUND     "PkgConfig  " "found"   ${Yellow})
package_status(LIBLO_FOUND          "Liblo      " "enabled" ${Yellow})
package_status(JACK_FOUND           "jack       " "enabled" ${Yellow})
package_status(GUI_FOUND            "FLTK       " "enabled" ${Yellow})
package_status(BUILD_RTOSC_EXAMPLES "Examples   " "enabled" ${Yellow})

message(STATUS "Build C   flags: ${CMAKE_C_FLAGS}")
message(STATUS "Build C++ flags: ${CMAKE_CXX_FLAGS}")
