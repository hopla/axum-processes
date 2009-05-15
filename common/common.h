
#ifndef _common_h
#define _common_h


/* Logging functions. */
void log_write(char *, ...);
void log_close();
/* log_open() can be called multiple times, all subsequent calls will be ignored */
void log_open(char *);


/* Handles daemonizing and sets signal handling.
 * This function should be called before initialization. */
void daemonize();
/* Should be called after initialization, to signal that
 * the process has successfully started */
void daemonize_finish();
/* Will be set to 1 in a signal handler to indicate that
 * the process should quit */
extern volatile int main_quit;


#endif
