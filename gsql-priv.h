#ifndef __GSQL_PRIV_H__
#define __GSQL_PRIV_H__

#include <glib.h>

/** @file gsql_priv Header
 */

#include "gsqldriver.h"

struct _gs_conn
{
  gs_dsn* dsn;
  gs_driver* driver;
  void* priv;
};

struct _gs_query
{
  gs_conn* conn;
  char* sql;
  void* priv;
};


#endif
