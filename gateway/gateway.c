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

#define MBN_VARARG
#include "common.h"
#include "if_scan.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mbn.h>
#include <pthread.h>

#define DEFAULT_UNIX_PATH "/tmp/axum-gateway"
#define DEFAULT_DATA_PATH "/etc/conf.d/ip"
#define DEFAULT_LOG_FILE  "/var/log/axum-gateway.log"

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX 108
#endif

#define nodestr(n) (n == eth) ? "eth" : ((n == can) ? "can" : ((n == tcp) ? "tcp" : "udp"))

#define OBJ_IPADDR   0
#define OBJ_IPNET    1
#define OBJ_IPGW     2
#define OBJ_CANNODES 3
#define OBJ_ETHNODES 4
#define OBJ_TCPNODES 5
#define OBJ_UDPNODES 6

#define NR_OF_OBJECTS 7
//Major version 3 = 6 objects
//Major version 4 = 7 objects, added UDP

struct mbn_node_info this_node = {
  0x00000000, 0x00, /* MambaNet Addr + Services */
  "MambaNet CAN+TCP+UDP+Ethernet Gateway",
  "Axum MambaNet Gateway",
  0x0001, 0x000D, 0x0001,   /* UniqueMediaAccessId */
  0, 0,           /* Hardware revision */
  4, 1,           /* Firmware revision */
  0, 0,           /* FPGAFirmware revision */
  NR_OF_OBJECTS,  /* NumberOfObjects */
  0,              /* DefaultEngineAddr */
  {0,0,0},        /* Hardwareparent */
  0               /* Service request */
};

struct mbn_handler *eth, *can, *tcp, *udp;
int verbose;
char ieth[50], data_path[1000];
unsigned int net_ip, net_mask, net_gw;


void net_read() {
  FILE *f;
  char buf[500];
  char ip[20];

  net_ip = net_mask = net_gw = 0;
  if((f = fopen(data_path, "r")) == NULL)
    return;
  while(fgets(buf, 500, f) != NULL) {
    if(sscanf(buf, "net_ip=\" %[0-9.]s \"", ip) == 1) {
      net_ip = ntohl(inet_addr(ip));
      log_write("Read IP=%s", ip);
    }
    if(sscanf(buf, "net_mask=\" %[0-9.]s \"", ip) == 1) {
      net_mask = ntohl(inet_addr(ip));
      log_write("Read subnet mask=%s", ip);
    }
    if(sscanf(buf, "net_gw=\" %[0-9.]s \"", ip) == 1) {
      net_gw = ntohl(inet_addr(ip));
      log_write("Read gateway=%s", ip);
    }
  }
  fclose(f);
}


void net_write() {
  char tmppath[1025], buf[500];
  struct in_addr a;
  FILE *r, *w;

  if((r = fopen(data_path, "r")) == NULL)
    return;
  sprintf(tmppath, "%s~", data_path);
  if((w = fopen(tmppath, "w")) == NULL)
    return;
  while(fgets(buf, 500, r) != NULL) {
    if(!strncmp(buf, "net_ip=\"", 8)) {
      a.s_addr = ntohl(net_ip);
      fprintf(w, "net_ip=\"%s\"\n", inet_ntoa(a));
      log_write("Write IP:%s", inet_ntoa(a));
    } else if(!strncmp(buf, "net_mask=\"", 8)) {
      a.s_addr = ntohl(net_mask);
      fprintf(w, "net_mask=\"%s\"\n", inet_ntoa(a));
      log_write("Write subnet mask:%s", inet_ntoa(a));
    } else if(!strncmp(buf, "net_gw=\"", 8)) {
      a.s_addr = ntohl(net_gw);
      fprintf(w, "net_gw=\"%s\"\n", inet_ntoa(a));
      log_write("Write gateway:%s", inet_ntoa(a));
    } else
      fprintf(w, "%s", buf);
  }
  fclose(r);
  fclose(w);
  if(rename(tmppath, data_path) == -1 && verbose)
    printf("Renaming %s to %s: %s\n", tmppath, data_path, strerror(errno));
}


void SynchroniseDateTime(struct mbn_handler *mbn, time_t time) {
  struct timeval tv;
  tv.tv_usec = 0;
  tv.tv_sec = time;
  settimeofday(&tv, NULL);
  /* Neither POSIX nor Linux provide a nice API to do this, so run hwclock instead
   * NOTE: this command can block a few seconds */
  system("/sbin/hwclock --systohc");
  log_write("Setting system time to %ld, request from %s\n", time, nodestr(mbn));
}


int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data dat) {
  object -= 1024;
  if(mbn != eth || object > OBJ_IPGW)
    return 1;
  net_read();
  if(object == OBJ_IPADDR) net_ip   = dat.UInt;
  if(object == OBJ_IPNET)  net_mask = dat.UInt;
  if(object == OBJ_IPGW)   net_gw   = dat.UInt;
  net_write();

  dat.UInt = net_ip;
  if(eth != NULL) mbnUpdateActuatorData(eth, OBJ_IPADDR+1024, dat);
  if(can != NULL) mbnUpdateActuatorData(can, OBJ_IPADDR+1024, dat);
  if(tcp != NULL) mbnUpdateActuatorData(tcp, OBJ_IPADDR+1024, dat);
  if(udp != NULL) mbnUpdateActuatorData(udp, OBJ_IPADDR+1024, dat);
  dat.UInt = net_mask;
  if(eth != NULL) mbnUpdateActuatorData(eth, OBJ_IPNET+1024, dat);
  if(can != NULL) mbnUpdateActuatorData(can, OBJ_IPNET+1024, dat);
  if(tcp != NULL) mbnUpdateActuatorData(tcp, OBJ_IPNET+1024, dat);
  if(udp != NULL) mbnUpdateActuatorData(udp, OBJ_IPNET+1024, dat);
  dat.UInt = net_gw;
  if(eth != NULL) mbnUpdateActuatorData(eth, OBJ_IPGW+1024, dat);
  if(can != NULL) mbnUpdateActuatorData(can, OBJ_IPGW+1024, dat);
  if(tcp != NULL) mbnUpdateActuatorData(tcp, OBJ_IPGW+1024, dat);
  if(udp != NULL) mbnUpdateActuatorData(udp, OBJ_IPGW+1024, dat);

  /* make the changes active */
  if(object == OBJ_IPGW) {
    system("/etc/rc.d/network rtdown gateway");
    system("/etc/rc.d/network rtup gateway");
  } else
    /* NOTE: we do not call ifdown first, because this will also take
     *   down the ethernet device and all MambaNet communication */
    system("/etc/rc.d/network ifup eth0");

  return 0;
}


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  if(verbose)
    printf("OnlineStatus on %s: %08lX %s\n", nodestr(mbn), addr, valid ? "validated" : "invalid");
  if(valid) {
    if(can != NULL && mbn != can) mbnForceAddress(can, addr);
    if(eth != NULL && mbn != eth) mbnForceAddress(eth, addr);
    if(tcp != NULL && mbn != tcp) mbnForceAddress(tcp, addr);
    if(udp != NULL && mbn != udp) mbnForceAddress(udp, addr);
  }
  this_node.MambaNetAddr = addr;
}


void Error(struct mbn_handler *mbn, int code, char *msg) {
  if(verbose)
    printf("Error(%s, %d, \"%s\")\n", nodestr(mbn), code, msg);
}

void WriteLogMessage(struct mbn_handler *mbn, char *msg) {
  log_write(msg);
  return;
  mbn = NULL;
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
    else if(udp != NULL && mbnNodeStatus(udp, msg->AddressTo) != NULL)
      dest = udp;

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
    if(udp != NULL && mbn != udp) fwd(udp);
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

  obj = (mbn == can) ? OBJ_CANNODES : ((mbn == eth) ? OBJ_ETHNODES : ((mbn == tcp) ? OBJ_TCPNODES : OBJ_UDPNODES));
  obj += 1024;
  if(can != NULL) mbnUpdateSensorData(can, obj, count);
  if(eth != NULL) mbnUpdateSensorData(eth, obj, count);
  if(tcp != NULL) mbnUpdateSensorData(tcp, obj, count);
  if(udp != NULL) mbnUpdateSensorData(udp, obj, count);
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
  mbnSetWriteLogMessageCallback(mbn, WriteLogMessage);
}


void init(int argc, char **argv, char *upath) {
  struct mbn_interface *itf = NULL;
  struct mbn_object obj[NR_OF_OBJECTS];
  char err[MBN_ERRSIZE], ican[50], tport[10], uport[10];
  char remotehost[50], rport[10];
  int c, itfcount = 0;
  char oem_name[32];
  char cmdline[1024];
  int cnt;

  strcpy(upath, DEFAULT_UNIX_PATH);
  strcpy(data_path, DEFAULT_DATA_PATH);
  strcpy(log_file, DEFAULT_LOG_FILE);
  ican[0] = ieth[0] = tport[0] = uport[0] = remotehost[0] = rport[0] = 0;
  can = eth = tcp = udp = NULL;
  verbose = 0;

  while((c = getopt(argc, argv, "c:e:u:t:s:h:r:d:i:p:l:v")) != -1) {
    switch(c) {
      /* can interface */
      case 'c':
        if(strlen(optarg) > 49) {
          fprintf(stderr, "CAN interface/TTY device name too long\n");
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
      case 's':
        if (strlen(optarg) > 9) {
          fprintf(stderr, "UDP port too long\n");
          exit(1);
        }
        strcpy(uport, optarg);
        itfcount++;
        break;
      case 'h':
        if (strlen(optarg) > 49) {
          fprintf(stderr, "Hostname too long\n");
          exit(1);
        }
        strcpy(remotehost, optarg);
        itfcount++;
        break;
      case 'r':
        if (strlen(optarg) > 9) {
          fprintf(stderr, "UDP port too long\n");
          exit(1);
        }
        strcpy(rport, optarg);
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
      /* uniqueidperproduct */
      case 'i':
        if(sscanf(optarg, "%hd", &(this_node.UniqueIDPerProduct)) != 1) {
          fprintf(stderr, "Invalid UniqueIDPerProduct\n");
          exit(1);
        }
        break;
      /* hardwareparent */
      case 'p':
        this_node.ProductID = 25; //UI ProductID;
        if(strcmp(optarg, "self") == 0) {
          this_node.HardwareParent[0] = this_node.ManufacturerID;
          this_node.HardwareParent[1] = this_node.ProductID;
          this_node.HardwareParent[2] = this_node.UniqueIDPerProduct;
        } else if(sscanf(optarg, "%04hx:%04hx:%04hx", this_node.HardwareParent,
            this_node.HardwareParent+1, this_node.HardwareParent+2) != 3) {
          fprintf(stderr, "Invalid HardwareParent\n");
          exit(1);
        }
        break;
      case 'l':
        strcpy(log_file, optarg);
        break;
      /* verbose */
      case 'v':
        verbose++;
        break;
      /* wrong option */
      default:
        fprintf(stderr, "Usage: %s [-v] [-c dev] [-e dev] [-t port] [-s port] [-h hostname] [-r hostpor] [-u path] [-d path] [-i id] [-p id]\n", argv[0]);
        fprintf(stderr, "  -v           Print verbose output to stdout\n");
        fprintf(stderr, "  -c dev       CAN device or TTY device\n");
        fprintf(stderr, "  -e dev       Ethernet device\n");
        fprintf(stderr, "  -t port      TCP listen port (0 = use default)\n");
        fprintf(stderr, "  -s port      UDP listen port (0 = use default)\n");
        fprintf(stderr, "  -h hostname  Host to connect via UDP\n");
        fprintf(stderr, "  -r hostport  Hostport to connect via UDP\n");
        fprintf(stderr, "  -u path      Path to UNIX socket\n");
        fprintf(stderr, "  -d path      Path to data file (for IP setting)\n");
        fprintf(stderr, "  -p id        Hardware Parent (not specified = from CAN, 'self' = own ID)\n");
        fprintf(stderr, "  -i id        UniqueIDPerProduct for the MambaNet node\n");
        fprintf(stderr, "  -l path      Path to log file.\n");
        exit(1);
    }
  }

  if(itfcount < 2) {
    fprintf(stderr, "Need at least two interfaces to function as a gateway!\n");
    exit(1);
  }

  if(!verbose)
    log_open();

  log_write("------------------------------------------------");
  log_write("Try to start the %s", this_node.Name);
  log_write("Version %d.%d, compiled at %s (%s)", this_node.FirmwareMajorRevision, this_node.FirmwareMinorRevision, __DATE__, __TIME__);
  sprintf(cmdline, "command line:");
  for (cnt=0; cnt<argc; cnt++)
  {
    strcat(cmdline, " ");
    strcat(cmdline, argv[cnt]);
  }
  log_write(cmdline);
  log_write(mbnVersion());

  net_read();
  /* objects */
  obj[OBJ_IPADDR]   = MBN_OBJ("IP Address", MBN_DATATYPE_NODATA, MBN_DATATYPE_UINT, 4, 0, ~0, 0, net_ip);
  obj[OBJ_IPNET]    = MBN_OBJ("IP Netmask", MBN_DATATYPE_NODATA, MBN_DATATYPE_UINT, 4, 0, ~0, 0, net_mask);
  obj[OBJ_IPGW]     = MBN_OBJ("IP Gateway", MBN_DATATYPE_NODATA, MBN_DATATYPE_UINT, 4, 0, ~0, 0, net_gw);
  obj[OBJ_CANNODES] = MBN_OBJ("CAN Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_ETHNODES] = MBN_OBJ("Ethernet Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_TCPNODES] = MBN_OBJ("TCP Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);
  obj[OBJ_UDPNODES] = MBN_OBJ("UDP Online Nodes", MBN_DATATYPE_UINT, 0, 2, 0, 1000, 0, MBN_DATATYPE_NODATA);

  if(!verbose)
    daemonize();

  if (oem_name_short(oem_name, 32))
  {
    strncpy(this_node.Name, oem_name, 32);
    strcat(this_node.Name, " MambaNet Gateway");
  }

  /* init can interface */
  if(ican[0]) {
    if((itf = mbnCANOpen(ican, this_node.HardwareParent, err)) == NULL) {
      fprintf(stderr, "mbnCANOpen: %s\n", err);
      log_close();
      exit(1);
    }
    if(verbose)
      printf("Received hardware parent from CAN: %04X:%04X:%04X\n",
        this_node.HardwareParent[0], this_node.HardwareParent[1], this_node.HardwareParent[2]);
    if((can = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(can): %s\n", err);
      log_close();
      exit(1);
    }
    setcallbacks(can);

    //start interface for the mbn-handler
    mbnStartInterface(itf, err);
    log_write("CAN interface started (%s)", ican);
  }

  /* init ethernet */
  if(ieth[0]) {
    if((itf = mbnEthernetOpen(ieth, err)) == NULL) {
      fprintf(stderr, "mbnEthernetOpen: %s\n", err);
      if(can)
        mbnFree(can);
      log_close();
      exit(1);
    }
    if((eth = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(eth): %s\n", err);
      if(can)
        mbnFree(can);
      log_close();
      exit(1);
    }
    setcallbacks(eth);

    //start interface for the mbn-handler
    mbnStartInterface(itf, err);
    log_write("Ethernet interface started (%s)", ieth);
  }

  /* init TCP */
  if(tport[0]) {
    if((itf = mbnTCPOpen(NULL, NULL, "0.0.0.0", strcmp(tport, "0") ? tport : NULL, err)) == NULL) {
      fprintf(stderr, "mbnTCPOpen: %s\n", err);
      if(can)
        mbnFree(can);
      if(eth)
        mbnFree(eth);
      log_close();
      exit(1);
    }
    if((tcp = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(tcp): %s\n", err);
      if(can)
        mbnFree(can);
      if(eth)
        mbnFree(eth);
      log_close();
      exit(1);
    }
    setcallbacks(tcp);

    //start interface for the mbn-handler
    mbnStartInterface(itf, err);
    log_write("TCP interface started listening on port %s", strcmp(tport, "0") ? tport : "34848");
  }

  if(uport[0] || remotehost[0]) {
    if((itf = mbnUDPOpen(remotehost[0] != 0 ? remotehost : NULL, strcmp(rport, "0") ? rport: NULL, strcmp(uport, "0") ? uport : "34848", err)) == NULL) {
      fprintf(stderr, "mbnUDPOpen: %s\n", err);
      if(tcp)
        mbnFree(tcp);
      if(can)
        mbnFree(can);
      if(eth)
        mbnFree(eth);
      log_close();
      exit(1);
    }
    if((udp = mbnInit(&this_node, obj, itf, err)) == NULL) {
      fprintf(stderr, "mbnInit(udp): %s\n", err);
      if(tcp)
        mbnFree(tcp);
      if(can)
        mbnFree(can);
      if(eth)
        mbnFree(eth);
      log_close();
      exit(1);
    }
    setcallbacks(udp);

    //start interface for the mbn-handler
    mbnStartInterface(itf, err);
    log_write("UDP interface started listening on port %s", strcmp(uport, "0") ? uport : "34848");
  }

  if(!verbose)
    daemonize_finish();

  log_write("Axum Gateway Initialized");
}


int main(int argc, char **argv) {
  char upath[UNIX_PATH_MAX];

  init(argc, argv, upath);
  process_unix(upath);

  log_write("Closing gateway");

  if(can)
    mbnFree(can);
  if(eth)
    mbnFree(eth);
  if(tcp)
    mbnFree(tcp);
  if(udp)
    mbnFree(udp);

  log_close();

  return 0;
}


