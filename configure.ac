#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.59])
AC_INIT([statmon], [0.1], [adam.r.drescher@gmail.com])
AC_CONFIG_SRCDIR([statmon.cpp])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([config/config.h])
AM_INIT_AUTOMAKE([foreign])

# change language to C++
AC_LANG(C++)

# debug flag
AC_ARG_ENABLE([debug],
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
  yes) debug=true ;;
  no)  debug=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],[debug=false])
AM_CONDITIONAL([DEBUG], [test "x${debug}" = "xtrue"])

# override default CXXFLAGS if we are debugging
if (test "x${debug}" = "xtrue"); then
   CXXFLAGS="-g -O0"
fi

# check for necessary programs.
AC_PATH_TOOL(PKG_CONFIG, pkg-config)
AC_PROG_CXX([clang++ g++ c++])

# will resolve -std=c++0x or c++11 automatically
AX_CXX_COMPILE_STDCXX_11

# check for C header files
AC_CHECK_HEADERS([sys/time.h unistd.h netlink/netlink.h netlink/route/link.h netlink/cache.h])

# check for header files.
AC_CHECK_HEADERS([iostream fstream csignal iomanip string vector])

# checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT64_T

# checks for library functions.
AC_CHECK_FUNCS([gettimeofday signal getopt])

# checks for libraries.
# libnl-route-3.0 is a superset of libnl-3.0 
PKG_CHECK_MODULES(LIBNL3, libnl-route-3.0 >= 3.1, [have_libnl3=yes], [have_libnl3=no])
if (test "${have_libnl3}" = "yes"); then
   CXXFLAGS+=" $LIBNL3_CFLAGS"
   LIBS+=" $LIBNL3_LIBS"
else
	AC_MSG_ERROR([You need libnl-3.0 (netlink) to build this program!])	
fi

AC_OUTPUT(Makefile)
