#include "gsqlwrapper.h"

static void test1()
{
  gs_query* q;
  gs_conn* c;
  
  c = gs_connect("pgsql8:database=test", "postgres", "heslo");

  // simple direct query
  q = gs_query_new(c, "INSERT INTO test (id, name) VALUES (1, 'name')");
  gs_query_free(q);

  // custom execution query
  q = gs_query_new(c, "INSERT INTO test (id, name) VALUES (?, ?)");
  gs_query_put(q, "IS", 1, "test");
  gs_query_put(q, "IS", 2, "test2");
  gs_query_free(q);

  // select query
  q = gs_query_new(c, "SELECT * FROM test");
  int id_val;
  char* str_val;
  gs_query_get(q, "IS", &id_val, &str_val);
  gs_query_free(q);

  gs_disconnect(c);
}

static void test2()
{
  gs_conn* c = gs_connect("pgsql8:database=test", "postgres", "heslo");
  gs_disconnect(c);
}

int main()
{
  gs_init();

  test1();
  test2();

  gs_fini();
}
