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

#include "gsqlw-priv.h"

struct _gs_conn_mysql
{
    gs_conn base;
    MYSQL* handle;
    int max_col_len; /* Max length of column text value */
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
    my_ulonglong row_no;
    
    MYSQL_BIND *bind;
    my_bool *my_null;
    my_bool *error;
    unsigned long *length;
    int **val_is_null; /* which columns contain NULL values */
    int *mem_aloc;     /* which columns need memory allocation */
    char **str;        /* memory pointers to string values */
};

#define CONN(c) ((struct _gs_conn_mysql*)(c))
#define QUERY(c) ((struct _gs_query_mysql*)(c))

static int _mysql_prepare_stmt_vars(gs_query* query, const char* fmt, va_list ap, int col_count);
static void _mysql_free_stmt_vars(gs_query *query);
static void mysql_gs_query_free(gs_query* query);
static int _mysql_stmt_fetch_prepare(gs_query *query, int col_count);

/*
 * Parse DSN from key-value format to array of values
 * in order used by mysql_real_connect(..) function.
 */
static char** _mysql_parse_dsn(const char *dsn)
{
    /* default values initialization */
    char *host = g_strdup("localhost");
    char *port = g_strdup("0");
    char *dbname = g_strdup("mysql");
    char *user = g_strdup("mysql");
    char *password = g_strdup("mysql");
    char *textlen = g_strdup("1024");
    
    char **keyvals = g_strsplit(dsn, " ", -1);

    int i;
    for (i = 0; keyvals[i]; i++)
    {
        char *valptr = strchr(keyvals[i], '=');
        int keylen = valptr - keyvals[i];
        
        if (strncmp(keyvals[i], "host", keylen) ==  0)
        {
            g_free(host);
            host = g_strdup(keyvals[i]+keylen+1);
            continue;
        }
        if (strncmp(keyvals[i], "port", keylen) ==  0)
        {
            g_free(port);
            port = g_strdup((keyvals[i])+keylen+1);
            continue;
        }
        if (strncmp(keyvals[i], "dbname", keylen) ==  0)
        {
            g_free(dbname);
            dbname = g_strdup((keyvals[i])+keylen+1);
            continue;
        }
        if (strncmp(keyvals[i], "user", keylen) ==  0)
        {
            g_free(user);
            user = g_strdup((keyvals[i])+keylen+1);
            continue;
        }
        if (strncmp(keyvals[i], "password", keylen) ==  0)
        {
            g_free(password);
            password = g_strdup((keyvals[i])+keylen+1);
            continue;
        }
        if (strncmp(keyvals[i], "textlen", keylen) ==  0)
        {
            g_free(textlen);
            textlen = g_strdup((keyvals[i])+keylen+1);
            continue;
        }
        
        /* unknown key or wrong dsn format */
        g_free(host);
        g_free(user);
        g_free(password);
        g_free(dbname);
        g_free(port);
        g_free(textlen);
        g_strfreev(keyvals);
        return NULL;
    }
    
    char **dsn_chunks = g_new0(char *, 7);
    dsn_chunks[0] = host;
    dsn_chunks[1] = user;
    dsn_chunks[2] = password;
    dsn_chunks[3] = dbname;
    dsn_chunks[4] = port;
    dsn_chunks[5] = textlen;
    g_strfreev(keyvals);
    
    return dsn_chunks;
}

static gs_conn* mysql_gs_connect(const char *dsn)
{
    struct _gs_conn_mysql* conn;
    
    conn = g_new0(struct _gs_conn_mysql, 1);
    conn->handle = mysql_init(NULL);
    if (conn->handle == NULL)
    {
        gs_set_error((gs_conn*)conn, GS_ERR_OTHER, "mysql_init error");
        g_free(conn);
        return NULL;
    }
    
    char **dsn_chunks = _mysql_parse_dsn(dsn);
    if (dsn_chunks == NULL)
    {
        gs_set_error((gs_conn*)conn, GS_ERR_OTHER, "Wrong DSN format");
        g_free(conn);
        return NULL;
    }

    conn->handle = mysql_real_connect(conn->handle, dsn_chunks[0],
                                      dsn_chunks[1], dsn_chunks[2],
                                      dsn_chunks[3], atoi(dsn_chunks[4]),
                                      NULL, 0);
    conn->max_col_len = atoi(dsn_chunks[5]);
    g_strfreev(dsn_chunks);
    if (conn->handle == NULL)
    {
        gs_set_error((gs_conn*)conn, GS_ERR_OTHER, mysql_error(conn->handle));
        g_free(conn);
        return NULL;
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
    /* 
     * MySQL doesn't support BEGIN in prepared statements.
     * Nevertheless, autocommit is off and COMMIT and
     * ROLLBACK are ok to use.
     */
    
    //return gs_exec(conn, "BEGIN", NULL);
    return 0; /* success */
}

static int mysql_gs_commit(gs_conn* conn)
{
    return gs_exec(conn, "COMMIT", NULL);
}

static int mysql_gs_rollback(gs_conn* conn)
{
    return gs_exec(conn, "ROLLBACK", NULL);
}

/*
 * Converts SQL to mysql prepared statement format.
 * Variables in query ($1, $2, ...) replaces with '?'.
 */
char* _mysql_fixup_sql(const char* str)
{
    char* tmp = g_new0(char, strlen(str)+1); /* +1 for \0 character */
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

static gs_query* mysql_gs_query_new(gs_conn* conn, const char* sql_string)
{
    struct _gs_query_mysql* query;
    
    query = g_new0(struct _gs_query_mysql, 1);
    query->base.conn = conn;
    query->base.sql = _mysql_fixup_sql(sql_string);
    
    query->stmt = mysql_stmt_init(CONN(conn)->handle);
    if (query->stmt == NULL)
    {
        gs_set_error(conn, GS_ERR_OTHER, "mysql_stmt_init() error: out of memory");
        mysql_gs_query_free((gs_query*)query);
        return NULL;
    }
    if (mysql_stmt_prepare(query->stmt, query->base.sql, strlen(query->base.sql)) != 0)
    {
        gs_set_error(conn, GS_ERR_OTHER, mysql_stmt_error(query->stmt));
        mysql_gs_query_free((gs_query*)query);
        return NULL;
    }
    query->state = QUERY_STATE_INIT;
    
    return (gs_query*)query;
}

static void mysql_gs_query_free(gs_query* query)
{
    if (QUERY(query)->stmt != NULL)
    {
        mysql_stmt_close(QUERY(query)->stmt);
        QUERY(query)->stmt = NULL;
    }
    if (QUERY(query)->bind != NULL)
    {
        _mysql_free_stmt_vars(query);
    }
    g_free(query->sql);
    g_free(query);
}

static int mysql_gs_query_get_rows(gs_query* query)
{
    /* casting from my_ulonglong to int, problem with greater values */
    return (int)QUERY(query)->row_no;
}

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
    int col_count = mysql_stmt_field_count(stmt);
    int i;
    
    if (QUERY(query)->state == QUERY_STATE_ROW_PENDING)
    {
        int ret = _mysql_prepare_stmt_vars(query, fmt, ap, col_count);
        if (ret != 0)
        {
            return ret;
        }
    }
    for (i = 0; i < col_count; i++)
    {
        if (QUERY(query)->val_is_null[i] != NULL)
        {
            *QUERY(query)->val_is_null[i]= 0;
        }
        if (QUERY(query)->mem_aloc[i])
        {
            *QUERY(query)->str = g_new0(char, CONN(query->conn)->max_col_len);
            QUERY(query)->bind[i].buffer = *QUERY(query)->str;
        }
    }
    
    int ret = _mysql_stmt_fetch_prepare(query, col_count);
    if (ret != 0)
    {
        return ret;
    }
    int res = mysql_stmt_fetch(stmt);
    
    switch (res)
    {
        case 0: /* success */
        {
            for (i = 0; i < col_count; i++)
            {
                if (QUERY(query)->my_null[i] && (QUERY(query)->val_is_null[i] != NULL))
                {
                    *(QUERY(query)->val_is_null[i]) = 1;
                }
            }
            break;
        }
        case 1: /* error */
        {
            gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
            _mysql_free_stmt_vars(query);
            return -1;
        }
        case MYSQL_NO_DATA: /* No more rows/data exists */
        {
            QUERY(query)->state = QUERY_STATE_COMPLETED;
            _mysql_free_stmt_vars(query);
            return 1;
        }
        case MYSQL_DATA_TRUNCATED:
        {
            gs_set_error(query->conn, GS_ERR_OTHER, "mysql error: buffer size is small. You probably want to increase buffer size (DSN parameter textlen).");
            _mysql_free_stmt_vars(query);
            return -1;
        }
        default:
        {
            gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
            _mysql_free_stmt_vars(query);
            return -1;
        }
    }
    
    return 0;
}

/*
 * Binds variables with columns.
 */
static int _mysql_prepare_stmt_vars(gs_query* query, const char* fmt, va_list ap, int col_count)
{
    MYSQL_STMT* stmt = QUERY(query)->stmt;
    int param_count = fmt != NULL ? strlen(fmt) : 0;
    
    QUERY(query)->bind = g_new0(MYSQL_BIND, col_count);
    QUERY(query)->my_null = g_new0(my_bool, col_count);
    QUERY(query)->error = g_new0(my_bool, col_count);
    QUERY(query)->length = g_new0(unsigned long, col_count);
    QUERY(query)->val_is_null = g_new0(int *, col_count);
    QUERY(query)->mem_aloc = g_new0(int *, col_count);
    MYSQL_BIND *bind = QUERY(query)->bind;
    int i, col = 0;

    for (i = 0; i < param_count; i++)
    {
        if (fmt[i] == 's')
        {
            char** str_ptr = (char**)va_arg(ap, char**);
            *str_ptr = g_new0(char, CONN(query->conn)->max_col_len);
            bind[col].buffer_type = MYSQL_TYPE_STRING;
            bind[col].buffer = (char *)*str_ptr;
            bind[col].buffer_length = CONN(query->conn)->max_col_len;
            bind[col].is_null = &QUERY(query)->my_null[col];
            bind[col].length = &QUERY(query)->length[col];
            bind[col].error = &QUERY(query)->error[col];
            col++;
        }
        else if (fmt[i] == 'S')
        {
            QUERY(query)->str = (char**)va_arg(ap, char**);
            bind[col].buffer_type = MYSQL_TYPE_STRING;
            bind[col].buffer_length = CONN(query->conn)->max_col_len;
            bind[col].is_null = &QUERY(query)->my_null[col];
            bind[col].length = &QUERY(query)->length[col];
            bind[col].error = &QUERY(query)->error[col];
            QUERY(query)->mem_aloc[col] = 1;
            col++;
        }
        else if (fmt[i] == '?' && i<param_count-1 && fmt[i+1] == 'i')
        {
            QUERY(query)->val_is_null[col] = (int*)va_arg(ap, int*);
        }
        else if (fmt[i] == 'i')
        {
            int* int_ptr = (int*)va_arg(ap, int*);
            bind[col].buffer_type = MYSQL_TYPE_LONG;
            bind[col].buffer = (int *)int_ptr;
            bind[col].is_null = &QUERY(query)->my_null[col];
            bind[col].length = &QUERY(query)->length[col];
            bind[col].error = &QUERY(query)->error[col];
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
    if (mysql_stmt_bind_result(stmt, bind) != 0)
    {
        gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
        _mysql_free_stmt_vars(query);
        return -1;
    }
    if (mysql_stmt_store_result(stmt) != 0)
    {
        gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
        _mysql_free_stmt_vars(query);
        return -1;
    }
    QUERY(query)->row_no = mysql_stmt_num_rows(QUERY(query)->stmt);
    QUERY(query)->state = QUERY_STATE_ROW_READ;
    
    return 0;
}

/*
 * Cleans information about which columns were NULL and allocates
 * new memory for string columns. Must be called before
 * each call of mysql_stmt_fetch().
 */
static int _mysql_stmt_fetch_prepare(gs_query *query, int col_count)
{
    int i;
    for (i = 0; i < col_count; i++)
    {
        if (QUERY(query)->val_is_null[i] != NULL)
        {
            *QUERY(query)->val_is_null[i]= 0;
        }
        if (QUERY(query)->mem_aloc[i])
        {
            *QUERY(query)->str = g_new0(char, CONN(query->conn)->max_col_len);
            QUERY(query)->bind[i].buffer = *QUERY(query)->str;
        }
    }
    if (mysql_stmt_bind_result(QUERY(query)->stmt, QUERY(query)->bind) != 0)
    {
        gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(QUERY(query)->stmt));
        _mysql_free_stmt_vars(query);
        return -1;
    }
    
    return 0;
}

static void _mysql_free_stmt_vars(gs_query *query)
{
    g_free(QUERY(query)->bind);
    g_free(QUERY(query)->my_null);
    g_free(QUERY(query)->error);
    g_free(QUERY(query)->length);
    g_free(QUERY(query)->val_is_null);
    g_free(QUERY(query)->mem_aloc);
    QUERY(query)->bind = NULL;
    QUERY(query)->my_null = NULL;
    QUERY(query)->error = NULL;
    QUERY(query)->length = NULL;
    QUERY(query)->val_is_null = NULL;
    QUERY(query)->mem_aloc = NULL;
}

static int mysql_gs_query_putv(gs_query* query, const char* fmt, va_list ap)
{
    MYSQL_STMT* stmt = QUERY(query)->stmt;
    int param_count = (fmt != NULL) ? strlen(fmt) : 0;
    int col_count = mysql_stmt_param_count(stmt);
    int i, j;
    my_bool my_null;
    MYSQL_BIND *bind = g_new0(MYSQL_BIND, col_count);
    long unsigned *lengths = g_new0(long unsigned, col_count);
    
    for (i = 0, j = 0; i < param_count; i++, j++)
    {
        int is_null = 0;
        
        if (fmt[i] == '?')
        {
            is_null = (int)va_arg(ap, int);
            if (is_null)
            {
                bind[j].buffer_type = MYSQL_TYPE_NULL;
            }
            i++;
        }
        
        if (fmt[i] == 's')
        {
            char* value = (char*)va_arg(ap, char*);
            
            if (value != NULL)
            {
                bind[j].buffer_type = MYSQL_TYPE_STRING;
                bind[j].buffer = (char *) value;
                bind[j].buffer_length = value ? strlen(value) : 0;
                lengths[j] = value ? strlen(value) : 0;
                bind[j].length = &lengths[j];
                bind[j].is_null = (my_bool *) 0;
            }
            else
            {
                bind[j].buffer_type = MYSQL_TYPE_NULL;
            }
        }
        else if (fmt[i] == 'i')
        {
            int value = (int)va_arg(ap, int);
            if (! is_null)
            {
                bind[j].buffer_type = MYSQL_TYPE_LONG;
                bind[j].buffer = (char *)&value;
                bind[j].length = 0;
                bind[j].is_null = (my_bool *) 0;
            }
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
        gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
        g_free(lengths);
        g_free(bind);
        return -1;
    }
    if (mysql_stmt_execute(stmt) != 0)
    {
        unsigned err_code = mysql_stmt_errno(stmt);
        switch (err_code)
        {
            case 1048: /* Column '%s' cannot be null */
            {
                gs_set_error(query->conn, GS_ERR_NOT_NULL_VIOLATION, mysql_stmt_error(stmt));
                break;
            }
            case 1050: /* table exists */
            case 1061: /* Duplicate key name '%s' */
            case 1062: /* Duplicate entry '%s' for key %d */
            {
                gs_set_error(query->conn, GS_ERR_UNIQUE_VIOLATION, mysql_stmt_error(stmt));
                break;
            }
            default:
            {
                gs_set_error(query->conn, GS_ERR_OTHER, mysql_stmt_error(stmt));
            }
        }
        g_free(lengths);
        g_free(bind);
        return -1;
    }
    
    QUERY(query)->state = QUERY_STATE_ROW_PENDING;
    
    g_free(lengths);
    g_free(bind);
    return 0;
}

static int mysql_gs_query_get_last_id(gs_query* query, const char* seq_name)
{
    /*
     * Can be called after at least one call of mysql_gs_query_getv
     * else returns undefined value.
     */
    my_ulonglong id = mysql_insert_id(CONN(query->conn)->handle);
    return (int)id;
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
