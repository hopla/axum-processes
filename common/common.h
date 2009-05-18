
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


/* similar to PQexecParams(), except it returns NULL on
 * error, calls log_write(), and lacks the paramTypes,
 * paramLengths, paramFormats and resultFormat arguments. */
PGresult *sql_exec(PGconn *, const char *, char, int, const char * const *);



#endif
