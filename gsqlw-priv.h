/*
 * Glib sql wrapper.
 *
 * Copyright (C) 2008-2010 Zonio s.r.o <developers@zonio.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __GSQLW_PRIV_H__
#define __GSQLW_PRIV_H__

#include <config.h>

#include "gsqlw.h"

typedef struct _gs_driver gs_driver;

struct _gs_conn
{
  char* dsn;
  int errcode;
  char* errmsg;
  gs_driver* driver;
  int in_transaction;
};

struct _gs_query
{
  gs_conn* conn;
  char* sql;
};

struct _gs_driver
{
  char* name;

  gs_conn* (*connect)(const char* dsn);
  void (*disconnect)(gs_conn* conn);

  int (*begin)(gs_conn* conn);
  int (*commit)(gs_conn* conn);
  int (*rollback)(gs_conn* conn);

  gs_query* (*query_new)(gs_conn* conn, const char* sql_string);
  void (*query_free)(gs_query* query);

  int (*query_getv)(gs_query* query, const char* fmt, va_list ap);
  int (*query_putv)(gs_query* query, const char* fmt, va_list ap);

  int (*query_get_rows)(gs_query* query);
  int (*query_get_last_id)(gs_query* query, const char* seq_name);
};

#ifdef HAVE_SQLITE
extern gs_driver sqlite_driver G_GNUC_INTERNAL;
#endif
#ifdef HAVE_POSTGRES
extern gs_driver pgsql_driver G_GNUC_INTERNAL;
#endif
#ifdef HAVE_MYSQL
extern gs_driver mysql_driver G_GNUC_INTERNAL;
#endif

#endif
