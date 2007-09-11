#include <stdlib.h>
#include <string.h>

#include "gsqlw-priv.h"

static gs_driver* drivers[] = {
  //&sqlite_driver,
  &pgsql_driver,
};

#define CONN_DRIVER(c) \
  c->driver

#define QUERY_DRIVER(q) \
  q->conn->driver

#define CONN_RETURN_IF_INVALID(c) \
  if (c == NULL || c->error) \
    return;

#define CONN_RETURN_VAL_IF_INVALID(c, val) \
  if (c == NULL || c->error) \
    return val;

#define QUERY_RETURN_IF_INVALID(q) \
  if (q == NULL || q->conn->error) \
    return;

#define QUERY_RETURN_VAL_IF_INVALID(q, val) \
  if (q == NULL || q->conn->error) \
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
  g_free(conn->dsn);
  g_free(conn);
}

const char* gs_get_error(gs_conn* conn)
{
  if (conn == NULL)
    return "Connection obejct is NULL";
  return conn->error;
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
  if (gs_get_error(conn))
  {
    gs_rollback(conn);
    return -1;
  }
  gs_commit(conn);
  return 0;
}
