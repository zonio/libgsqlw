AC_PREREQ(2.57)
AC_INIT([gsqlwrapper], 0.2, [oss@zonio.net], gsqlwrapper)
AC_CONFIG_SRCDIR([Makefile.am])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_LN_S

# Checks for pkg-config packages
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.8.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

CFLAGS="$CFLAGS -Wall -Wextra"

# Checks for header files.
AC_HEADER_STDC

# Output
AC_OUTPUT([
    Makefile
])
