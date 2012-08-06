AUTOMAKE_OPTIONS = foreign

SUBDIRS = docs

libgsqlw_la_CFLAGS = \
  $(GLIB_CFLAGS)

include_HEADERS = \
  gsqlw.h

lib_LTLIBRARIES = \
  libgsqlw.la

libgsqlw_la_LIBADD = \
  $(GLIB_LIBS)

libgsqlw_la_LDFLAGS = \
  -version-info 0:0:0 -no-undefined -z now

libgsqlw_la_SOURCES = \
  gsqlw.h \
  gsqlw.c \
  gsqlw-priv.h

if POSTGRES
libgsqlw_la_CFLAGS += \
  $(PGSQL_CFLAGS)

libgsqlw_la_LIBADD += \
  $(PGSQL_LIBS)

libgsqlw_la_SOURCES += \
  gsqlw-pgsql.c
endif

if SQLITE
libgsqlw_la_CFLAGS += \
  $(SQLITE_CFLAGS)

libgsqlw_la_LIBADD += \
  $(SQLITE_LIBS)

libgsqlw_la_SOURCES += \
  gsqlw-sqlite.c
endif

if MYSQL
libgsqlw_la_CFLAGS += \
  $(MYSQL_CFLAGS)

libgsqlw_la_LIBADD += \
  $(MYSQL_LIBS)

libgsqlw_la_SOURCES += \
  gsqlw-mysql.c
endif

# pkgconfig

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsqlw.pc

EXTRA_DIST = libgsqlw.pc.in

# test code

noinst_PROGRAMS = \
  gsqlw-test

gsqlw_test_CFLAGS = $(GLIB_CFLAGS)
gsqlw_test_SOURCES = gsqlw-test.c
gsqlw_test_LDADD = libgsqlw.la
gsqlw_test_LDFLAGS = -static
