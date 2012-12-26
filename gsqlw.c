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

#include <stdlib.h>
#include <string.h>

#include <config.h>

#include "gsqlw-priv.h"

static gs_driver* drivers[] = {
#ifdef HAVE_SQLITE
  &sqlite_driver,
#endif
#ifdef HAVE_POSTGRES
  &pgsql_driver,
#endif
#ifdef HAVE_MYSQL
  &mysql_driver,
#endif
};

#define CONN_DRIVER(c) \
  c->driver

#define QUERY_DRIVER(q) \
  q->conn->driver

#define CONN_RETURN_IF_INVALID(c) \
  if (gs_get_errcode(c) != GS_ERR_NONE) \
    return;

#define CONN_RETURN_VAL_IF_INVALID(c, val) \
  if (gs_get_errcode(c) != GS_ERR_NONE) \
    return val;

#define QUERY_RETURN_IF_INVALID(q) \
  if (q == NULL || gs_get_errcode(q->conn) != GS_ERR_NONE) \
    return;

#define QUERY_RETURN_VAL_IF_INVALID(q, val) \
  if (q == NULL || gs_get_errcode(q->conn) != GS_ERR_NONE) \
    return val;

gs_conn* gs_connect(const char* dsn)
{
  guint i;
  gs_conn* conn = NULL;

  if (dsn == NULL)
    return NULL;
  
  for (i = 0; i < G_N_ELEMENTS(drivers); i++)
  {
    const char* name = drivers[i]->name;
    if (g_str_has_prefix(dsn, name) && dsn[strlen(name)] == ':')
    {
      const char* drv_dsn = strchr(dsn, ':') + 1;
      conn = drivers[i]->connect(drv_dsn);
      if (conn)
      {
        conn->dsn = g_strdup(drv_dsn);
        conn->driver = drivers[i];
      }
      return conn;
    }
  }

  return NULL;
}

void gs_disconnect(gs_conn* conn)
{
  if (conn == NULL)
    return;
  CONN_DRIVER(conn)->disconnect(conn);
  gs_clear_error(conn);
  g_free(conn->dsn);
  g_free(conn);
}

const char* gs_get_backend(gs_conn* conn)
{
  if (conn == NULL || conn->driver == NULL || conn->driver->name == NULL)
    return "";
  return conn->driver->name;
}

const char* gs_get_errmsg(gs_conn* conn)
{
  if (conn == NULL)
    return "Connection object is NULL.";
  return conn->errmsg;
}

int gs_get_errcode(gs_conn* conn)
{
  if (conn == NULL)
    return GS_ERR_OTHER;
  return conn->errcode;
}

void gs_set_error(gs_conn* conn, int code, const char* msg)
{
  CONN_RETURN_IF_INVALID(conn);
  conn->errcode = code;
  g_free(conn->errmsg);
  conn->errmsg = g_strdup(msg);
}

void gs_clear_error(gs_conn* conn)
{
  if (conn == NULL)
    return;
  conn->errcode = GS_ERR_NONE;
  g_free(conn->errmsg);
  conn->errmsg = NULL;
}

int gs_begin(gs_conn* conn)
{
  CONN_RETURN_VAL_IF_INVALID(conn, -1);
  int retval = CONN_DRIVER(conn)->begin(conn);
  if (retval == 0)
    conn->in_transaction = TRUE;
  return retval;
}

int gs_commit(gs_conn* conn)
{
  CONN_RETURN_VAL_IF_INVALID(conn, -1);
  int retval = CONN_DRIVER(conn)->commit(conn);
  if (retval == 0)
    conn->in_transaction = FALSE;
  return retval;
}

int gs_rollback(gs_conn* conn)
{
  if (conn == NULL)
    return -1;
  int retval = CONN_DRIVER(conn)->rollback(conn);
  if (retval == 0)
    conn->in_transaction = FALSE;
  return retval;
}

gs_query* gs_query_new(gs_conn* conn, const char* sql_string)
{
  CONN_RETURN_VAL_IF_INVALID(conn, NULL);
  return CONN_DRIVER(conn)->query_new(conn, sql_string);
}

int gs_query_putv(gs_query* query, const char* fmt, va_list ap)
{
  QUERY_RETURN_VAL_IF_INVALID(query, -1);
  return QUERY_DRIVER(query)->query_putv(query, fmt, ap);
}

int gs_query_put(gs_query* query, const char* fmt, ...)
{
  int retval;
  va_list ap;

  va_start(ap, fmt);
  retval = gs_query_putv(query, fmt, ap);
  va_end(ap);

  return retval;
}

void gs_query_free(gs_query* query)
{
  if (query)
    QUERY_DRIVER(query)->query_free(query);
}

int gs_query_getv(gs_query* query, const char* fmt, va_list ap)
{
  QUERY_RETURN_VAL_IF_INVALID(query, -1);
  return QUERY_DRIVER(query)->query_getv(query, fmt, ap);
}

int gs_query_get(gs_query* query, const char* fmt, ...)
{
  int retval;
  va_list ap;

  va_start(ap, fmt);
  retval = gs_query_getv(query, fmt, ap);
  va_end(ap);

  return retval;
}

int gs_query_get_rows(gs_query* query)
{
  QUERY_RETURN_VAL_IF_INVALID(query, -1);
  return QUERY_DRIVER(query)->query_get_rows(query);
}

int gs_query_get_last_id(gs_query* query, const char* seq_name)
{
  QUERY_RETURN_VAL_IF_INVALID(query, -1);
  return QUERY_DRIVER(query)->query_get_last_id(query, seq_name);
}

/* helper functions */

int gs_exec(gs_conn* conn, const char* sql_string, const char* fmt, ...)
{
  int retval;
  va_list ap;
  gs_query* query;

  query = gs_query_new(conn, sql_string);
  if (!query)
    return -1;

  va_start(ap, fmt);
  retval = gs_query_putv(query, fmt, ap);
  va_end(ap);

  gs_query_free(query);

  return retval;
}

int gs_finish(gs_conn* conn)
{
  if (conn == NULL)
    return -1;
  if (!conn->in_transaction)
    return -1;
  if (gs_get_errcode(conn) != GS_ERR_NONE)
  {
    gs_rollback(conn);
    return -1;
  }
  gs_commit(conn);
  return 0;
}
