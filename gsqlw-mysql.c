/*
 * Glib sql wrapper.
 *
 * Copyright (C) 2008-2010 Zonio s.r.o <developers@zonio.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <my_global.h>
#include "gsqlw-priv.h"

struct _gs_conn_mysql
{
  gs_conn base;
  MYSQL* handle;
};

enum _sqlite_query_state
{
  QUERY_STATE_INIT,
  QUERY_STATE_ROW_PENDING,
  QUERY_STATE_ROW_READ,
  QUERY_STATE_COMPLETED,
};

struct _gs_query_mysql
{
    gs_query base;
    MYSQL_STMT *stmt;
    int state;
    int row_no;
    
    MYSQL_BIND *bind;
    my_bool *my_null;
    my_bool *error;
    unsigned long *length;
};

#define CONN(c) ((struct _gs_conn_mysql*)(c))
#define QUERY(c) ((struct _gs_query_mysql*)(c))

void gs_error_set( gs_conn *conn, int code, const char *str)
{
    printf ("ERROR: %s\n", str);
}


static gs_conn* mysql_gs_connect(const char *dsn)
{
    struct _gs_conn_mysql* conn;
    
    conn = g_new0(struct _gs_conn_mysql, 1);
    conn->handle = mysql_init(NULL);
    if (conn->handle == NULL)
    {
        gs_set_error((gs_conn*)conn, GS_ERR_OTHER, "mysql_init error.\n");
    }
    char ** dsn_chunks = g_strsplit(dsn, ";", 4);
    if (dsn_chunks == NULL)
    {
        gs_error_set((gs_conn*)conn, GS_ERR_OTHER, "Wrong DSN format\n");
        g_free(conn);
        return NULL;
    }
    conn->handle = mysql_real_connect(conn->handle, "localhost", "mysql", "heslo", "test", 0, NULL, 0);
    g_strfreev(dsn_chunks);
    if (conn->handle == NULL)
    {
        gs_set_error((gs_conn*)conn, GS_ERR_OTHER, mysql_error(conn->handle));
    }
    mysql_autocommit(conn->handle, 0);
    
    return (gs_conn *)conn;
}

static void mysql_gs_disconnect(gs_conn *conn)
{
    if (CONN(conn) != NULL)
    {
        mysql_close(CONN(conn)->handle);
    }
}

static int mysql_gs_begin(gs_conn* conn)
{
    return gs_exec(conn, "BEGIN", NULL);
}

static int mysql_gs_commit(gs_conn* conn)
{
    return gs_exec(conn, "COMMIT", NULL);
}

static int mysql_gs_rollback(gs_conn* conn)
{
    return gs_exec(conn, "ROLLBACK", NULL);
}

char* _mysql_fixup_sql(const char* str)
{
    char* tmp = g_new0(char, strlen(str));
    gboolean in_string = FALSE;
    guint i, j;
    
    for (i = 0, j = 0; i < strlen(str); i++)
    {
        if (! g_ascii_isdigit(str[i]))
        {
            tmp[j] = str[i];
            j++;
        }
    }
    
    if (tmp == NULL)
        return NULL;
    
    for (i = 0; i < strlen(tmp); i++)
    {
        if (tmp[i] == '\'')
            in_string = !in_string;
        if (!in_string && tmp[i] == '$')
            tmp[i] = '?';
    }
    
    return tmp;
}

static void mysql_gs_query_free(gs_query* query);

static gs_query* mysql_gs_query_new(gs_conn* conn, const char* sql_string)
{
    struct _gs_query_mysql* query;
    
    query = g_new0(struct _gs_query_mysql, 1);
    query->base.conn = conn;
    // XXX change SQL string to be compatible with mysql API
    query->base.sql = _mysql_fixup_sql(sql_string);
    query->state = QUERY_STATE_INIT;
    
    query->stmt = mysql_stmt_init(CONN(conn)->handle);
    if (query->stmt == NULL)
    {
        gs_set_error(conn, GS_ERR_OTHER, "mysql_stmt_init() error: out of memory\n");
        mysql_gs_query_free((gs_query*)query);
        return NULL;
    }
    if (mysql_stmt_prepare(query->stmt, query->base.sql, strlen(query->base.sql)) != 0)
    {
        gs_set_error(conn, GS_ERR_OTHER, mysql_stmt_error(query->stmt));
        mysql_gs_query_free((gs_query*)query);
        return NULL;
    }
    
    return (gs_query*)query;
}

static void mysql_gs_query_free(gs_query* query)
{
    if (QUERY(query)->stmt != NULL)
    {
        mysql_stmt_close(QUERY(query)->stmt);
    }
    g_free(query->sql);
    g_free(query);
}

static int mysql_gs_query_get_rows(gs_query* query)
{
    return mysql_stmt_affected_rows(QUERY(query)->stmt);
}

static void _mysql_free_stmt_vars(gs_query *query);

static int mysql_gs_query_getv(gs_query* query, const char* fmt, va_list ap)
{
    if (QUERY(query)->state == QUERY_STATE_INIT)
    {
        gs_set_error(query->conn, GS_ERR_OTHER, "Invalid API use, call gs_query_put() before gs_query_get().");
        return -1;
    }
    if (QUERY(query)->state == QUERY_STATE_COMPLETED)
    {
        return 1;
    }
    MYSQL_STMT* stmt = QUERY(query)->stmt;
    int param_count = fmt != NULL ? strlen(fmt) : 0;
    int i;
    int col = 0;
    
    if (QUERY(query)->state == QUERY_STATE_ROW_PENDING)
    {
        QUERY(query)->bind = g_new0(MYSQL_BIND, param_count);
        QUERY(query)->my_null = g_new0(my_bool, param_count);
        QUERY(query)->error = g_new0(my_bool, param_count);
        QUERY(query)->length = g_new(unsigned long, param_count);

        for (i = 0; i < param_count; i++)
        {
            if (fmt[i] == 's')
            {
                char** str_ptr = (char**)va_arg(ap, char**);
                QUERY(query)->bind[i].buffer_type = MYSQL_TYPE_STRING;
                QUERY(query)->bind[i].buffer = (char *)*str_ptr;
                QUERY(query)->bind[i].is_null = &QUERY(query)->my_null[i];
                QUERY(query)->bind[i].length = &QUERY(query)->length[i];
                QUERY(query)->bind[i].error = &QUERY(query)->error[i];
                col++;
            }
            else if (fmt[i] == 'S')
            {
                char** str_ptr = (char**)va_arg(ap, char**);
                QUERY(query)->bind[i].buffer_type = MYSQL_TYPE_STRING;
                QUERY(query)->bind[i].buffer = (char *)*str_ptr;
                QUERY(query)->bind[i].is_null = &QUERY(query)->my_null[i];
                QUERY(query)->bind[i].length = &QUERY(query)->length[i];
                QUERY(query)->bind[i].error = &QUERY(query)->error[i];
                col++;
            }
            else if (fmt[i] == '?' && fmt[i+1] == 'i') // null flag
            {
                int* null_ptr = (int*)va_arg(ap, int*);
                i++;
                int* int_ptr = (int*)va_arg(ap, int*);
                QUERY(query)->bind[i].buffer_type = MYSQL_TYPE_LONG;
                QUERY(query)->bind[i].buffer = (int *)int_ptr;
                QUERY(query)->bind[i].is_null = (my_bool *)null_ptr;
                QUERY(query)->bind[i].is_null = &QUERY(query)->my_null[i];
                QUERY(query)->bind[i].length = &QUERY(query)->length[i];
                QUERY(query)->bind[i].error = &QUERY(query)->error[i];
                
            }
            else if (fmt[i] == 'i')
            {
                int* int_ptr = (int*)va_arg(ap, int*);
                QUERY(query)->bind[i].buffer_type = MYSQL_TYPE_LONG;
                QUERY(query)->bind[i].buffer = (int *)int_ptr;
                QUERY(query)->bind[i].is_null = &QUERY(query)->my_null[i];
                QUERY(query)->bind[i].length = &QUERY(query)->length[i];
                QUERY(query)->bind[i].error = &QUERY(query)->error[i];
                col++;
            }
            else
            {
                gs_set_error(query->conn, GS_ERR_OTHER, "Invalid format string.");
                va_end(ap);
                _mysql_free_stmt_vars(query);
                return -1;
            }
        }
        if (mysql_stmt_bind_result(stmt, QUERY(query)->bind) != 0)
        {
            gs_error_set(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
            _mysql_free_stmt_vars(query);
            return -1;
        }
        if (mysql_stmt_store_result(stmt) != 0)
        {
            gs_error_set(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
            _mysql_free_stmt_vars(query);
            return -1;
        }
        QUERY(query)->state = QUERY_STATE_ROW_READ;
    }
    
    int res = mysql_stmt_fetch(stmt);
    switch (res)
    {
        case 0: break; /* success */
        case 1: /* error */
        {
            gs_error_set(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
            _mysql_free_stmt_vars(query);
            return -1;
        }
        case MYSQL_NO_DATA: /* No more rows/data exists */
        {
            _mysql_free_stmt_vars(query);
            QUERY(query)->state = QUERY_STATE_COMPLETED;
            return 1;
        }
    }
    
    return 0;
}

static void _mysql_free_stmt_vars(gs_query *query)
{
    g_free(QUERY(query)->bind);
    g_free(QUERY(query)->my_null);
    g_free(QUERY(query)->error);
    g_free(QUERY(query)->length);
    QUERY(query)->bind = NULL;
    QUERY(query)->my_null = NULL;
    QUERY(query)->error = NULL;
    QUERY(query)->length = NULL;
}


static int mysql_gs_query_putv(gs_query* query, const char* fmt, va_list ap)
{
    MYSQL_STMT* stmt = QUERY(query)->stmt;
    int param_count = (fmt != NULL) ? strlen(fmt) : 0;
    int sub_count = mysql_stmt_param_count(stmt);
    int i, j;
    my_bool my_null;
    MYSQL_BIND *bind = g_new0(MYSQL_BIND, sub_count);
    long unsigned *lengths = g_new0(long unsigned, sub_count);
    
    for (i = 0, j = 0; i < param_count; i++, j++)
    {
        int is_null = 0;
        
        if (fmt[i] == '?')
        {
            i++;
            is_null = (int)va_arg(ap, int);
            if (is_null)
            {
                bind[j].buffer_type = MYSQL_TYPE_NULL;
                continue;
            }
        }
        
        if (fmt[i] == 's')
        {
            char* value = (char*)va_arg(ap, char*);
            bind[j].buffer_type = MYSQL_TYPE_STRING;
            bind[j].buffer = (char *) value;
            bind[j].buffer_length = strlen(value);
            lengths[j] = strlen(value);
            bind[j].length = &lengths[j];
            bind[j].is_null = (my_bool *) 0;
        }
        else if (fmt[i] == 'i')
        {
            int value = (int)va_arg(ap, int);
            bind[j].buffer_type = MYSQL_TYPE_LONG;
            bind[j].buffer = (char *)&value;
            bind[j].length = 0;
            bind[j].is_null = (my_bool *) 0;
        }
        else
        {
            gs_set_error(query->conn, GS_ERR_OTHER, "Invalid format string.");
            g_free(lengths);
            g_free(bind);
            return -1;
        }
    }
    
    if (mysql_stmt_bind_param(stmt, bind) != 0)
    {
        gs_error_set(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
        g_free(lengths);
        g_free(bind);
        return -1;
    }
    if (mysql_stmt_execute(stmt) != 0)
    {
        gs_error_set(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
        g_free(lengths);
        g_free(bind);
        return -1;
    }
    
    g_free(lengths);
    g_free(bind);
    return 0;
}

static int mysql_gs_query_get_last_id(gs_query* query, const char* seq_name)
{
    int id;
    
    // XXX
    
    return id;
}




gs_driver mysql_driver =
{
  .name = "mysql",
  .connect = mysql_gs_connect,
  .disconnect = mysql_gs_disconnect,
  .begin = mysql_gs_begin,
  .commit = mysql_gs_commit,
  .rollback = mysql_gs_rollback,
  .query_new = mysql_gs_query_new,
  .query_free = mysql_gs_query_free,
  .query_getv = mysql_gs_query_getv,
  .query_putv = mysql_gs_query_putv,
  .query_get_rows = mysql_gs_query_get_rows,
  .query_get_last_id = mysql_gs_query_get_last_id,
};
