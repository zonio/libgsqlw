#ifndef __GSW_H__
#define __GSW_H__

#include <glib.h>

/** @file gsqlw Glib based SQL DB C interface wrapper.
 */

typedef struct _gs_conn gs_conn;
typedef struct _gs_query gs_query;

enum _gs_errors
{
  GS_ERR_NONE = 0,
  GS_ERR_OTHER,
};

G_BEGIN_DECLS

/** Create connection to the database.
 *
 * @param dsn DSN specifies connection info. DSN consists of two parts separated
 * by a colon. First part specifies backend that should be used and second part
 * backend specific "connection" setup. Currently there are two supported backends:
 * @li sqlite:file_path
 * @li pgsql:dbname=test host=localhost user=postgres password=pass
 *
 * @return gs_conn on success NULL on error.
 */
gs_conn* gs_connect(const char* dsn);

/** Disconnect and free database connection.
 *
 * All queries associated with this connection must be freed before calling this
 * function.
 *
 * @param conn DB connection object.
 */
void gs_disconnect(gs_conn* conn);

/** Get backend name.
 *
 * Currently this method return either 'sqlite' or 'pgsql'.
 *
 * @param conn DB connection object.
 *
 * @return Backend name or empty string on error.
 */
const char* gs_get_backend(gs_conn* conn);

/** Get string describing last error.
 *
 * If this method returns non-NULL value all other methods except for
 * gs_rollback(), gs_finish(), gs_query_free() and gs_disconnect() are inhibited.
 *
 * @param conn DB connection object.
 *
 * @return Error string or NULL.
 */
const char* gs_get_errmsg(gs_conn* conn);

/** Get error code.
 *
 * @param conn DB connection object.
 *
 * @return Error code, see enum _gs_errors for list of supported error codes.
 */
int gs_get_errcode(gs_conn* conn);

/** Set error code and message.
 *
 * This is mainly for internal use, but knowledgable user can use it externally
 * to handle errors too.
 *
 * @param conn DB connection object.
 * @param code Error code (see enum _gs_errors for list of codes).
 * @param msg Error message.
 */
void gs_set_error(gs_conn* conn, int code, const char* msg);

/** Clear error state.
 *
 * Use with caution. If error was set during gs_query execution you must NOT
 * continue using that query after clearing the error. Basically what you should
 * do when error happens is aborting the current transaction if any and freeing
 * current query. Then you can clear error and try again.
 *
 * @param conn DB connection object.
 */
void gs_clear_error(gs_conn* conn);

/** Begin transaction on the given connection.
 *
 * @param conn DB connection object.
 *
 * @return -1 on error, 0 on success.
 */
int gs_begin(gs_conn* conn);

/** Commit transaction on the given connection.
 *
 * @param conn DB connection object.
 *
 * @return -1 on error, 0 on success.
 */
int gs_commit(gs_conn* conn);

/** Rollback transaction on the given connection.
 *
 * @param conn DB connection object.
 *
 * @return -1 on error, 0 on success.
 */
int gs_rollback(gs_conn* conn);

/** Finish current transaction. 
 *
 * This means commit if error is not set (see gs_get_error()) or rollback if it
 * is.
 *
 * @param conn DB connection object.
 *
 * @return -1 on error (rollback), 0 on success.
 */
int gs_finish(gs_conn* conn);

/** Execute simple SQL command without returning any results.
 *
 * @param conn DB connection object.
 * @param sql_string SQL command. This may contain $N substitutions.
 * @param fmt Format string that defines number and type of substitutions given
 * as remaining parameters to exec.
 * @li s - const char*
 * @li i - int
 * @li ?i - int is_null, int val
 *
 * @return -1 on error, 0 on success.
 *
 * Example:
 * gs_exec("INSERT INTO t(v1, v2) VALUES($1, $2)", "is", 1, "tes't");
 */
int gs_exec(gs_conn* conn, const char* sql_string, const char* fmt, ...);

/** Create new SQL query.
 *
 * @param conn DB connection object.
 * @param sql_string SQL query string.
 *
 * @return NULL on error, gs_query object on success.
 */
gs_query* gs_query_new(gs_conn* conn, const char* sql_string);

/** Free query object.
 *
 * @param query Query object.
 */
void gs_query_free(gs_query* query);

/** Get next row from the query result set.
 *
 * @param query Query object.
 * @param fmt Fromat string describing format of row data, remaining parameters
 * are pointers to variables that will be used to store row data according to
 * the format string.
 * @li s - const char**  - valid untill next gs_query_get call.
 * @li S - char**        - caller must free returned data using g_free
 * @li i - int*
 * @li ?i - int* is_null, int* val
 *
 * @return -1 on error, 0 on success, 1 if no more rows avaliable.
 */
int gs_query_get(gs_query* query, const char* fmt, ...);

/** (Re-)execute prepared query using given set of substitution parameters.
 *
 * @param query Query object.
 * @param fmt Format string that defines number and type of substitutions given
 * as remaining parameters to exec.
 * @li s - const char*
 * @li i - int
 * @li ?i - int is_null, int val
 *
 * @return -1 on error, 0 on success.
 */
int gs_query_put(gs_query* query, const char* fmt, ...);

/** Return number of rows that given query returns.
 *
 * May be only called after successfull gs_query_put.
 *
 * @param query Query object.
 *
 * @return -1 on error, number of rows on success.
 */
int gs_query_get_rows(gs_query* query);

/** Return last inserted row's ID.
 *
 * May be only called after successfull gs_query_put or gs_exec.
 *
 * @param query Query object.
 * @param seq_name Postgresql backend requires sequence name.
 *
 * @return -1 on error, positive ID on success.
 */
int gs_query_get_last_id(gs_query* query, const char* seq_name);

G_END_DECLS

#endif
