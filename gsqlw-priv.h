#ifndef __GSQLW_PRIV_H__
#define __GSQLW_PRIV_H__

#include "gsqlw.h"

typedef struct _gs_driver gs_driver;

struct _gs_conn
{
  char* dsn;
  char* error;
  int error_code;
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

extern gs_driver sqlite_driver;
extern gs_driver pgsql_driver;

#endif
