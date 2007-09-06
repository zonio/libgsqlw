AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = \
  $(GLIB_CFLAGS) \
  $(PGSQL_CFLAGS)

# library

lib_LTLIBRARIES = \
  libgsqlw.la

libgsqlw_la_LIBADD = \
  $(GLIB_LIBS) \
  $(PGSQL_LIBS)

libgsqlw_la_LDFLAGS = \
  -version-info 0:0:0 -no-undefined -z now

libgsqlw_la_SOURCES = \
  gsqlw.c \
  gsqlw-pgsql.c

# test code

bin_PROGRAMS = \
  gsqlw-test

gsqlw_test_SOURCES = gsqlw-test.c
gsqlw_test_LDADD = libgsqlw.la
gsqlw_test_LDFLAGS = -static
