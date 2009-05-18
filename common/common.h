
#ifndef _common_h
#define _common_h

#include <mbn.h>
#include <libpq-fe.h>


/* Logging functions. */
extern char log_file[500];
void log_write(const char *, ...);
void log_close();
void log_open();


/* Handles daemonizing and sets signal handling.
 * This function should be called before initialization. */
void daemonize();
/* Should be called after initialization, to signal that
 * the process has successfully started */
void daemonize_finish();
/* Will be set to 1 in a signal handler to indicate that
 * the process should quit */
extern volatile int main_quit;


/* receives hardware parent from UNIX socket or command line */
extern char hwparent_path[500];
void hwparent(struct mbn_node_info *);


/* the sql connection, shouldn't really be used in the application,
 * but might be useful in some rare cases. */
extern PGconn *sql_conn;

/* A listen event */
struct sql_notify {
  char *event;
  void (*callback)(char, char *);
};

/* Open the database connection, first argument is a connection
 * string (a la PQconnectdb), second is the number of listen events,
 * third an array of events to process */
void sql_open(const char *, int, struct sql_notify *);

/* closes the connection */
void sql_close();

/* lock access to the database internally, and begin/commit a transaction */
void sql_lock(int l);

/* check for notifications from PostgreSQL, automatically called by
 * sql_exec() and sql_loop(), but might be useful if you're not using
 * sql_loop() */
void sql_processnotifies();

/* blocking wait for notifications, returns 0 when it should be called again,
 * or 1 on error */
int sql_loop();

/* similar to PQexecParams(), except it returns NULL on
 * error, calls log_write(), and lacks the paramTypes,
 * paramLengths, paramFormats and resultFormat arguments. */
PGresult *sql_exec(const char *, char, int, const char * const *);


#endif
