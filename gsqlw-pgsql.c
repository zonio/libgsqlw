#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

#include "gsqlw-priv.h"

struct _gs_conn_pgsql
{
  gs_conn base;
  PGconn* pg;
};

struct _gs_query_pgsql
{
  gs_query base;
  PGresult* pg_res;
  int row_no;
};

#define CONN(c) ((struct _gs_conn_pgsql*)(c))
#define QUERY(c) ((struct _gs_query_pgsql*)(c))

static gs_conn* pgsql_gs_connect(const char* dsn)
{
  struct _gs_conn_pgsql* conn;
  PGconn* pg;
  
  pg = PQconnectdb(dsn);
  if (PQstatus(pg) == CONNECTION_BAD)
    return NULL;

  conn = g_new0(struct _gs_conn_pgsql, 1);
  conn->pg = pg;

  return (gs_conn*)conn;
}

static void pgsql_gs_disconnect(gs_conn* conn)
{
  PQfinish(CONN(conn)->pg);
}

static int pgsql_gs_begin(gs_conn* conn)
{
  PGresult* res;
  
  res = PQexec(CONN(conn)->pg, "BEGIN");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    conn->error = PQresultErrorMessage(res);
    PQclear(res);
    return -1;
  }

  PQclear(res);
  return 0;
}

static int pgsql_gs_commit(gs_conn* conn)
{
  PGresult* res;
  
  res = PQexec(CONN(conn)->pg, "COMMIT");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    conn->error = PQresultErrorMessage(res);
    PQclear(res);
    return -1;
  }

  PQclear(res);
  return 0;
}

static int pgsql_gs_rollback(gs_conn* conn)
{
  PGresult* res;
  
  res = PQexec(CONN(conn)->pg, "ROLLBACK");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    conn->error = PQresultErrorMessage(res);
    PQclear(res);
    return -1;
  }

  PQclear(res);
  return 0;
}

static gs_query* pgsql_gs_query_new(gs_conn* conn, const char* sql_string)
{
  struct _gs_query_pgsql* query;
  
  query = g_new0(struct _gs_query_pgsql, 1);
  query->base.conn = conn;
  query->base.sql = g_strdup(sql_string);

  return (gs_query*)query;
}

static void pgsql_gs_query_free(gs_query* query)
{
  if (QUERY(query)->pg_res != NULL)
    PQclear(QUERY(query)->pg_res);
  g_free(query->sql);
  g_free(query);
}

static int pgsql_gs_query_getv(gs_query* query, const char* fmt, va_list ap)
{
  PGresult* res = QUERY(query)->pg_res;
  int row_no = QUERY(query)->row_no;

  if (res == NULL)
    return -1;

  if (row_no >= gs_query_get_rows(query))
    return 1;

  int param_count = fmt != NULL ? strlen(fmt) : 0;
  int i, col = 0;

  for (i = 0; i < param_count; i++)
  {
    if (fmt[i] == 's')
    {
      char** str_ptr = (char**)va_arg(ap, char**);
      if (PQgetisnull(res, row_no, col))
        *str_ptr = NULL;
      else
        *str_ptr = PQgetvalue(res, row_no, col);
      col++;
    }
    else if (fmt[i] == 'S')
    {
      char** str_ptr = (char**)va_arg(ap, char**);
      if (PQgetisnull(res, row_no, col))
        *str_ptr = NULL;
      else
        *str_ptr = g_strdup(PQgetvalue(res, row_no, col));
      col++;
    }
    else if (fmt[i] == 'i')
    {
      int* int_ptr = (int*)va_arg(ap, int*);
      if (!PQgetisnull(res, row_no, col))
        *int_ptr = atoi(PQgetvalue(res, row_no, col));
      col++;
    }
    else if (fmt[i] == '?') // null flag
    {
      int* int_ptr = (int*)va_arg(ap, int*);
      *int_ptr = PQgetisnull(res, row_no, col);
    }
    else
    {
      query->conn->error = "invalid format string";
      va_end(ap);
      return -1;
    }
  }

  QUERY(query)->row_no++;
  return 0;
}

static int pgsql_gs_query_putv(gs_query* query, const char* fmt, va_list ap)
{
  int param_count = (fmt != NULL) ? strlen(fmt) : 0;
  char** param_values = g_new0(char*, param_count);
  int* free_list = g_new0(int, param_count);
  int i, col = 0, retval = 0;
  PGresult* res;

  for (i = 0; i < param_count; i++)
  {
    if (fmt[i] == 's')
    {
      char* value = (char*)va_arg(ap, char*);
      param_values[col] = value;
      col++;
    }
    else if (fmt[i] == 'i')
    {
      int value = (int)va_arg(ap, int);
      param_values[col] = g_strdup_printf("%d", value);
      free_list[col] = 1;
      col++;
    }
    else if (fmt[i] == '?')
    {
      int is_null = (int)va_arg(ap, int);
      if (is_null)
      {
        i++;
        col++;
      }
    }
    else
    {
      query->conn->error = "invalid format string";
      retval = -1;
      goto err;
    }
  }
  param_count = col;

  res = PQexecParams(CONN(query->conn)->pg, query->sql, param_count, NULL, (const char* const*)param_values, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK
      && PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    query->conn->error = PQresultErrorMessage(res);
    retval = -1;
  }

  if (QUERY(query)->pg_res != NULL)
    PQclear(QUERY(query)->pg_res);
  QUERY(query)->pg_res = res;
  
 err:
  for (i=0; i<param_count; i++)
    if (free_list[i])
      g_free(param_values[i]);
  g_free(param_values);
  g_free(free_list);

  return retval;
}

static int pgsql_gs_query_get_rows(gs_query* query)
{
  PGresult* res = QUERY(query)->pg_res;
  if (res)
    return PQntuples(res);
  return -1;
}

static int pgsql_gs_query_get_last_id(gs_query* query, const char* seq_name)
{
  g_warning("pgsql_gs_query_get_last_id() is not implemented!");
  return -1;
}

gs_driver pgsql_driver =
{
  .name = "pgsql",
  .connect = pgsql_gs_connect,
  .disconnect = pgsql_gs_disconnect,
  .begin = pgsql_gs_begin,
  .commit = pgsql_gs_commit,
  .rollback = pgsql_gs_rollback,
  .query_new = pgsql_gs_query_new,
  .query_free = pgsql_gs_query_free,
  .query_getv = pgsql_gs_query_getv,
  .query_putv = pgsql_gs_query_putv,
  .query_get_rows = pgsql_gs_query_get_rows,
  .query_get_last_id = pgsql_gs_query_get_last_id,
};
