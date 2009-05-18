
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <mbn.h>
#include <libpq-fe.h>


FILE *logfd = NULL;
char log_file[500];
char hwparent_path[500];
pid_t parent_pid;
volatile int main_quit = 0;


void log_open() {
  if(logfd != NULL)
    fclose(logfd);
  if((logfd = fopen(log_file, "a")) == NULL) {
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
  log_open();
}


void log_write(const char *fmt, ...) {
  va_list ap;
  char buf[500], tm[20];
  time_t t = time(NULL);
  int fd = logfd == NULL ? stderr : logfd;
  va_start(ap, fmt);
  vsnprintf(buf, 500, fmt, ap);
  va_end(ap);
  strftime(tm, 20, "%Y-%m-%d %H:%M:%S", gmtime(&t));
  fprintf(fd, "[%s] %s\n", tm, buf);
  fflush(fd);
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


void hwparent(struct mbn_node_info *node) {
  struct sockaddr_un p;
  char msg[14];
  int sock;

  /* if path is a hardware parent, use that */
  if(sscanf(hwparent_path, "%04hx:%04hx:%04hx", node->HardwareParent, node->HardwareParent+1, node->HardwareParent+2) == 3)
    return;

  /* otherwise, connect to unix socket */
  p.sun_family = AF_UNIX;
  strcpy(p.sun_path, hwparent_path);
  if((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("Opening UNIX socket to gateway");
    exit(1);
  }
  if(connect(sock, (struct sockaddr *)&p, sizeof(struct sockaddr_un)) < 0) {
    perror("Connecting to gateway");
    exit(1);
  }
  /* TODO: check readability (select(), nonblock)? time-out? */
  if(read(sock, msg, 14) < 14) {
    perror("Reading from gateway socket");
    exit(1);
  }
  if(sscanf(msg, "%04hx:%04hx:%04hx", node->HardwareParent, node->HardwareParent+1, node->HardwareParent+2) != 3) {
    fprintf(stderr, "Received invalid parent: %s\n", msg);
    exit(1);
  }
}


PGresult *sql_exec(PGconn *db, const char *query, char res, int nparams, const char * const *values) {
  PGresult *qs;
  qs = PQexecParams(db, query, nparams, NULL, values, NULL, NULL, 0);
  if(qs == NULL) {
    log_write("Fatal PostgreSQL error: %s", PQerrorMessage(db));
    return NULL;
  }
  if(PQresultStatus(qs) != (res ? PGRES_TUPLES_OK : PGRES_COMMAND_OK)) {
    log_write("SQL Error for %s: %s", query, PQresultErrorMessage(qs));
    PQclear(qs);
    return NULL;
  }
  return qs;
}

