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

#include "mbn.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>


#define nodestr(n) (n == eth ? "eth" : "can")


struct mbn_node_info this_node = {
  0x00000000, 0x00, /* MambaNet Addr + Services */
  "Axum CAN Gateway",
  "YorHels Gateway",
  0xFFFF, 0x0002, 0x0001,   /* UniqueMediaAccessId */
  0, 0,          /* Hardware revision */
  0, 1,          /* Firmware revision */
  0, 0,          /* FPGAFirmware revision */
  0,             /* NumberOfObjects */
  0,             /* DefaultEngineAddr */
  {0,0,0,0,0,0}, /* Hardwareparent */
  0              /* Service request */
};

struct mbn_handler *eth, *can;


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  printf("OnlineStatus on %s: %08lX %s\n", nodestr(mbn), addr, valid ? "validated" : "invalid");
  if(valid) {
    if(mbn != can) mbnForceAddress(can, addr);
    if(mbn != eth) mbnForceAddress(eth, addr);
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

    /* don't forward if we haven't found the destination node */
    if(dest == NULL)
      return 0;
  }

  /* filter out address reservation packets to can */
  if(dest == NULL && msg->MessageType == MBN_MSGTYPE_ADDRESS && msg->Message.Address.Action == MBN_ADDR_ACTION_INFO)
    return 0;

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
    if(mbn != can) fwd(can);
  }
#undef fwd
  return 0;
}


int main(int argc, char **argv) {
  struct mbn_interface *itf = NULL;
  char err[MBN_ERRSIZE];
  int c;

  can = eth = NULL;

  while((c = getopt(argc, argv, "c:e:")) != -1) {
    switch(c) {
      /* can interface */
      case 'c':
        if((itf = mbnCANOpen("can0", err)) == NULL) {
          printf("mbnCANOpen: %s\n", err);
          return 1;
        }
        if((can = mbnInit(&this_node, NULL, itf, err)) == NULL) {
          printf("mbnInit(can): %s", err);
          return 1;
        }
        mbnSetErrorCallback(can, Error);
        mbnSetReceiveMessageCallback(can, ReceiveMessage);
        break;
      /* ethernet interface */
      case 'e':
        if((itf = mbnEthernetOpen("eth0", err)) == NULL) {
          printf("mbnEthernetOpen: %s\n", err);
          return 1;
        }
        if((eth = mbnInit(&this_node, NULL, itf, err)) == NULL) {
          printf("mbnInit(eth): %s", err);
          return 1;
        }
        mbnSetErrorCallback(eth, Error);
        mbnSetOnlineStatusCallback(eth, OnlineStatus);
        mbnSetReceiveMessageCallback(eth, ReceiveMessage);
        break;
      /* wrong option */
      default:
        return 1;
    }
  }

  if(!can || !eth) {
    printf("Need at least two interfaces to function as a gateway!\n");
    return 1;
  }

  while(getchar() != 'q')
    continue;

  if(can)
    mbnFree(can);
  if(eth)
    mbnFree(eth);

  return 0;
}


