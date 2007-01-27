#ifndef __GSW_H__
#define __GSW_H__

#include <glib.h>

/** @file gsqlwrapper Header
 */

typedef struct _gs_conn gs_conn;
typedef struct _gs_query gs_query;

/* functions */

gs_conn* gs_connect(const char* dsn);
void gs_disconnect(gs_conn* conn);
const char* gs_get_error(gs_conn* conn);

int gs_begin(gs_conn* conn);
int gs_commit(gs_conn* conn);
int gs_rollback(gs_conn* conn);

gs_query* gs_query_new(gs_conn* conn, const char* sql);
void gs_query_free(gs_query* query);

int gs_query_get(gs_query* query, const char* fmt, ...);
int gs_query_put(gs_query* query, const char* fmt, ...);

int gs_query_get_rows(gs_query* query);
int gs_query_get_num(gs_query* query); /* get first column from the first row */
int gs_query_get_last_id(gs_query* query); /* get last insert id */

#endif
