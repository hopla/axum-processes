
#include <mbn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define DEFAULT_UNIX_PATH "/tmp/axum-address"
#define DEFAULT_ETH_DEV   "eth0"
#define MAX_CONNECTIONS   10
#define UNIX_SOCK_BUFFER  2048

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX 108
#endif
#define MAX(x, y) ((x)>(y)?(x):(y))


/* node info */
struct mbn_node_info this_node = {
  0, MBN_ADDR_SERVICES_SERVER,
  "MambaNet Address Server",
  "D&R Address Server (beta)",
  0xFFFF, 0x0004, 0x0001,
  0, 0, /* HW */
  0, 1, /* FW */
  0, 0, /* FPGA */
  0, 0, /* objects, engine */
  {0,0,0,0,0,0}, /* parent */
  0 /* service */
};


/* global variables */
struct mbn_handler *mbn;
struct sockaddr_un unix_path;
int listensock;
volatile int main_quit;
struct s_conn {
  int sock;
  int buflen;
  char state; /* 0=unused, 1=read, 2=write */
  char *buf[UNIX_SOCK_BUFFER];
} connections[MAX_CONNECTIONS];


void init(int argc, char **argv) {
  struct mbn_interface *itf;
  char err[MBN_ERRSIZE];
  char ethdev[50];
  int c;

  unix_path.sun_family = AF_UNIX;
  strcpy(unix_path.sun_path, DEFAULT_UNIX_PATH);
  memset((void *)connections, 0, sizeof(struct s_conn)*MAX_CONNECTIONS);
  strcpy(ethdev, DEFAULT_ETH_DEV);

  /* parse options */
  while((c = getopt(argc, argv, "e:u:")) != -1) {
    switch(c) {
      case 'e':
        if(strlen(optarg) > 50) {
          fprintf(stderr, "Too long device name.");
          exit(1);
        }
        strcpy(ethdev, optarg);
        break;
      case 'u':
        if(strlen(optarg) > UNIX_PATH_MAX) {
          fprintf(stderr, "Too long path to UNIX socket!");
          exit(1);
        }
        strcpy(unix_path.sun_path, optarg);
        break;
      default:
        exit(1);
    }
  }

  /* initialize the MambaNet node */
  if((itf = mbnEthernetOpen(ethdev, err)) == NULL) {
    fprintf(stderr, "Opening %s: %s\n", ethdev, err);
    exit(1);
  }
  if((mbn = mbnInit(&this_node, NULL, itf, err)) == NULL) {
    fprintf(stderr, "mbnInit: %s\n", err);
    exit(1);
  }

  /* initialize UNIX listen socket */
  if((listensock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("Opening socket");
    mbnFree(mbn);
    exit(1);
  }
  if(bind(listensock, (struct sockaddr *)&unix_path, sizeof(struct sockaddr_un)) < 0) {
    perror("Binding to path");
    close(listensock);
    mbnFree(mbn);
    exit(1);
  }
  if(listen(listensock, 5) < 0) {
    perror("Listening on socket");
    fprintf(stderr, "Are you sure no other address server is running?\n");
    close(listensock);
    mbnFree(mbn);
    exit(1);
  }
}


int mainloop() {
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

  return 0;
}


void trapsig(int sig) {
  main_quit = sig;
}


int main(int argc, char **argv) {
  struct sigaction act;
  int i;

  act.sa_handler = trapsig;
  act.sa_flags = 0;
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  main_quit = 0;

  init(argc, argv);
  while(!main_quit && !mainloop())
    ;

  /* free */
  for(i=0; i<MAX_CONNECTIONS+1; i++)
    if(connections[i].state > 0)
      close(connections[i].sock);
  close(listensock);
  unlink(unix_path.sun_path);
  mbnFree(mbn);
  return 0;
}

