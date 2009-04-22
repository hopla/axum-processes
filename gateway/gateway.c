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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define MBN_VARARG
#include "mbn.h"

#define DEFAULT_UNIX_PATH "/tmp/axum-gateway"

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX 108
#endif

#define nodestr(n) (n == eth ? "eth" : n == can ? "can" : "tcp")

#define OBJ_CANNODES 0
#define OBJ_ETHNODES 1
#define OBJ_TCPNODES 2


struct mbn_node_info this_node = {
  0x00000000, 0x00, /* MambaNet Addr + Services */
  "Axum CAN Gateway",
  "YorHels Gateway",
  0xFFFF, 0x0002, 0x0001,   /* UniqueMediaAccessId */
  0, 0,    /* Hardware revision */
  1, 1,    /* Firmware revision */
  0, 0,    /* FPGAFirmware revision */
  2,       /* NumberOfObjects */
  0,       /* DefaultEngineAddr */
  {0,0,0}, /* Hardwareparent */
  0        /* Service request */
};

struct mbn_handler *eth, *can, *tcp;


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  printf("OnlineStatus on %s: %08lX %s\n", nodestr(mbn), addr, valid ? "validated" : "invalid");
  if(valid) {
    if(mbn != can) mbnForceAddress(can, addr);
    if(mbn != eth) mbnForceAddress(eth, addr);
    if(mbn != tcp) mbnForceAddress(tcp, addr);
  }
  this_node.MambaNetAddr = addr;
}


void Error(struct mbn_handler *mbn, int code, char *msg) {
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
    if(mbnNodeStatus(can, msg->AddressTo) != NULL)
      dest = can;
    else if(mbnNodeStatus(eth, msg->AddressTo) != NULL)
      dest = eth;
    else if(mbnNodeStatus(tcp, msg->AddressTo) != NULL)
      dest = tcp;

    /* don't forward if we haven't found the destination node */
    if(dest == NULL)
      return 0;

    /* don't forward if the destination is on the same network */
    if(dest == mbn)
      return 0;
  }

  /* print out what's happening */
  printf(" %s %1X %08lX %08lX %03X %1X:", mbn == can ? "<" : ">",
    msg->AcknowledgeReply, msg->AddressTo, msg->AddressFrom, msg->MessageID, msg->MessageType);
  for(i=0;i<msg->bufferlength;i++)
    printf(" %02X", msg->buffer[i]);
  printf("\n");
  fflush(stdout);

  /* forward message */
#define fwd(m) mbnSendMessage(m, msg, MBN_SEND_IGNOREVALID | MBN_SEND_FORCEADDR | MBN_SEND_NOCREATE | MBN_SEND_FORCEID)
  if(dest != NULL)
    fwd(dest);
  else {
    if(mbn != eth) fwd(eth);
    if(mbn != tcp) fwd(tcp);
    if(mbn != can) {
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
  mbnUpdateSensorData(can, obj, count);
  mbnUpdateSensorData(eth, obj, count);
  mbnUpdateSensorData(tcp, obj, count);
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
}


void init(int argc, char **argv, char *upath) {
  struct mbn_interface *itf = NULL;
  struct mbn_object obj[3];
  char err[MBN_ERRSIZE], ican[50], ieth[50], tport[10];
  unsigned short parent[3] = {0,0,0};
  int c, itfcount = 0;

  strcpy(upath, DEFAULT_UNIX_PATH);
  ican[0] = ieth[0] = tport[0] = 0;
  can = eth = tcp = NULL;

  obj[OBJ_CANNODES] = MBN_OBJ("CAN Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_ETHNODES] = MBN_OBJ("Ethernet Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_TCPNODES] = MBN_OBJ("TCP Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);

  while((c = getopt(argc, argv, "c:e:u:t:")) != -1) {
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
      /* wrong option */
      default:
        fprintf(stderr, "Usage: %s [-c dev] [-e dev] [-u path]\n", argv[0]);
        fprintf(stderr, "  -c dev   CAN device\n");
        fprintf(stderr, "  -e dev   Ethernet device\n");
        fprintf(stderr, "  -t port  TCP port (0 = use default)\n");
        fprintf(stderr, "  -u path  Path to UNIX socket\n");
        exit(1);
    }
  }

  if(itfcount < 2) {
    fprintf(stderr, "Need at least two interfaces to function as a gateway!\n");
    exit(1);
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


