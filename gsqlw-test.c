#include <stdio.h>
#include "gsqlw.h"

#define DSN "pgsql:dbname=test host=localhost user=postgres password=heslo"

int gs_finish(gs_conn* c)
{
  if (gs_get_error(c))
  {
    g_print("ERROR: %s\n", gs_get_error(c));
    gs_rollback(c);
    return -1;
  }
  gs_commit(c);
  return 0;
}

static gpointer _thread_func(gpointer data)
{
  int id_val;
  char* str_val;
  gs_query* q;
  gs_conn* c;
  
  c = gs_connect(DSN);
  gs_begin(c);

  q = gs_query_new(c, "INSERT INTO test (id, name) VALUES ($1, $2)");
  gs_query_put(q, "is", 1, "test 1");
  gs_query_put(q, "is", 2, "test 2");
  gs_query_put(q, "is", 3, "test 3");
  gs_query_put(q, "is", 4, "test 4''");
  gs_query_free(q);

  q = gs_query_new(c, "SELECT id, name FROM test WHERE id = $1");
  gs_query_put(q, "i", 1);
  printf("get data:\n");
  while (gs_query_get(q, "is", &id_val, &str_val) == 0)
    printf("  getting %s %d\n", str_val, id_val);
  gs_query_free(q);

  gs_finish(c);
  gs_disconnect(c);

  return NULL;
}

int main(int ac, char* av[])
{
  GThread* t[1024];
  int count = 10, i;
  gs_conn* c;

  if (!g_thread_supported())
    g_thread_init(NULL);

  c = gs_connect(DSN);
  gs_begin(c);

  //gs_exec(c, "DROP TABLE test;");
  gs_exec(c, "CREATE TABLE test (id INT, name TEXT)", NULL);
  gs_exec(c, "INSERT INTO test (id, name) VALUES ($1, $2)", "is", 10, "blalbla");

  gs_finish(c);
  gs_disconnect(c);

  for (i=0; i<count; i++)
    t[i] = g_thread_create(_thread_func, (gpointer)i, TRUE, NULL);

  for (i=0; i<count; i++)
    if (t[i])
      g_thread_join(t[i]);

  return 0;
}
