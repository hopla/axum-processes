
#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>


FILE *logfd = NULL;
char logfile[256];
pid_t parent_pid;
volatile int main_quit = 0;


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
  if(logfd != NULL)
    fclose(logfd);
  logfd = NULL;
}


void log_reopen() {
  if(logfd == NULL)
    return;
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


void trapsig(int sig) {
  switch(sig) {
    case SIGALRM:
    case SIGCHLD:
      exit(1);
    case SIGUSR1:
      exit(0);
    case SIGHUP:
      log_reopen();
      break;
    default:
      main_quit = 1;
  }
}


void daemonize() {
  struct sigaction act;
  pid_t pid;

  /* catch signals in parent process */
  act.sa_handler = trapsig;
  act.sa_flags = 0;
  sigaction(SIGCHLD, &act, NULL);
  sigaction(SIGUSR1, &act, NULL);
  sigaction(SIGALRM, &act, NULL);

  /* fork */
  if((pid = fork()) < 0) {
    perror("fork()");
    exit(1);
  }
  /* parent process, wait for initialization and return 1 on error */
  if(pid > 0) {
    alarm(15);
    pause();
    fprintf(stderr, "Initialization took too long!\n");
    kill(pid, SIGKILL);
    exit(1);
  }

  /* catch signals in daemon process */
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGHUP, &act, NULL);
  act.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &act, NULL);
  act.sa_handler = SIG_IGN;
  sigaction(SIGUSR1, &act, NULL);
  sigaction(SIGALRM, &act, NULL);
  umask(0);

  /* get parent id and a new session id */
  parent_pid = getppid();
  if(setsid() < 0) {
    perror("setsid()");
    exit(1);
  }
}


void daemonize_finish() {
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  kill(parent_pid, SIGUSR1);
}

