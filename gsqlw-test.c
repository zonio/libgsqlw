/*
 * Glib sql wrapper.
 *
 * Copyright (C) 2008-2010 Zonio s.r.o <developers@zonio.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <unistd.h>

#include <config.h>
#include "gsqlw.h"

#ifdef HAVE_MYSQL
# define DSN "mysql:dbname=test host=localhost user=user password=heslo"
#elif HAVE_POSTGRES
# define DSN "pgsql:dbname=test host=localhost user=postgres password=heslo"
#elif HAVE_SQLITE
# define DSN "sqlite:.test.db"
#endif

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
  //g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_put(q, "?is", TRUE, 2, "test 2");
  //g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_put(q, "?is", FALSE, 3, "test 3");
  //g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_put(q, "is", 4, "test 4''");
  //g_print("last insert ID: %d\n", gs_query_get_last_id(q, NULL));
  gs_query_free(q);
}

/** gs_query: repeated selects
 */
void test3()
{
  int id_val;
  int id_null;
  char* str_val = NULL;

  q = gs_query_new(c, "SELECT id, name FROM test WHERE id > $1");

  gs_query_put(q, "i", 1);
  while (gs_query_get(q, "is", &id_val, &str_val) == 0)
    g_print("  getting row: '%s' %d\n", str_val, id_val);

  gs_query_put(q, "i", 3);
  while (gs_query_get(q, "?iS", &id_null, &id_val, &str_val) == 0)
  {
    g_print("  getting row: '%s' %d (%s)\n", str_val, id_val, id_null ? "IS NULL" : "IS NOT NULL");
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
    g_print("  getting row: '%s' %d\n", str_val, id_val);

  gs_query_free(q);
}

/** cosntraint violation tests
 */
void test5()
{
  gs_exec(c, "CREATE TABLE ct (id INT UNIQUE, name TEXT NOT NULL)", NULL);
  gs_exec(c, "INSERT INTO ct (id, name) VALUES ($1, $2)", "is", 1, NULL);
  if (gs_get_errcode(c) != GS_ERR_NOT_NULL_VIOLATION)
    g_print("ASSERT FAILED: should return GS_ERR_NOT_NULL_VIOLATION (%d:%s)\n", gs_get_errcode(c), gs_get_errmsg(c));
}

void test6()
{
  gs_exec(c, "CREATE TABLE ct (id INT UNIQUE, name TEXT NOT NULL)", NULL);
  gs_exec(c, "INSERT INTO ct (id, name) VALUES ($1, $2)", "is", 1, "text 1");
  gs_exec(c, "INSERT INTO ct (id, name) VALUES ($1, $2)", "is", 1, "text 2");
  if (gs_get_errcode(c) != GS_ERR_UNIQUE_VIOLATION)
    g_print("ASSERT FAILED: should return GS_ERR_UNIQUE_VIOLATION (%d:%s)\n", gs_get_errcode(c), gs_get_errmsg(c));
}

void test7()
{
    gs_exec(c, "CREATE TABLE test2 (id INT UNIQUE, name1 TEXT NOT NULL, name2 TEXT NOT NULL)", NULL);
    gs_exec(c, "INSERT INTO test2 (id, name1, name2) VALUES ($1, $2, $2)", "is", 1, "hello");
    gs_exec(c, "INSERT INTO test2 (id, name1, name2) VALUES ($1, $2, $2)", "is", 2, "hello again");
    gs_exec(c, "INSERT INTO test2 (id, name1, name2) VALUES ($2, $1, $1)", "si", "hola", 3);
}

int main(int ac, char* av[])
{
  guint i;

  if (!g_thread_supported())
    g_thread_init(NULL);

  unlink(".test.db");

  void (*tests[])() = {
    test1,
    test2,
    test3,
    test4,
    test5,
    test6,
    test7,
  };

  for (i = 0; i < G_N_ELEMENTS(tests); i++)
  {
    c = gs_connect(DSN);
    if (gs_get_errcode(c) != GS_ERR_NONE)
    {
      g_print("ERROR: %s\n", gs_get_errmsg(c));
      gs_disconnect(c);
      break;
    }
    gs_begin(c);

    tests[i]();

    if (gs_finish(c) < 0)
      g_print("ERROR: %s\n", gs_get_errmsg(c));
    gs_disconnect(c);
  }

  return 0;
}
