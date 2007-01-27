#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

#include "gsqlw.h"

struct _gs_conn
{
  char* dsn;
  PGconn* pg;
  char* error;
};

struct _gs_query
{
  gs_conn* conn;
  char* sql;
  PGresult* pg_res;
  int failed;
  int row_no;
};

gs_conn* gs_connect(const char* dsn)
{
  PGconn *pg = PQconnectdb(dsn);
  if (PQstatus(pg) == CONNECTION_BAD)
    return NULL;
  gs_conn* conn = g_new0(gs_conn, 1);
  conn->pg = pg;
  conn->dsn = g_strdup(dsn);
  return conn;
}

void gs_disconnect(gs_conn* conn)
{
  if (conn == NULL)
    return;
  PQfinish(conn->pg);
  g_free(conn->dsn);
  g_free(conn);
}

const char* gs_get_error(gs_conn* conn)
{
  if (conn == NULL)
    return "conn is null";
  return conn->error;
}

int gs_begin(gs_conn* conn)
{
  if (conn == NULL)
    return -1;
  PGresult* res = PQexec(conn->pg, "BEGIN");
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return 0;
  }
  conn->error = PQresultErrorMessage(res);
  PQclear(res);
  return -1;
}

int gs_commit(gs_conn* conn)
{
  if (conn == NULL)
    return -1;
  PGresult* res = PQexec(conn->pg, "COMMIT");
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return 0;
  }
  conn->error = PQresultErrorMessage(res);
  PQclear(res);
  return -1;
}

int gs_rollback(gs_conn* conn)
{
  if (conn == NULL)
    return -1;
  PGresult* res = PQexec(conn->pg, "ROLLBACK");
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return 0;
  }
  conn->error = PQresultErrorMessage(res);
  PQclear(res);
  return -1;
}

gs_query* gs_query_new(gs_conn* conn, const char* sql_string)
{
  if (conn == NULL || conn->error != NULL)
    return NULL;
  gs_query* query = g_new0(gs_query, 1);
  query->conn = conn;
  query->sql = g_strdup(sql_string);
  return query;
}

int gs_query_put(gs_query* query, const char* fmt, ...)
{
  if (query == NULL || query->conn->error != NULL)
    return -1;

  int param_count = (fmt != NULL) ? strlen(fmt) : 0;
  char** param_values = g_new0(char*, param_count);
  int* free_list = g_new0(int, param_count);
  int i, retval = 0;
  va_list ap;

  va_start(ap, fmt);
  for (i=0; i<param_count; i++)
  {
    if (fmt[i] == 's')
    {
      param_values[i] = (char*)va_arg(ap, char*);
    }
    else if (fmt[i] == 'i')
    {
      param_values[i] = g_strdup_printf("%d", (int)va_arg(ap, int));
      free_list[i] = 1;
    }
    else if (fmt[i] == 'n')
    {
    }
    else
    {
    }
  }
  va_end(ap);

  if (query->pg_res != NULL)
    PQclear(query->pg_res);

  query->pg_res = PQexecParams(query->conn->pg, query->sql, param_count, NULL, param_values, NULL, NULL, 0);
  if (PQresultStatus(query->pg_res) != PGRES_COMMAND_OK
      && PQresultStatus(query->pg_res) != PGRES_TUPLES_OK)
  {
    query->conn->error = PQresultErrorMessage(query->pg_res);
    retval = -1;
  }
  
  for (i=0; i<param_count; i++)
    if (free_list[i])
      g_free(param_values[i]);
  g_free(param_values);
  g_free(free_list);

  return retval;
}

void gs_query_free(gs_query* query)
{
  if (query == NULL)
    return;
  if (query->pg_res != NULL)
    PQclear(query->pg_res);
  g_free(query->sql);
  g_free(query);
}

int gs_query_get(gs_query* query, const char* fmt, ...)
{
  if (query == NULL || query->conn->error != NULL || query->pg_res == NULL)
    return -1;

  if (query->row_no >= PQntuples(query->pg_res))
    return 1;

  int param_count = fmt != NULL ? strlen(fmt) : 0;
  va_list ap;
  int i;

  va_start(ap, fmt);
  for (i=0; i<param_count; i++)
  {
    if (fmt[i] == 's')
    {
      char** str_ptr = (char**)va_arg(ap, char**);
      if (PQgetisnull(query->pg_res, query->row_no, i))
        *str_ptr = NULL;
      else
        *str_ptr = PQgetvalue(query->pg_res, query->row_no, i);
    }
    else if (fmt[i] == 'S')
    {
      char** str_ptr = (char**)va_arg(ap, char**);
      if (PQgetisnull(query->pg_res, query->row_no, i))
        *str_ptr = NULL;
      else
        *str_ptr = g_strdup(PQgetvalue(query->pg_res, query->row_no, i));
    }
    else if (fmt[i] == 'i')
    {
      int* int_ptr = (int*)va_arg(ap, int*);
      if (!PQgetisnull(query->pg_res, query->row_no, i))
        *int_ptr = atoi(PQgetvalue(query->pg_res, query->row_no, i));
    }
    else if (fmt[i] == 'n')
    {
    }
    else
    {
    }
  }
  va_end(ap);

  query->row_no++;
  return 0;
}

int gs_query_get_rows(gs_query* query)
{
  if (query == NULL)
    return 0;
  return PQntuples(query->pg_res);
}

int gs_query_get_num(gs_query* query)
{
  return 0;
}

int gs_query_get_last_id(gs_query* query)
{
  return 0;
}
