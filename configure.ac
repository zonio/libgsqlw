AC_PREREQ(2.57)
AC_INIT([libgsqlw], 1.0, [megous@megous.com], libgsqlw)
AC_CONFIG_SRCDIR([Makefile.am])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_LN_S

# Checks for pkg-config packages
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.8.0 gthread-2.0 >= 2.8.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

CFLAGS="$CFLAGS -Wall -Wextra"

# Checks for header files.
AC_HEADER_STDC

# Postgres
AC_MSG_CHECKING(for pg_config)
for i in /usr/local/pgsql/bin /usr/local/bin /usr/bin "" ; do
    if test -x $i/pg_config; then
        PG_CONFIG="$i/pg_config"
        break
    fi
done

if test -n "$PG_CONFIG"; then
    AC_MSG_RESULT([$PG_CONFIG])
    PGSQL_INCLUDE=`$PG_CONFIG --includedir`
    PGSQL_LIBDIR=`$PG_CONFIG --libdir`
    AC_DEFINE(HAVE_PG_CONFIG_H,1,[Whether to have pg_config.h])
    PGSQL_CFLAGS="-I$PGSQL_INCLUDE"
    PGSQL_LIBS="-L$PGSQL_LIBDIR -lpq"
    AC_SUBST(PGSQL_CFLAGS)
    AC_SUBST(PGSQL_LIBS)
    AC_CHECK_LIB(pq, PQexecParams,AC_DEFINE(HAVE_PQEXECPARAMS,1,[PostgreSQL 7.4 or later]))
    AC_CHECK_LIB(pq, PQprepare,AC_DEFINE(HAVE_PQPREPARE,1,[prepared statements]))
fi

SQLITE_LIBS="-lsqlite3"
AC_SUBST(SQLITE_LIBS)

#
AS_IF([pkg-config 'sqlite3 >= 3.3.6'],
    [AC_DEFINE(HAVE_SQLITE_V2_METHODS, 1, [whether sqlite supports new API])
])

# Output
AC_OUTPUT([
    Makefile
    docs/Makefile
    libgsqlw.pc
])
