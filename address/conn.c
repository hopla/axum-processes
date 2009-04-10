
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define MAX_CONNECTIONS   10
#define UNIX_SOCK_BUFFER  2048

#define MAX(x, y) ((x)>(y)?(x):(y))

struct s_conn {
  int sock;
  int buflen;
  char state; /* 0=unused, 1=read, 2=write */
  char *buf[UNIX_SOCK_BUFFER];
};


struct sockaddr_un unix_path;
int listensock;
struct s_conn connections[MAX_CONNECTIONS];


int conn_init(char *upath, int force, char *err) {
  memset((void *)connections, 0, sizeof(struct s_conn)*MAX_CONNECTIONS);
  unix_path.sun_family = AF_UNIX;
  strcpy(unix_path.sun_path, upath);

  if(force)
    unlink(unix_path.sun_path);

  if((listensock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    sprintf(err, "Opening socket: %s", strerror(errno));
    return 1;
  }
  if(bind(listensock, (struct sockaddr *)&unix_path, sizeof(struct sockaddr_un)) < 0) {
    sprintf(err, "Binding to path: %s", strerror(errno));
    close(listensock);
    return 1;
  }
  if(listen(listensock, 5) < 0) {
    sprintf(err, "Listening on socket: %s", strerror(errno));
    close(listensock);
    return 1;
  }
  return 0;
}


int conn_loop() {
  fd_set rd, wr;
  int i, n, max = 0;

  /* set FDs for select() */
  FD_ZERO(&rd);
  FD_ZERO(&wr);
  FD_SET(listensock, &rd);
  max = MAX(max, listensock);
  /*
  for(i=0;i<MAX_CONNECTIONS;i++)
    if(connections[i].state > 0) {
      FD_SET(connections[i].sock, (connections[i].state == 1 ? &rd : &wr));
      max = MAX(max, connections[i].sock);
    }
  */

  /* select() */
  n = select(max+1, &rd, &wr, NULL, NULL);
  if(n == 0 || (n < 1 && errno == EINTR))
    return 0;
  if(n < 1) {
    perror("select");
    return 1;
  }

  /* accept new connection */
  if(FD_ISSET(listensock, &rd)) {
    if((n = accept(listensock, NULL, NULL)) < 0) {
      perror("Accepting new connection");
      return 1;
    }
    for(i=0;i<MAX_CONNECTIONS;i++)
      if(connections[i].state == 0)
        break;
    if(i == MAX_CONNECTIONS)
      close(n);
    else {
      connections[i].sock = n;
      connections[i].state = 1;
      connections[i].buflen = 0;
    }
  }

  /* TODO: handle I/O */

  return 0;
}


void conn_free() {
  int i;
  for(i=0; i<MAX_CONNECTIONS+1; i++)
    if(connections[i].state > 0)
      close(connections[i].sock);
  close(listensock);
  unlink(unix_path.sun_path);
}

