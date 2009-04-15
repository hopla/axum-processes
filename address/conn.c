
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <json.h>

#define MAX_CONNECTIONS   10
#define READBUFSIZE     1024 /* should be large enough to store one command */
#define WRITEBUFSIZE    8192 /* should be large enough to store several replies */
 /* required memory for buffers = MAX_CONNECTIONS * (READBUFSIZE + WRITEBUFSIZE) */

#define MAX(x, y) ((x)>(y)?(x):(y))

struct s_conn {
  int sock;
  int rdlen;
  int wrstart, wrend;
  char state; /* 0=unused, 1=active */
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


void conn_send(int client, char *cmd) {
  int i, len;
  len = strlen(cmd);
  /* buffer overflow simply overwrites previous commands at the moment */
  for(i=0; i<=len; i++) {
    conn[client].wr[conn[client].wrend] = i==len ? '\n' : cmd[i];
    if(++conn[client].wrend >= WRITEBUFSIZE)
      conn[client].wrend = 0;
  }
}


void conn_receive(int client, char *cmd) {
  printf("Received \"%s\", echo'ing...\n", cmd);
  conn_send(client, cmd);
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
      /* always check read */
      FD_SET(conn[i].sock, &rd);
      x = MAX(x, conn[i].sock);
      /* only check send if we have data to send */
      if(conn[i].wrstart != conn[i].wrend)
        FD_SET(conn[i].sock, &wr);
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
    /* read data */
    if(conn[i].state > 0 && FD_ISSET(conn[i].sock, &rd)) {
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
          conn_receive(i, conn[i].rd);
          conn[i].rdlen = 0;
          continue;
        }
        conn[i].rd[conn[i].rdlen++] = buf[x];
        if(conn[i].rdlen >= READBUFSIZE)
          conn[i].rdlen = 0;
      }
    }

    /* write data */
    if(conn[i].state > 0 && conn[i].wrstart != conn[i].wrend && FD_ISSET(conn[i].sock, &wr)) {
      n = conn[i].wrend - conn[i].wrstart;
      if(n < 0)
        n = WRITEBUFSIZE-n;
      n = n > WRITEBUFSIZE-conn[i].wrstart ? WRITEBUFSIZE-conn[i].wrstart : n;
      if((n = write(conn[i].sock, &(conn[i].wr[conn[i].wrstart]), n)) < 0 && errno != EINTR) {
        perror("write");
        return 1;
      }
      conn[i].wrstart += n;
      if(conn[i].wrstart >= WRITEBUFSIZE)
        conn[i].wrstart -= WRITEBUFSIZE;
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

