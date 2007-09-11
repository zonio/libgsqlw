#include <stdio.h>
#include "gsqlw.h"

//#define DSN "pgsql:dbname=test host=localhost user=postgres password=heslo"
#define DSN "sqlite:.test.db"

gs_query* q;
gs_conn* c;

/** gs_exec: create/insert
 */
void test1()
{
  gs_exec(c, "CREATE TABLE test (id INT, name TEXT)", NULL);
  gs_exec(c, "INSERT INTO test (id, name) VALUES ($1, $2)", "is", 10, "blalbla");
}

/** gs_query: repeated inserts
 */
void test2()
{
  q = gs_query_new(c, "INSERT INTO test (id, name) VALUES ($1, $2)");
  gs_query_put(q, "is", 1, "test 1");
  g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_put(q, "is", 2, "test 2");
  g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_put(q, "is", 3, "test 3");
  g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_put(q, "is", 4, "test 4''");
  g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_free(q);
}

/** gs_query: repeated selects
 */
void test3()
{
  int id_val;
  int id_null;
  char* str_val = NULL;

  q = gs_query_new(c, "SELECT id, name FROM test WHERE id = $1");

  gs_query_put(q, "i", 1);
  while (gs_query_get(q, "is", &id_val, &str_val) == 0)
    g_print("  getting row: %s %d\n", str_val, id_val);

  gs_query_put(q, "i", 2);
  while (gs_query_get(q, "?iS", &id_null, &id_val, &str_val) == 0)
  {
    g_print("  getting row: %s %d (%s)\n", str_val, id_val, id_null ? "IS NULL" : "IS NOT NULL");
    g_free(str_val);
    str_val = NULL;
  }

  gs_query_free(q);
}

/** gs_query_* methods
 */
void test4()
{
  int id_val;
  char* str_val = NULL;

  q = gs_query_new(c, "SELECT id, name FROM test WHERE id = $1");

  gs_query_put(q, "i", 1);
  g_print("rows %d\n", gs_query_get_rows(q));
  while (gs_query_get(q, "is", &id_val, &str_val) == 0)
    g_print("  getting row: %s %d\n", str_val, id_val);

  gs_query_free(q);
}

int main(int ac, char* av[])
{
  guint i;

  if (!g_thread_supported())
    g_thread_init(NULL);

  void (*tests[])() = {
    test1,
    test2,
    test3,
    test4,
  };

  for (i = 0; i < G_N_ELEMENTS(tests); i++)
  {
    c = gs_connect(DSN);
    gs_begin(c);

    tests[i]();

    if (gs_finish(c) < 0)
      g_print("ERROR: %s\n", gs_get_error(c));
    gs_disconnect(c);
  }

  return 0;
}
