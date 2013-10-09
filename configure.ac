AC_PREREQ([2.69])
AC_INIT([libgsqlw],[1.0.2-],[3e-list@zonio.net],[libgsqlw])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
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

AC_DEFUN([BACKEND_ARG_ENABLE],
	 [
	   AC_ARG_ENABLE($1,
			 [AS_HELP_STRING([--disable-$1],[disable $1 (default: autodetect)])],
			 [AS_CASE([$enableval],
				  [yes], [$2=yes],
				  [no], [$2=no],
				  [auto], [$2=auto],
				  [AC_MSG_ERROR([bad value $enableval for --enable-$1])])
			 ],
			 [$2=auto])
	 ])

BACKEND_ARG_ENABLE(mysql, USE_MYSQL)
BACKEND_ARG_ENABLE(postgres, USE_POSTGRES)
BACKEND_ARG_ENABLE(sqlite, USE_SQLITE)

AC_ARG_WITH([mysql],
	    [AS_HELP_STRING([--with-mysql=PATH],[path to mysql_config binary or mysql prefix dir])],
	    [with_mysql=$withval])

AS_IF([test "x$USE_POSTGRES" != xno],
      [
	# Postgres
	AC_PATH_PROG([PG_CONFIG],[pg_config],[],[$PATH$PATH_SEPARATOR/usr/local/pgsql/bin$PATH_SEPARATOR/usr/local/bin$PATH_SEPARATOR/usr/bin$PATH_SEPARATOR/])

	AS_IF([test -n "$PG_CONFIG"],
	      [
		AS_IF([$PG_CONFIG > /dev/null],
		      [],
		      [AC_MSG_ERROR([missing client development files. Install libpq-dev/libpq-devel])])
		PGSQL_INCLUDE=`$PG_CONFIG --includedir`
		PGSQL_LIBDIR=`$PG_CONFIG --libdir`
		PGSQL_CFLAGS="-I$PGSQL_INCLUDE"
		PGSQL_LIBS="-L$PGSQL_LIBDIR -lpq"
		AC_SUBST(PGSQL_CFLAGS)
		AC_SUBST(PGSQL_LIBS)
		AC_DEFINE(HAVE_POSTGRES, 1, [Have postgres])
		USE_POSTGRES=yes
	      ],
	      [test "x$USE_POSTGRES" = xyes],
	      [AC_MSG_ERROR([No postgres devel files found, install libpq-dev/libpq-devel])],
	      [USE_POSTGRES=no])
      ])

AS_IF([test "x$USE_SQLITE" != xno],
      [
	# Sqlite
	PKG_CHECK_MODULES(SQLITE, [sqlite3],
			  [USE_SQLITE=yes],
			  [AS_IF([test x$USE_SQLITE = xyes],
				 [AC_MSG_ERROR([No sqlite3 libraries found while explicitly requested])],
				 [USE_SQLITE=no])])

	AS_IF([test "x$USE_SQLITE" = xyes],
	      [
		AC_SUBST(SQLITE_CFLAGS)
		AC_SUBST(SQLITE_LIBS)

		AC_DEFINE(HAVE_SQLITE, 1, [Have sqlite])

		PKG_CHECK_EXISTS([sqlite3 >= 3.3.6],
				 [AC_DEFINE(HAVE_SQLITE_V2_METHODS, 1, [whether sqlite supports new API])])
	      ])
      ])

AS_IF([test "x$USE_MYSQL" != xno],
      [
	# MySQL
	AS_IF([test "x$with_mysql" != x],
	      [
		AC_MSG_CHECKING(for mysql_config)
		AS_IF([AS_EXECUTABLE_P("$with_mysql")],
		      [MYSQL_CONFIG=$with_mysql],
		      [AS_EXECUTABLE_P("$with_mysql/bin/mysql_config")],
		      [MYSQL_CONFIG=$with_mysql/bin/mysql_config])

		AS_IF([test "x$MYSQL_CONFIG" = x],
		      [AC_MSG_RESULT([not found])],
		      [AC_MSG_RESULT([$MYSQL_CONFIG])])
	      ],
	      [AC_PATH_PROG([MYSQL_CONFIG],[mysql_config],[],[$PATH$PATH_SEPARATOR/usr/local/mysql/bin$PATH_SEPARATOR/usr/bin])])

	AS_IF([test "x$MYSQL_CONFIG" = x -a "x$USE_MYSQL" = xyes],
	      [AC_MSG_ERROR([No MySQL libraries found while explicitly requested])])

	AS_IF([test "x$MYSQL_CONFIG" = x],
	      [USE_MYSQL=no],
	      [
		USE_MYSQL=yes

		# add regular MySQL C flags
		MYSQL_CFLAGS=`$MYSQL_CONFIG --cflags`

		# add NDB API specific C flags
		IBASE=`$MYSQL_CONFIG --include`
		MYSQL_CFLAGS="$MYSQL_CFLAGS $IBASE/storage/ndb"
		MYSQL_CFLAGS="$MYSQL_CFLAGS $IBASE/storage/ndb/ndbapi"
		MYSQL_CFLAGS="$MYSQL_CFLAGS $IBASE/storage/ndb/mgmapi"

		MYSQL_LIBS=`$MYSQL_CONFIG --libs_r`

		AC_SUBST(MYSQL_CFLAGS)
		AC_SUBST(MYSQL_LIBS)

		AC_DEFINE(HAVE_MYSQL, 1, [Have MySQL])
	      ])
      ])

AS_IF([test "x$USE_MYSQL" = xno -a "x$USE_POSTGRES" = xno -a "x$USE_SQLITE" = xno],
      [AC_MSG_ERROR(["You either don't have any of the supported database backends or you disabled them all"])])

AM_CONDITIONAL(SQLITE,   [test "x$USE_SQLITE" = xyes])
AM_CONDITIONAL(POSTGRES, [test "x$USE_POSTGRES" = xyes])
AM_CONDITIONAL(MYSQL,    [test "x$USE_MYSQL" = xyes])

# Output
AC_CONFIG_FILES([
		  Makefile
		  docs/Makefile
		  libgsqlw.pc
		])
AC_OUTPUT
