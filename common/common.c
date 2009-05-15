
#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>


FILE *logfd = NULL;
char logfile[256];


void log_open(char *file) {
  if(logfd != NULL)
    return;
  if(strlen(file) > 255) {
    fprintf(stderr, "Path to log file is too long!\n");
    exit(1);
  }
  strcpy(logfile, file);
  if((logfd = fopen(logfile, "a")) == NULL) {
    fprintf(stderr, "Couldn't open log file: %s\n", strerror(errno));
    exit(1);
  }
}


void log_close() {
  fclose(logfd);
}


void log_reopen() {
  log_write("SIGHUP received, re-opening log file");
  log_close();
  logfd = fopen(logfile, "a");
}


void log_write(char *fmt, ...) {
  va_list ap;
  char buf[500], tm[20];
  time_t t = time(NULL);
  if(logfd == NULL)
    return;
  va_start(ap, fmt);
  vsnprintf(buf, 500, fmt, ap);
  va_end(ap);
  strftime(tm, 20, "%Y-%m-%d %H:%M:%S", gmtime(&t));
  fprintf(logfd, "[%s] %s\n", tm, buf);
  fflush(logfd);
}


