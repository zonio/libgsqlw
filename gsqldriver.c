#include "gsqldriver.h"
#include "gsql-priv.h"

gs_driver* _drivers_list[] =
{
  pgsql8_driver,
  sqlite3_driver,
  NULL
};

gs_dsn* gs_dsn_new(const char* dsn_string)
{
}

void gs_dsn_free(gs_dsn* dsn)
{
}

gs_driver* gs_driver_get(const char* name)
{
}
