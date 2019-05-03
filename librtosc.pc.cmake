#
# pkgconfig for rtosc
#

prefix=@CMAKE_INSTALL_PREFIX@
libdir=@CMAKE_INSTALL_FULL_LIBDIR@
includedir=@CMAKE_INSTALL_FULL_INCLUDEDIR@

Name: rtosc
Description: rtosc - a realtime safe open sound control serialization and dispatch system
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
Libs: -L${libdir} -lrtosc
Cflags: -I${includedir}
