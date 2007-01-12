#include "gsqlwraper.h"
#include "gsqldriver.h"
#include "gsql-priv.h"

gs_conn* gs_connect(const char* dsn_string, const char* username, const char* password)
{
  gs_conn* conn = g_new0(gs_conn, 1);
  conn->dsn = gs_dsn_new(dsn_string);
  conn->driver = gs_driver_get(conn->dsn->dbtype);
  conn->driver->connect(conn, username, password);
  return conn;
}

void gs_disconnect(gs_conn* conn)
{
  conn->driver->disconnect(conn);
  gs_dsn_free(conn->dsn);
  g_free(conn);
}

gs_query* gs_query_new(gs_conn* conn, const char* sql_string)
{
  gs_query* query = g_new0(gs_query, 1);
  query->conn = conn;
  conn->driver->query_new(query);
  gs_sql* sql = gs_sql_new(sql_string);
  // parse and check for ? sequences
  // create gs_query
  // run query if no ? sequneces are present
}

void gs_query_free(gs_query* query)
{
  query->conn->driver->query_free(query);
  g_free(query);
  // free query
}

int gs_query_get(gs_query* query, const char* fmt, ...)
{
}

int gs_query_put(gs_query* query, const char* fmt, ...)
{
}

/* */

int gs_query_get_rows(gs_query* query)
{
}

int gs_query_get_num(gs_query* query)
{
}

int gs_query_get_last_id(gs_query* query)
{
}

/* library initialization */

int gs_init()
{
}

void gs_fini()
{
}
