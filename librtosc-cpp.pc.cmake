#
# pkgconfig for rtosc_cpp
#

prefix=@CMAKE_INSTALL_PREFIX@
libdir=@CMAKE_INSTALL_FULL_LIBDIR@
includedir=@CMAKE_INSTALL_FULL_INCLUDEDIR@
set(LIBS_FLAG "$<$<CXX_COMPILER_ID:MSVC>:/LIBPATH:${libdir}>$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-L${libdir}> -lrtosc -lrtosc-cpp")


Name: rtosc_cpp
Description: rtosc_cpp - a realtime safe open sound control serialization and dispatch system for C++
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
Requires: librtosc = @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
set(Libs: ${LIBS_FLAG})
Cflags: -I${includedir}
