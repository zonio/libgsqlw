Glib SQL Wrapper
----------------

Mission: simple wrapper library for sql databases with minimalistic, user
extensible API intended for hand written code.

If you have any questions, patches, new backend implementations, etc.
send them to glib-sql-wrapper@zhost.net.


!!! IMPORTANT !!!

API Assumptions:
- you must always call query_put after query_new
- you may call query_get_rows only immediately after query_put


