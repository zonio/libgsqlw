#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "gsqlw-priv.h"

struct _gs_conn_sqlite
{
  gs_conn base;
  sqlite3* handle;
};

enum _sqlite_query_state
{
  QUERY_STATE_INIT,          // just after sqlite3_prepare
  QUERY_STATE_ROW_PENDING,   // sqlite3_step was called and row was not retrieved using getv
  QUERY_STATE_ROW_READ,      // row was retrieved using getv
  QUERY_STATE_COMPLETED,     // no more rows
};

struct _gs_query_sqlite
{
  gs_query base;
  sqlite3_stmt* stmt;
  int state;
};

#define CONN(c) ((struct _gs_conn_sqlite*)(c))
#define QUERY(c) ((struct _gs_query_sqlite*)(c))

// dsn is filename
static gs_conn* sqlite_gs_connect(const char* dsn)
{
  struct _gs_conn_sqlite* conn;

  conn = g_new0(struct _gs_conn_sqlite, 1);
  if (sqlite3_open(dsn, &conn->handle) != SQLITE_OK)
    gs_set_error((gs_conn*)conn, GS_ERR_OTHER, sqlite3_errmsg(conn->handle));

  return (gs_conn*)conn;
}

static void sqlite_gs_disconnect(gs_conn* conn)
{
  sqlite3_close(CONN(conn)->handle);
}

static int sqlite_gs_begin(gs_conn* conn)
{
  return gs_exec(conn, "BEGIN", NULL);
}

static int sqlite_gs_commit(gs_conn* conn)
{
  return gs_exec(conn, "COMMIT", NULL);
}

static int sqlite_gs_rollback(gs_conn* conn)
{
  return gs_exec(conn, "ROLLBACK", NULL);
}

char* _sqlite_fixup_sql(const char* str)
{
  char* tmp = g_strdup(str);
  gboolean in_string = FALSE;
  guint i;

  if (tmp == NULL)
    return NULL;

  for (i = 0; i < strlen(tmp); i++)
  {
    if (tmp[i] == '\'')
      in_string = !in_string;
    if (!in_string && tmp[i] == '$' && g_ascii_isdigit(tmp[i+1]))
      tmp[i] = '?';
  }

  return tmp;
}

static void sqlite_gs_query_free(gs_query* query);

static gs_query* sqlite_gs_query_new(gs_conn* conn, const char* sql_string)
{
  struct _gs_query_sqlite* query;
  int rs;
  
  query = g_new0(struct _gs_query_sqlite, 1);
  query->base.conn = conn;
  query->base.sql = _sqlite_fixup_sql(sql_string);
  query->state = QUERY_STATE_INIT;

  rs = sqlite3_prepare_v2(CONN(conn)->handle, query->base.sql, -1, &query->stmt, NULL);
  if (rs != SQLITE_OK)
  {
    gs_set_error(conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(conn)->handle));
    sqlite_gs_query_free((gs_query*)query);
    return NULL;
  }

  return (gs_query*)query;
}

static void sqlite_gs_query_free(gs_query* query)
{
  if (QUERY(query)->stmt != NULL)
    sqlite3_finalize(QUERY(query)->stmt);
  g_free(query->sql);
  g_free(query);
}

static int sqlite_gs_query_getv(gs_query* query, const char* fmt, va_list ap)
{
  sqlite3_stmt* stmt = QUERY(query)->stmt;
  int param_count = fmt != NULL ? strlen(fmt) : 0;
  int i, rs;
  int col = 0;

  switch (QUERY(query)->state)
  {
    case QUERY_STATE_INIT: 
      gs_set_error(query->conn, GS_ERR_OTHER, "Invalid API use, call gs_query_put() before gs_query_get().");
      return -1;
    case QUERY_STATE_COMPLETED:
      return 1;
    case QUERY_STATE_ROW_READ:
      // fetch next row
      rs = sqlite3_step(stmt);
      if (rs == SQLITE_ROW)
        break;
      else if (rs == SQLITE_DONE)
      {
        QUERY(query)->state = QUERY_STATE_COMPLETED;
        return 1;
      }
      else
      {
        //XXX: set error based on sqlite state
        gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
        return -1;
      }
    case QUERY_STATE_ROW_PENDING:
      QUERY(query)->state = QUERY_STATE_ROW_READ;
      break;
  }

  for (i = 0; i < param_count; i++)
  {
    if (fmt[i] == 's')
    {
      char** str_ptr = (char**)va_arg(ap, char**);
      *str_ptr = (char*)sqlite3_column_text(stmt, col);
      col++;
    }
    else if (fmt[i] == 'S')
    {
      char** str_ptr = (char**)va_arg(ap, char**);
      *str_ptr = g_strdup((char*)sqlite3_column_text(stmt, col));
      col++;
    }
    else if (fmt[i] == 'i')
    {
      int* int_ptr = (int*)va_arg(ap, int*);
      *int_ptr = sqlite3_column_int(stmt, col);
      col++;
    }
    else if (fmt[i] == '?') // null flag
    {
      int* int_ptr = (int*)va_arg(ap, int*);
      *int_ptr = sqlite3_value_type(sqlite3_column_value(stmt, col)) == SQLITE_NULL;
    }
    else
    {
      gs_set_error(query->conn, GS_ERR_OTHER, "Invalid format string.");
      va_end(ap);
      return -1;
    }
  }

  return 0;
}

static int sqlite_gs_query_putv(gs_query* query, const char* fmt, va_list ap)
{
  sqlite3_stmt* stmt = QUERY(query)->stmt;
  int param_count = (fmt != NULL) ? strlen(fmt) : 0;
  int i, rs;
  int col = 1;

  if (QUERY(query)->state != QUERY_STATE_INIT)
  {
    if (sqlite3_reset(stmt) != SQLITE_OK)
    {
      gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
      return -1;
    }
  }

  for (i = 0; i < param_count; i++)
  {
    int is_null = 0;

    if (fmt[i] == '?')
    {
      is_null = (int)va_arg(ap, int);
      if (is_null)
        sqlite3_bind_null(stmt, col);
      i++;
    }

    if (fmt[i] == 's')
    {
      char* value = (char*)va_arg(ap, char*);
      if (!is_null)
      {
        if (value == NULL)
          sqlite3_bind_null(stmt, col);
        else
          sqlite3_bind_text(stmt, col, value, -1, SQLITE_TRANSIENT);
      }
      col++;
    }
    else if (fmt[i] == 'i')
    {
      int value = (int)va_arg(ap, int);
      if (!is_null)
        sqlite3_bind_int(stmt, col, value);
      col++;
    }
    else
    {
      gs_set_error(query->conn, GS_ERR_OTHER, "Invalid format string.");
      return -1;
    }
  }

  rs = sqlite3_step(stmt);
  if (rs == SQLITE_DONE)
    QUERY(query)->state = QUERY_STATE_COMPLETED;
  else if (rs == SQLITE_ROW)
    QUERY(query)->state = QUERY_STATE_ROW_PENDING;
  else
  {
    //XXX: set error based on sqlite state
    gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
    return -1;
  }

  return 0;
}

static int sqlite_gs_query_get_rows(gs_query* query)
{
  int rs;
  int count = 0;
  sqlite3_stmt* stmt = QUERY(query)->stmt;

  if (QUERY(query)->state == QUERY_STATE_INIT)
  {
    gs_set_error(query->conn, GS_ERR_OTHER, "Invalid API use, call gs_query_put() before gs_query_get_rows().");
    return -1;
  }

  if (sqlite3_reset(stmt) != SQLITE_OK)
  {
    gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
    return -1;
  }

  while (TRUE)
  {
    rs = sqlite3_step(stmt);
    if (rs == SQLITE_DONE)
      break;
    else if (rs == SQLITE_ROW)
      count++;
    else
    {
      //XXX: set error based on sqlite state
      gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
      return -1;
    }
  }

  if (sqlite3_reset(stmt) != SQLITE_OK)
  {
    gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
    return -1;
  }

  rs = sqlite3_step(stmt);
  if (rs == SQLITE_ROW)
    QUERY(query)->state = QUERY_STATE_ROW_PENDING;
  else if (rs == SQLITE_DONE)
    QUERY(query)->state = QUERY_STATE_COMPLETED;
  else
  {
    //XXX: set error based on sqlite state
    gs_set_error(query->conn, GS_ERR_OTHER, sqlite3_errmsg(CONN(query->conn)->handle));
    return -1;
  }

  return count;
}

static int sqlite_gs_query_get_last_id(gs_query* query, const char* seq_name)
{
  int id;

  id = sqlite3_last_insert_rowid(CONN(query->conn)->handle);

  return id;
}

gs_driver sqlite_driver =
{
  .name = "sqlite",
  .connect = sqlite_gs_connect,
  .disconnect = sqlite_gs_disconnect,
  .begin = sqlite_gs_begin,
  .commit = sqlite_gs_commit,
  .rollback = sqlite_gs_rollback,
  .query_new = sqlite_gs_query_new,
  .query_free = sqlite_gs_query_free,
  .query_getv = sqlite_gs_query_getv,
  .query_putv = sqlite_gs_query_putv,
  .query_get_rows = sqlite_gs_query_get_rows,
  .query_get_last_id = sqlite_gs_query_get_last_id,
};
