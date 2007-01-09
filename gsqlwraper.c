#include "gsqlwrapper.h"
#include "gsqldriver.h"
#include "gsql-priv.h"

gs_conn* gs_connect(const char* dsn_string, const char* username, const char* password)
{
  gs_conn* conn = g_new0(gs_conn, 1);
  conn->dsn = gs_dsn_new(dsn_string);
  conn->driver = gs_driver_get(dsn->driver_name);
  conn->driver->connect(dsn, username, password);
  return conn;
}

void gs_disconnect(gs_conn* conn)
{
  conn->driver->disconnect();
}

gs_query* gs_query_new(gs_conn* conn, const char* sql)
{
  // parse and check for ? sequences
  // create gs_query
  // run query if no ? sequneces are present
}

void gs_query_free(gs_query* query)
{
  // free query
}

int gs_query_get(gs_query* query, const char* fmt, ...)
{
}

int gs_query_put(gs_query* query, const char* fmt, ...)
{
}

/* library initialization */

int gs_init()
{
}

void gs_fini()
{
}
