AC_PREREQ([2.69])
AC_INIT([libgsqlw],[1.0.2-],[3e-list@zonio.net],[libgsqlw])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

# Checks for programs.
LT_INIT
AC_PROG_CC
AC_PROG_INSTALL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset strchr])

# Checks for pkg-config packages
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.8.0 gthread-2.0 >= 2.8.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

CFLAGS="$CFLAGS -Wall -Wextra"

AC_DEFUN([BACKEND_ARG_ENABLE], [
	AC_ARG_ENABLE($1,
		AS_HELP_STRING([--disable-$1],[disable $1 (default: autodetect)]),
		[
			case "${enableval}" in
				yes) $2=yes ;;
				no)  $2=no ;;
				*)   AC_MSG_ERROR(bad value ${enableval} for --enable-$1) ;;
			esac
		],
		[$2=auto])
	])

BACKEND_ARG_ENABLE(mysql, USE_MYSQL)
BACKEND_ARG_ENABLE(postgres, USE_POSTGRES)
BACKEND_ARG_ENABLE(sqlite, USE_SQLITE)

AC_ARG_WITH(mysql, [  --with-mysql=PATH path to mysql_config binary or mysql prefix dir], [
		if test -x $withval -a -f $withval; then
			MYSQL_CONFIG=$withval
		elif test -x $withval/bin/mysql_config -a -f $withval/bin/mysql_config; then
			MYSQL_CONFIG=$withval/bin/mysql_config
		fi
	], [
		if test -x /usr/local/mysql/bin/mysql_config -a -f /usr/local/mysql/bin/mysql_config; then
			MYSQL_CONFIG=/usr/local/mysql/bin/mysql_config
		elif test -x /usr/bin/mysql_config -a -f /usr/bin/mysql_config; then
			MYSQL_CONFIG=/usr/bin/mysql_config
		fi
	])

if test x$USE_POSTGRES != xno; then
	# Postgres
	AC_MSG_CHECKING(for pg_config)
	for i in /usr/local/pgsql/bin /usr/local/bin /usr/bin "" ; do
		if test -x $i/pg_config; then
			PG_CONFIG="$i/pg_config"
			break
		fi
	done

	if test -n "$PG_CONFIG"; then
		AM_CONDITIONAL(POSTGRES_NO_DEV_FILES, [`pg_config > /dev/null; test $? -ne 0`])
		AM_COND_IF(POSTGRES_NO_DEV_FILES,
			AC_MSG_ERROR([missing client development files. Install libpq-dev/libpq-devel]))
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
		AC_DEFINE(HAVE_POSTGRES, 1, [Have postgres])
		USE_POSTGRES=yes
	elif test x$USE_POSTGRES = xyes; then
		AC_MSG_ERROR([No postgres devel files found, install libpq-dev/libpq-devel])
	else
		USE_POSTGRES=no
	fi
fi

if test x$USE_SQLITE != xno; then
	# Sqlite

	PKG_CHECK_MODULES(SQLITE3, [sqlite3])
	SQLITE_LIBS="-lsqlite3"
	AC_SUBST(SQLITE_LIBS)

	if test x$USE_SQLITE = xyes; then
		PKG_CHECK_MODULES(SQLITE, sqlite3)
		USE_SQLITE=yes
	else
		PKG_CHECK_MODULES(SQLITE, sqlite3, [USE_SQLITE=yes], [USE_SQLITE=no])
	fi

	if test x$USE_SQLITE = xyes; then
		AC_SUBST(SQLITE_CFLAGS)
		AC_SUBST(SQLITE_LIBS)

		AC_DEFINE(HAVE_SQLITE, 1, [Have sqlite])

		AS_IF([pkg-config 'sqlite3 >= 3.3.6'],
			[AC_DEFINE(HAVE_SQLITE_V2_METHODS, 1, [whether sqlite supports new API])
		])
	fi
fi

if test x$USE_MYSQL != xno; then
	# MySQL
	AC_MSG_CHECKING(for mysql_config executable)

	if test "x$MYSQL_CONFIG" = x; then
		if test x$USE_MYSQL = yes; then
			AC_MSG_ERROR([not found])
		fi
		AC_MSG_RESULT([not found])
		USE_MYSQL=no
	else
		USE_MYSQL=yes

		# add regular MySQL C flags
		ADDFLAGS=`$MYSQL_CONFIG --cflags`

		# add NDB API specific C flags
		IBASE=`$MYSQL_CONFIG --include`
		ADDFLAGS="$ADDFLAGS $IBASE/storage/ndb"
		ADDFLAGS="$ADDFLAGS $IBASE/storage/ndb/ndbapi"
		ADDFLAGS="$ADDFLAGS $IBASE/storage/ndb/mgmapi"

		CFLAGS="$CFLAGS $ADDFLAGS"

		LDFLAGS="$LDFLAGS "`$MYSQL_CONFIG --libs_r`
		LDFLAGS="$LDFLAGS "`$MYSQL_CONFIG --libs_r`

		AC_DEFINE(HAVE_MYSQL, 1, [Have MySQL])
		AC_MSG_RESULT($MYSQL_CONFIG)
	fi
fi

if test x$USE_MYSQL = xno -a x$USE_POSTGRES = xno -a x$USE_SQLITE = xno; then
	AC_MSG_ERROR([You either don't have any of the supported database backends or you disabled them all])
fi

AM_CONDITIONAL(SQLITE, test x$USE_SQLITE != xno)
AM_CONDITIONAL(POSTGRES, test x$USE_POSTGRES != xno)
AM_CONDITIONAL(MYSQL, test x$USE_MYSQL != xno)

# Output
AC_CONFIG_FILES([
	Makefile
	docs/Makefile
	libgsqlw.pc
])
AC_OUTPUT
