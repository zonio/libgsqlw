AUTOMAKE_OPTIONS = foreign

SUBDIRS = docs

AM_CFLAGS = \
  $(GLIB_CFLAGS) \
  $(PGSQL_CFLAGS)

# library

include_HEADERS = \
  gsqlw.h

lib_LTLIBRARIES = \
  libgsqlw.la

libgsqlw_la_LIBADD = \
  $(GLIB_LIBS) \
  $(PGSQL_LIBS) \
  $(SQLITE_LIBS)

libgsqlw_la_LDFLAGS = \
  -version-info 0:0:0 -no-undefined -z now

libgsqlw_la_SOURCES = \
  gsqlw.h \
  gsqlw.c \
  gsqlw-priv.h \
  gsqlw-pgsql.c \
  gsqlw-sqlite.c

# pkgconfig

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsqlw.pc

EXTRA_DIST = libgsqlw.pc.in

# test code

noinst_PROGRAMS = \
  gsqlw-test

gsqlw_test_SOURCES = gsqlw-test.c
gsqlw_test_LDADD = libgsqlw.la
gsqlw_test_LDFLAGS = -static
