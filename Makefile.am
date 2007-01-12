AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = \
  $(GLIB_CFLAGS)

# library

lib_LTLIBRARIES = \
  libgsqlwrapper.la

libgsqlwrapper_la_LIBADD = \
  $(GLIB_LIBS)

libgsqlwrapper_la_LDFLAGS = \
  -version-info 0:0:0 -no-undefined -z now

libgsqlwrapper_la_SOURCES = \
  gsqlwraper.c \
  gsqldriver.c

# test code

bin_PROGRAMS = \
  gsql-test

gsql_test_SOURCES = \
  test.c

gsql_test_LDADD = \
  libgsqlwrapper.la
