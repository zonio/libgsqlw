#include "gsqlw.h"

int main()
{
  int id_val;
  char* str_val;
  gs_query* q;
  gs_conn* c;
  
  c = gs_connect("dbname=test host=localhost user=postgres password=heslo");
  gs_begin(c);

  q = gs_query_new(c, "DROP TABLE test;");
  gs_query_put(q, NULL);
  gs_query_free(q);

  q = gs_query_new(c, "CREATE TABLE test (id INT, name TEXT)");
  gs_query_put(q, NULL);
  gs_query_free(q);

  q = gs_query_new(c, "INSERT INTO test (id, name) VALUES ($1, $2)");
  gs_query_put(q, "is", 1, "test 1");
  gs_query_put(q, "is", 2, "test 2");
  gs_query_put(q, "is", 3, "test 3");
  gs_query_put(q, "is", 4, "test 4''");
  gs_query_free(q);

  q = gs_query_new(c, "SELECT id, name FROM test");
  gs_query_put(q, NULL);
  while (gs_query_get(q, "is", &id_val, &str_val) == 0)
    printf("getting %s %d\n", str_val, id_val);
  gs_query_free(q);

  if (gs_get_error(c))
  {
    printf("%s", gs_get_error(c));
    gs_rollback(c);
  }
  else
    gs_commit(c);
  gs_disconnect(c);
}
