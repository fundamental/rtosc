#
# pkgconfig for rtosc_cpp
#

prefix=@CMAKE_INSTALL_PREFIX@
libdir=@CMAKE_INSTALL_FULL_LIBDIR@
includedir=@CMAKE_INSTALL_FULL_INCLUDEDIR@

Name: rtosc_cpp
Description: rtosc_cpp - a realtime safe open sound control serialization and dispatch system for C++
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
Requires: librtosc = @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
if(NOT MSVC)
    Libs: -L${libdir} -lrtosc -lrtosc-cpp
else
    Libs: /LIBPATH:${libdir} -lrtosc -lrtosc-cpp
endif()
Cflags: -I${includedir}
