
#include <mbn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DEFAULT_UNIX_PATH "/tmp/axum-address"
#define DEFAULT_ETH_DEV   "eth0"
#define MAX_CONNECTIONS   10

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX 108
#endif


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
int unix_socks[MAX_CONNECTIONS+1];


void init(int argc, char **argv) {
  struct mbn_interface *itf;
  char err[MBN_ERRSIZE];
  char ethdev[50];
  int c;

  unix_path.sun_family = AF_UNIX;
  strcpy(unix_path.sun_path, DEFAULT_UNIX_PATH);
  memset((void *)unix_socks, 0, sizeof(int)*(MAX_CONNECTIONS+1));
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
  if((unix_socks[0] = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("Opening socket");
    mbnFree(mbn);
    exit(1);
  }
  if(bind(unix_socks[0], (struct sockaddr *)&unix_path, sizeof(struct sockaddr_un)) < 0) {
    perror("Binding to path");
    close(unix_socks[0]);
    mbnFree(mbn);
    exit(1);
  }
  if(listen(unix_socks[0], 5) < 0) {
    perror("Listening on socket");
    fprintf(stderr, "Are you sure no other address server is running?\n");
    close(unix_socks[0]);
    mbnFree(mbn);
    exit(1);
  }
}


int main(int argc, char **argv) {
  int i;

  init(argc, argv);
  sleep(10);

  /* free */
  for(i=0; i<MAX_CONNECTIONS+1; i++)
    if(unix_socks[i] > 0)
      close(unix_socks[i]);
  unlink(unix_path.sun_path);
  mbnFree(mbn);
  return 0;
}

