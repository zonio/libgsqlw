#ifndef __GSQLDRIVER_H__
#define __GSQLDRIVER_H__

#include <glib.h>

/** @file gsqldriver Header
 */

typedef struct _gs_driver gs_driver;
typedef struct _gs_dsn gs_dsn;

struct _gs_driver
{
  char* name;
  // driver methods
  int (*connect)(gs_conn* c, gs_dsn* dsn);
  int (*disconnect)(gs_conn* c);
};

struct _gs_dsn
{
  char* dbtype;
  char* dbname;
};

extern gs_driver* _drivers_list[];

gs_dsn* gs_dsn_new(const char* dsn_string);
gs_driver* gs_driver_get(const char* name);
void gs_dsn_free(gs_dsn* dsn);

#endif
