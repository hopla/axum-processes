#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <execinfo.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#define __USE_GNU
#include <dlfcn.h>

#include <mbn.h>
#include <pthread.h>
#include <libpq-fe.h>


FILE *logfd = NULL;
char log_file[500];
char hwparent_path[500];
pid_t parent_pid;
volatile int main_quit = 0;

struct sql_notify *sql_events;
int sql_notifylen = 0;
char sql_lastnotify[50];
char sql_lastnotify_changed_in_callback = 0;
pthread_mutex_t sql_mutex = PTHREAD_MUTEX_INITIALIZER;
PGconn *sql_conn;

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
  FILE *fd = logfd == NULL ? stderr : logfd;
  va_start(ap, fmt);
  vsnprintf(buf, 500, fmt, ap);
  va_end(ap);
//  strftime(tm, 20, "%Y-%m-%d %H:%M:%S", gmtime(&t));
  strftime(tm, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));
  fprintf(fd, "[%s] %s\n", tm, buf);
  fflush(fd);
}


void log_backtrace()
{
  void *array[10];
  size_t size;
  FILE *fd = logfd == NULL ? stderr : logfd;
  int fdno;
  char commandline[256];
  char tmp_log[1024];
  Dl_info segvinfo;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  dladdr(array[3], &segvinfo);

  // print out all the frames to stderr
  fdno = fileno(fd);
  backtrace_symbols_fd(array, size, fdno);
  fflush(fd);

  fprintf(fd, "Analyzing backtrace:\n");
  sprintf(commandline, "addr2line -e %s 0x%08x > addr2line.log\n", segvinfo.dli_fname, (unsigned int)array[3]);
  fprintf(fd, commandline);
  fflush(fd);
  system(commandline);

  FILE *fdtmp = fopen("addr2line.log","r");
  fread(tmp_log, 1024, 1, fdtmp);

  fprintf(fd, tmp_log);

  exit(1);
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
    case SIGSEGV:
      log_write("Segmentation fault, backtrace:");
      log_backtrace();
      break;
    default:
      main_quit = 1;
  }
}


void daemonize() {
  struct sigaction act;
  pid_t pid;

  memset(&act, 0, sizeof(struct sigaction));

  /* catch signals in parent process */
  act.sa_handler = trapsig;
  act.sa_flags = 0;
  sigaction(SIGCHLD, &act, NULL);
  sigaction(SIGUSR1, &act, NULL);
  sigaction(SIGALRM, &act, NULL);
  sigaction(SIGSEGV, &act, NULL);

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

PGresult *sql_exec(const char *query, char res, int nparams, const char * const *values) {
  PGresult *qs;
  qs = PQexecParams(sql_conn, query, nparams, NULL, values, NULL, NULL, 0);
  if(qs == NULL) {
    log_write("Fatal PostgreSQL error: %s", PQerrorMessage(sql_conn));
    return NULL;
  }
  if(PQresultStatus(qs) != (res ? PGRES_TUPLES_OK : PGRES_COMMAND_OK)) {
    log_write("SQL Error for %s: %s", query, PQresultErrorMessage(qs));
    PQclear(qs);
    return NULL;
  }
  sql_processnotifies();
  return qs;
}


void sql_open(const char *str, int l, struct sql_notify *events) {
  PGresult *res;

  sql_conn = PQconnectdb(str);
  if(PQstatus(sql_conn) != CONNECTION_OK) {
    fprintf(stderr, "Opening database: %s\n", PQerrorMessage(sql_conn));
    PQfinish(sql_conn);
    exit(1);
  }
  sql_notifylen = l;
  sql_events = events;

  if(l == 0)
    return;

  if((res = sql_exec("LISTEN change", 0, 0, NULL)) == NULL)
    exit(1);
  PQclear(res);

  /* get timestamp of the most recent change or the current time if the table is empty.
   * This ensures that we have a timestamp in a format suitable for comparing to other
   * timestamps, and in the same time and timezone as the PostgreSQL server. */
  if((res = sql_exec("SELECT MAX(timestamp) FROM recent_changes UNION SELECT NOW() LIMIT 1", 1, 0, NULL)) == NULL)
    exit(1);
  strcpy(sql_lastnotify, PQgetvalue(res, 0, 0));
  PQclear(res);
}

void sql_close() {
  PQfinish(sql_conn);
}


void sql_processnotifies() {
  PGresult *qs;
  PGnotify *not;
  const char *params[1] = { (const char *)sql_lastnotify };
  char *cmd, *arg, myself;
  int i, j, pid;

  /* we don't actually check the struct returned by PQnotifies() */
  if((not = PQnotifies(sql_conn)) == NULL)
    return;
  /* clear any other received notifications */
  do
    PQfreemem(not);
  while((not = PQnotifies(sql_conn)) != NULL);

  /* check the recent_changes table
   * TODO: only fetch notifies in the sql_notifies list? */
  if((qs = sql_exec("SELECT change, arguments, timestamp, pid\
      FROM recent_changes WHERE timestamp > $1 ORDER BY timestamp",
      1, 1, params)) == NULL)
    return;

  for(i=0; i<PQntuples(qs); i++) {
    cmd = PQgetvalue(qs, i, 0);
    arg = PQgetvalue(qs, i, 1);
    sscanf(PQgetvalue(qs, i, 3), "%d", &pid);
    myself = pid == PQbackendPID(sql_conn) ? 1 : 0;

    for(j=0; j<sql_notifylen; j++)
      if(strcmp(cmd, sql_events[j].event) == 0)
        sql_events[j].callback(myself, arg);
  }
  /* update lastnotify variable */
  if ((i>0) && (i<=PQntuples(qs)))
  {
    if (sql_lastnotify_changed_in_callback) {
      sql_lastnotify_changed_in_callback = 0;
      //do nothing else.
    } else {
      //if not changed in callback use time from change (which is normal)
      strcpy(sql_lastnotify, PQgetvalue(qs, i-1, 2));
    }
  }
  PQclear(qs);
}

/* Changes the last notify time, required in case of system time change */
void sql_setlastnotify(char *new_lastnotify)
{
  strcpy(sql_lastnotify, new_lastnotify);
  sql_lastnotify_changed_in_callback = 1;
}

void sql_lock(int l) {
  PGresult *qs;
  if(l) {
    pthread_mutex_lock(&sql_mutex);
    qs = sql_exec("BEGIN", 0, 0, NULL);
  } else {
    qs = sql_exec("COMMIT", 0, 0, NULL);
    pthread_mutex_unlock(&sql_mutex);
  }
  if(qs != NULL)
    PQclear(qs);
}


int sql_loop() {
  int s = PQsocket(sql_conn);
  int n;
  fd_set rd;
  if(s < 0) {
    log_write("Invalid PostgreSQL socket!");
    return 1;
  }

  FD_ZERO(&rd);
  FD_SET(s, &rd);
  n = select(s+1, &rd, NULL, NULL, NULL);
  if(n == 0 || (n < 0 && errno == EINTR))
    return 0;
  if(n < 0) {
    log_write("select() failed: %s\n", strerror(errno));
    return 1;
  }
  sql_lock(1);
  PQconsumeInput(sql_conn);
  sql_processnotifies();
  sql_lock(0);
  return 0;
}

int oem_name_short(char *name, int name_length)
{
  int name_found = 0;
  FILE *F = fopen("/var/lib/axum/OEMShortProductName", "r");
  if (F != NULL)
  {
    char *line=NULL;
    size_t len=0;
    size_t i;
    size_t ReadedBytes = getline(&line, &len, F);
    if (ReadedBytes>0)
    {
      for (i=0; i<ReadedBytes; i++)
      {
        if (line[i] == '\n')
        {
          line[i] = '\0';
        }
      }
      strncpy(name, line, name_length);
      name_found = 1;
    }
    if (line)
    {
      free(line);
    }
    fclose(F);
  }
  return name_found;
}
