/****************************************************************************
**
** Copyright (C) 2009 D&R Electronica Weesp B.V. All rights reserved.
**
** This file is part of the Axum/MambaNet digital mixing system.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>

#define MBN_VARARG
#include "mbn.h"

#define DEFAULT_UNIX_PATH "/tmp/axum-gateway"
#define DEFAULT_DATA_PATH "/var/lib/axum/axum-gateway.ip"

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX 108
#endif

#define nodestr(n) (n == eth ? "eth" : n == can ? "can" : "tcp")

#define OBJ_IPADDR   0
#define OBJ_CANNODES 1
#define OBJ_ETHNODES 2
#define OBJ_TCPNODES 3


struct mbn_node_info this_node = {
  0x00000000, 0x00, /* MambaNet Addr + Services */
  "Axum CAN Gateway",
  "YorHels Gateway",
  0xFFFF, 0x0002, 0x0001,   /* UniqueMediaAccessId */
  0, 0,    /* Hardware revision */
  2, 1,    /* Firmware revision */
  0, 0,    /* FPGAFirmware revision */
  4,       /* NumberOfObjects */
  0,       /* DefaultEngineAddr */
  {0,0,0}, /* Hardwareparent */
  0        /* Service request */
};

struct mbn_handler *eth, *can, *tcp;
int verbose;
char ieth[50], data_path[1000];


void set_ip(unsigned int ip) {
  struct ifreq ir;
  struct sockaddr_in *si = (struct sockaddr_in *)&(ir.ifr_addr);
  int s;
  FILE *f;

  /* set ip */
  if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return;
  si->sin_addr.s_addr = htonl(ip);
  si->sin_family = AF_INET;
  strcpy(ir.ifr_name, ieth);
  if(ioctl(s, SIOCSIFADDR, &ir) < 0)
    return;
  /* set flags */
  if(ip > 0) {
    strcpy(ir.ifr_name, ieth);
    if(ioctl(s, SIOCGIFFLAGS, &ir) < 0)
      return;
    ir.ifr_flags |= (IFF_UP | IFF_RUNNING);
    strcpy(ir.ifr_name, ieth);
    if(ioctl(s, SIOCSIFFLAGS, &ir) < 0)
      return;
  }
  if(verbose)
    printf("IP Set to %08X\n", ip);

  /* save to data file (silently ignoring errors) */
  if((f = fopen(data_path, "w")) != NULL) {
    fprintf(f, "%08X\n", ip);
    fclose(f);
  }
}

/* doesn't do IPv6 */
unsigned int get_ip() {
  struct ifreq ir;
  FILE *f;
  int s;
  unsigned int r;

  /* try to get an IP from the data file first */
  if((f = fopen(data_path, "r")) == NULL)
    goto from_system;
  if(fscanf(f, "%08x", &r) != 1)
    goto from_system;
  /* and set the IP */
  set_ip(r);
  return r;

  /* otherwise, get current setting */
from_system:
  if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return 0;
  strcpy(ir.ifr_name, ieth);
  if(ioctl(s, SIOCGIFADDR, &ir, sizeof(struct ifreq)) < 0)
    return 0;
  close(s);

  if(((struct sockaddr *)&(ir.ifr_addr))->sa_family == AF_INET)
    return ntohl(((struct sockaddr_in *)&(ir.ifr_addr))->sin_addr.s_addr);
  return 0;
}


void SynchroniseDateTime(struct mbn_handler *mbn, time_t time) {
  struct timeval tv;
  /* TODO: set hardware clock */
  tv.tv_usec = 0;
  tv.tv_sec = time;
  settimeofday(&tv, NULL);
  if(verbose)
    printf("Setting system time to %ld, request from %s\n", time, nodestr(mbn));
}


int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data dat) {
  if(object != OBJ_IPADDR+1024 || mbn != eth)
    return 1;
  set_ip(dat.UInt);
  return 0;
}


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  if(verbose)
    printf("OnlineStatus on %s: %08lX %s\n", nodestr(mbn), addr, valid ? "validated" : "invalid");
  if(valid) {
    if(can != NULL && mbn != can) mbnForceAddress(can, addr);
    if(eth != NULL && mbn != eth) mbnForceAddress(eth, addr);
    if(tcp != NULL && mbn != tcp) mbnForceAddress(tcp, addr);
  }
  this_node.MambaNetAddr = addr;
}


void Error(struct mbn_handler *mbn, int code, char *msg) {
  if(verbose)
    printf("Error(%s, %d, \"%s\")\n", nodestr(mbn), code, msg);
}


int ReceiveMessage(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_handler *dest = NULL;
  int i;

  /* don't forward anything that's targeted to us */
  if(msg->AddressTo == this_node.MambaNetAddr)
    return 0;

  /* figure out to which interface we need to send */
  if(msg->AddressTo != MBN_BROADCAST_ADDRESS) {
    if(can != NULL && mbnNodeStatus(can, msg->AddressTo) != NULL)
      dest = can;
    else if(eth != NULL && mbnNodeStatus(eth, msg->AddressTo) != NULL)
      dest = eth;
    else if(tcp != NULL && mbnNodeStatus(tcp, msg->AddressTo) != NULL)
      dest = tcp;

    /* don't forward if we haven't found the destination node */
    if(dest == NULL)
      return 0;

    /* don't forward if the destination is on the same network */
    if(dest == mbn)
      return 0;
  }

  /* print out what's happening */
  if(verbose) {
    printf(" %s %1X %08lX %08lX %03X %1X:", nodestr(mbn),
      msg->AcknowledgeReply, msg->AddressTo, msg->AddressFrom, msg->MessageID, msg->MessageType);
    for(i=0;i<msg->bufferlength;i++)
      printf(" %02X", msg->buffer[i]);
    printf("\n");
  }

  /* forward message */
#define fwd(m) mbnSendMessage(m, msg, MBN_SEND_IGNOREVALID | MBN_SEND_FORCEADDR | MBN_SEND_NOCREATE | MBN_SEND_FORCEID)
  if(dest != NULL)
    fwd(dest);
  else {
    if(eth != NULL && mbn != eth) fwd(eth);
    if(tcp != NULL && mbn != tcp) fwd(tcp);
    if(can != NULL && mbn != can) {
      /* filter out address reservation packets to can */
      if(!(dest == NULL && msg->MessageType == MBN_MSGTYPE_ADDRESS && msg->Message.Address.Action == MBN_ADDR_ACTION_INFO))
        fwd(can);
    }
  }
#undef fwd
  return 0;
}


void AddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct mbn_address_node *n = NULL;
  union mbn_data count;
  int obj;
  count.UInt = 0;

  if(old && new)
    return;

  while((n = mbnNextNode(mbn, n)) != NULL)
    count.UInt++;

  obj = mbn == can ? OBJ_CANNODES : mbn == eth ? OBJ_ETHNODES : OBJ_TCPNODES;
  obj += 1024;
  if(can != NULL) mbnUpdateSensorData(can, obj, count);
  if(eth != NULL) mbnUpdateSensorData(eth, obj, count);
  if(tcp != NULL) mbnUpdateSensorData(tcp, obj, count);
}


void process_unix(char *path) {
  struct sockaddr_un p;
  int sock, client;
  char msg[15];

  p.sun_family = AF_UNIX;
  strcpy(p.sun_path, path);
  unlink(path);

  if((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("Opening UNIX socket");
    return;
  }
  if(bind(sock, (struct sockaddr *)&p, sizeof(struct sockaddr_un)) < 0) {
    perror("Binding to path");
    close(sock);
    return;
  }
  if(listen(sock, 5) < 0) {
    perror("Listening on UNIX socket");
    close(sock);
    return;
  }

  sprintf(msg, "%04X:%04X:%04X", this_node.HardwareParent[0], this_node.HardwareParent[1], this_node.HardwareParent[2]);
  while((client = accept(sock, NULL, NULL)) >= 0) {
    if(write(client, msg, 14) < 14) {
      perror("Writing to client");
      return;
    }
    close(client);
  }
  perror("Accepting connections on UNIX socket");
}


void setcallbacks(struct mbn_handler *mbn) {
  mbnSetErrorCallback(mbn, Error);
  mbnSetOnlineStatusCallback(mbn, OnlineStatus);
  mbnSetReceiveMessageCallback(mbn, ReceiveMessage);
  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);
  mbnSetSetActuatorDataCallback(mbn, SetActuatorData);
  mbnSetSynchroniseDateTimeCallback(mbn, SynchroniseDateTime);
}


void trapsig(int sig) {
  switch(sig) {
    case SIGALRM:
    case SIGCHLD:
      exit(1);
    case SIGUSR1:
      exit(0);
  }
}


void init(int argc, char **argv, char *upath) {
  struct mbn_interface *itf = NULL;
  struct mbn_object obj[4];
  char err[MBN_ERRSIZE], ican[50], tport[10];
  unsigned short parent[3] = {0,0,0};
  int c, itfcount = 0;
  struct sigaction act;
  pid_t pid;

  strcpy(upath, DEFAULT_UNIX_PATH);
  strcpy(data_path, DEFAULT_DATA_PATH);
  ican[0] = ieth[0] = tport[0] = 0;
  can = eth = tcp = NULL;
  verbose = 0;

  while((c = getopt(argc, argv, "c:e:u:t:d:v")) != -1) {
    switch(c) {
      /* can interface */
      case 'c':
        if(strlen(optarg) > 49) {
          fprintf(stderr, "CAN interface name too long\n");
          exit(1);
        }
        strcpy(ican, optarg);
        itfcount++;
        break;
      /* ethernet interface */
      case 'e':
        if(strlen(optarg) > 49) {
          fprintf(stderr, "Ethernet interface name too long\n");
          exit(1);
        }
        strcpy(ieth, optarg);
        itfcount++;
        break;
      /* TCP port */
      case 't':
        if(strlen(optarg) > 9) {
          fprintf(stderr, "TCP port too long\n");
          exit(1);
        }
        strcpy(tport, optarg);
        itfcount++;
        break;
      /* UNIX socket */
      case 'u':
        if(strlen(optarg) > UNIX_PATH_MAX) {
          fprintf(stderr, "Too long path to UNIX socket!\n");
          exit(1);
        }
        strcpy(upath, optarg);
        break;
      /* data path */
      case 'd':
        if(strlen(optarg) > 1000) {
          fprintf(stderr, "Too long path to data file!\n");
          exit(1);
        }
        strcpy(data_path, optarg);
        break;
      /* verbose */
      case 'v':
        verbose++;
        break;
      /* wrong option */
      default:
        fprintf(stderr, "Usage: %s [-c dev] [-e dev] [-u path]\n", argv[0]);
        fprintf(stderr, "  -v       Print verbose output to stdout\n");
        fprintf(stderr, "  -c dev   CAN device\n");
        fprintf(stderr, "  -e dev   Ethernet device\n");
        fprintf(stderr, "  -t port  TCP port (0 = use default)\n");
        fprintf(stderr, "  -u path  Path to UNIX socket\n");
        fprintf(stderr, "  -d path  Path to data file (for IP setting)\n");
        exit(1);
    }
  }

  if(itfcount < 2) {
    fprintf(stderr, "Need at least two interfaces to function as a gateway!\n");
    exit(1);
  }

  /* objects */
  obj[OBJ_IPADDR]   = MBN_OBJ("IP Address", MBN_DATATYPE_NODATA, MBN_DATATYPE_UINT, 4, 0, ~0, 0, ieth[0] ? get_ip() : 0);
  obj[OBJ_CANNODES] = MBN_OBJ("CAN Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_ETHNODES] = MBN_OBJ("Ethernet Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_TCPNODES] = MBN_OBJ("TCP Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);

  if(!verbose) {
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
      exit(1);
    }
    umask(0);

    /* get parent id and a new session id */
    pid = getppid();
    if(setsid() < 0) {
      perror("setsid()");
      exit(1);
    }

    act.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGALRM, &act, NULL);
  }

  /* init can */
  if(ican[0]) {
    if((itf = mbnCANOpen(ican, parent, err)) == NULL) {
      fprintf(stderr, "mbnCANOpen: %s\n", err);
      exit(1);
    }
    this_node.HardwareParent[0] = parent[0];
    this_node.HardwareParent[1] = parent[1];
    this_node.HardwareParent[2] = parent[2];
    if(verbose)
      printf("Received hardware parent from CAN: %04X:%04X:%04X\n",
        parent[0], parent[1], parent[2]);
    if((can = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(can): %s\n", err);
      exit(1);
    }
    setcallbacks(can);
  }

  /* init ethernet */
  if(ieth[0]) {
    if((itf = mbnEthernetOpen(ieth, err)) == NULL) {
      fprintf(stderr, "mbnEthernetOpen: %s\n", err);
      if(can)
        mbnFree(can);
      exit(1);
    }
    if((eth = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(eth): %s\n", err);
      if(can)
        mbnFree(can);
      exit(1);
    }
    setcallbacks(eth);
  }

  /* init TCP */
  if(tport[0]) {
    if((itf = mbnTCPOpen(NULL, NULL, "0.0.0.0", strcmp(tport, "0") ? tport : NULL, err)) == NULL) {
      fprintf(stderr, "mbnTCPOpen: %s\n", err);
      if(can)
        mbnFree(can);
      if(eth)
        mbnFree(eth);
      exit(1);
    }
    if((tcp = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(tcp): %s\n", err);
      if(can)
        mbnFree(can);
      if(eth)
        mbnFree(eth);
      exit(1);
    }
    setcallbacks(tcp);
  }

  if(!verbose) {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    kill(pid, SIGUSR1);
  }
}


int main(int argc, char **argv) {
  char upath[UNIX_PATH_MAX];

  init(argc, argv, upath);
  process_unix(upath);

  if(can)
    mbnFree(can);
  if(eth)
    mbnFree(eth);
  if(tcp)
    mbnFree(tcp);

  return 0;
}


