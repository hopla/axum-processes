
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define MAX_CONNECTIONS   10
#define READBUFSIZE      256 /* should be large enough to store one command */
#define WRITEBUFSIZE    4096 /* should be large enough to store several replies */
 /* required memory for buffers = MAX_CONNECTIONS * (READBUFSIZE + WRITEBUFSIZE) */

#define MAX(x, y) ((x)>(y)?(x):(y))

struct s_conn {
  int sock;
  int rdlen;
  int wrstart, wrend;
  char state; /* 0=unused, 1=read, 2=write */
  char rd[READBUFSIZE];
  char wr[WRITEBUFSIZE]; /* circular */
};


struct sockaddr_un unix_path;
int listensock;
struct s_conn conn[MAX_CONNECTIONS];


int conn_init(char *upath, int force, char *err) {
  memset((void *)conn, 0, sizeof(struct s_conn)*MAX_CONNECTIONS);
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
  int i, n, x = 0;
  char buf[READBUFSIZE];

  /* set FDs for select() */
  FD_ZERO(&rd);
  FD_ZERO(&wr);
  FD_SET(listensock, &rd);
  x = MAX(x, listensock);
  for(i=0;i<MAX_CONNECTIONS;i++)
    if(conn[i].state > 0) {
      /*if(conn[i].state == 2)
        FS_SET(conn[i].sock, &wr);*/
      /* always check read */
      FD_SET(conn[i].sock, &rd);
      x = MAX(x, conn[i].sock);
    }

  /* select() */
  n = select(x+1, &rd, &wr, NULL, NULL);
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
      if(conn[i].state == 0)
        break;
    if(i == MAX_CONNECTIONS)
      close(n);
    else {
      conn[i].sock = n;
      conn[i].state = 1;
      conn[i].rdlen = 0;
      conn[i].wrstart = 0;
      conn[i].wrend = 0;
    }
  }

  for(i=0;i<MAX_CONNECTIONS;i++) {
    if(conn[i].state == 0)
      continue;

    /* read data */
    if(FD_ISSET(conn[i].sock, &rd)) {
      if((n = read(conn[i].sock, buf, READBUFSIZE)) < 0 && errno != EINTR) {
        perror("Read");
        return 1;
      }
      /* connection closed */
      if(n == 0) {
        close(conn[i].sock);
        conn[i].state = 0;
      }
      /* copy and process buffer */
      for(x=0; x<n; x++) {
        if(buf[x] == '\r')
          continue;
        if(buf[x] == '\n') {
          conn[i].rd[conn[i].rdlen] = 0;
          printf("COM: %s\n", conn[i].rd);
          /* TODO: handle command */
          conn[i].rdlen = 0;
          continue;
        }
        conn[i].rd[conn[i].rdlen++] = buf[x];
        if(conn[i].rdlen >= READBUFSIZE)
          conn[i].rdlen = 0;
      }
    }
  }

  return 0;
}


void conn_free() {
  int i;
  for(i=0; i<MAX_CONNECTIONS+1; i++)
    if(conn[i].state > 0)
      close(conn[i].sock);
  close(listensock);
  unlink(unix_path.sun_path);
}

