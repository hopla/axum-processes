
#include "conn.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#include <mbn.h>
#include <unistd.h>
#include <signal.h>

#define DEFAULT_UNIX_PATH "/tmp/axum-address"
#define DEFAULT_ETH_DEV   "eth0"
#define DEFAULT_DB_PATH   "/var/lib/axum/axum-address.sqlite3"
#define DEFAULT_LOG_FILE  "/var/log/axum-address.log"

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
  {0,0,0}, /* parent */
  0 /* service */
};


/* global variables */
struct mbn_handler *mbn;
char logfile[256];
FILE *logfd;
volatile int main_quit;


void writelog(char *fmt, ...) {
  va_list ap;
  char buf[400], tm[20];
  time_t t = time(NULL);
  if(logfd == NULL)
    return;
  va_start(ap, fmt);
  vsnprintf(buf, 400, fmt, ap);
  va_end(ap);
  strftime(tm, 20, "%Y-%m-%d %H:%M:%S", gmtime(&t));
  fprintf(logfd, "[%s] %s\n", tm, buf);
  fflush(logfd);
}


void node_online(struct db_node *node) {
  struct db_node addr, id;
  struct mbn_message reply;
  int addr_f, id_f;

  /* check DB for MambaNet Address and UniqueID */
  memset((void *)&addr, 0, sizeof(struct db_node));
  memset((void *)&id, 0, sizeof(struct db_node));
  addr_f = db_getnode(&addr, node->MambaNetAddr);
  id_f = db_searchnodes(node, DB_MANUFACTURERID|DB_PRODUCTID|DB_UNIQUEID, 1, 0, 0, &id);

  /* Reset node address when the previous search didn't return the exact same node */
  if((addr_f && !id_f) || (!addr_f && id_f)
      || (addr_f && id_f && memcmp((void *)&addr, (void *)&id, sizeof(struct db_node)) != 0)) {
    writelog("Address mismatch for %04X:%04X:%04X (%08lX), resetting valid bit",
      node->ManufacturerID, node->ProductID, node->UniqueIDPerProduct, node->MambaNetAddr);
    reply.MessageType = MBN_MSGTYPE_ADDRESS;
    reply.AddressTo = MBN_BROADCAST_ADDRESS;
    reply.Message.Address.Action = MBN_ADDR_ACTION_RESPONSE;
    reply.Message.Address.ManufacturerID = node->ManufacturerID;
    reply.Message.Address.ProductID = node->ProductID;
    reply.Message.Address.UniqueIDPerProduct = node->UniqueIDPerProduct;
    reply.Message.Address.Services = node->Services & ~MBN_ADDR_SERVICES_VALID;
    reply.Message.Address.MambaNetAddr = 0;
    reply.Message.Address.EngineAddr = node->EngineAddr;
    mbnSendMessage(mbn, &reply, MBN_SEND_IGNOREVALID);
    return;
  }

  /* not in the DB at all? Add it. */
  if(!addr_f) {
    writelog("New validated node found on the network but not in DB: %08lX (%04X:%04X:%04X)",
      node->MambaNetAddr, node->ManufacturerID, node->ProductID, node->UniqueIDPerProduct);
    db_setnode(0, node);
  }
  /* we don't have its name? get it! */
  if(!addr_f || addr.Name[0] == 0) {
    mbnGetActuatorData(mbn, node->MambaNetAddr, MBN_NODEOBJ_NAME, 1);
    mbnGetSensorData(mbn, node->MambaNetAddr, MBN_NODEOBJ_HWPARENT, 1);
  }
  /* not active? update! */
  if(addr_f && !addr.Active) {
    addr.Active = 1;
    db_setnode(node->MambaNetAddr, &addr);
  }
}


void mAddressTableChange(struct mbn_handler *m, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct db_node node;

  /* new node online, check with the DB */
  if(!old && new) {
    node.Name[0] = 0;
    node.Parent[0] = node.Parent[1] = node.Parent[2] = 0;
    node.ManufacturerID = new->ManufacturerID;
    node.ProductID = new->ProductID;
    node.UniqueIDPerProduct = new->UniqueIDPerProduct;
    node.MambaNetAddr = new->MambaNetAddr;
    node.EngineAddr = new->EngineAddr;
    node.Services = new->Services;
    node.Active = 1;
    node_online(&node);
  }

  /* node went offline, update status */
  if(old && !new) {
    if(db_getnode(&node, old->MambaNetAddr) && node.Active) {
      node.Active = 0;
      db_setnode(old->MambaNetAddr, &node);
    }
  }
  m++;
}


int mSensorDataResponse(struct mbn_handler *m, struct mbn_message *msg, unsigned short obj, unsigned char type, union mbn_data dat) {
  struct db_node node;
  unsigned char *p = dat.Octets;

  if(obj != MBN_NODEOBJ_HWPARENT || type != MBN_DATATYPE_OCTETS)
    return 1;
  if(!db_getnode(&node, msg->AddressFrom))
    return 1;

  node.Parent[0] = (unsigned short)(p[0]<<8) + p[1];
  node.Parent[1] = (unsigned short)(p[2]<<8) + p[3];
  node.Parent[2] = (unsigned short)(p[4]<<8) + p[5];
  writelog("Received hardware parent of %08lX: %04X:%04X:%04X",
    msg->AddressFrom, node.Parent[0], node.Parent[1], node.Parent[2]);
  db_setnode(msg->AddressFrom, &node);
  return 0;
  m++;
}


int mActuatorDataResponse(struct mbn_handler *m, struct mbn_message *msg, unsigned short obj, unsigned char type, union mbn_data dat) {
  struct db_node node;

  if(obj != MBN_NODEOBJ_NAME || type != MBN_DATATYPE_OCTETS)
    return 1;
  if(!db_getnode(&node, msg->AddressFrom))
    return 1;

  strncpy(node.Name, (char *)dat.Octets, 32);
  writelog("Received name of %08lX: %s", msg->AddressFrom, node.Name);
  db_setnode(msg->AddressFrom, &node);
  return 0;
  m++;
}


int mReceiveMessage(struct mbn_handler *m, struct mbn_message *msg) {
  struct mbn_message_address *nfo = &(msg->Message.Address);
  struct db_node node, res;
  struct mbn_message reply;

  /* ignore everything but address information messages without validated address */
  if(!(msg->MessageType == MBN_MSGTYPE_ADDRESS && nfo->Action != MBN_ADDR_ACTION_INFO &&
       (nfo->MambaNetAddr == 0 || !(nfo->Services & MBN_ADDR_SERVICES_VALID))))
    return 0;

  /* create a default reply message, MambaNetAddr and EngineAddr will need to be filled in */
  reply.MessageType = MBN_MSGTYPE_ADDRESS;
  reply.AddressTo = MBN_BROADCAST_ADDRESS;
  reply.Message.Address.Action = MBN_ADDR_ACTION_RESPONSE;
  reply.Message.Address.ManufacturerID = nfo->ManufacturerID;
  reply.Message.Address.ProductID = nfo->ProductID;
  reply.Message.Address.UniqueIDPerProduct = nfo->UniqueIDPerProduct;
  reply.Message.Address.Services = nfo->Services | MBN_ADDR_SERVICES_VALID;

  /* search for UniqueMediaAccessID in the DB */
  node.ManufacturerID = nfo->ManufacturerID;
  node.ProductID = nfo->ProductID;
  node.UniqueIDPerProduct = nfo->UniqueIDPerProduct;

  /* found it? reply with its old address */
  if(db_searchnodes(&node, DB_MANUFACTURERID | DB_PRODUCTID | DB_UNIQUEID, 1, 0, 0, &res)) {
    reply.Message.Address.MambaNetAddr = res.MambaNetAddr;
    reply.Message.Address.EngineAddr = res.EngineAddr;
    writelog("Address request of %04X:%04X:%04X, sent %08lX",
      node.ManufacturerID, node.ProductID, node.UniqueIDPerProduct, res.MambaNetAddr);
  } else {
    /* not found, get new address and insert into the DB */
    node.MambaNetAddr = db_newaddress();
    node.Services = nfo->Services;
    node.Name[0] = node.Active = node.EngineAddr = 0;
    node.Parent[0] = node.Parent[1] = node.Parent[2] = 0;
    db_setnode(0, &node);
    reply.Message.Address.MambaNetAddr = node.MambaNetAddr;
    reply.Message.Address.EngineAddr = node.EngineAddr;
    writelog("New node added to the network: %08lX (%04X:%04X:%04X)",
      node.MambaNetAddr, node.ManufacturerID, node.ProductID, node.UniqueIDPerProduct);
  }

  /* send the reply */
  mbnSendMessage(m, &reply, MBN_SEND_IGNOREVALID);
  return 1;
}


void init(int argc, char **argv) {
  struct mbn_interface *itf;
  char err[MBN_ERRSIZE];
  char ethdev[50];
  char dbpath[256];
  char upath[UNIX_PATH_MAX];
  int c, forcelisten = 0;

  strcpy(upath, DEFAULT_UNIX_PATH);
  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(dbpath, DEFAULT_DB_PATH);
  strcpy(logfile, DEFAULT_LOG_FILE);

  /* parse options */
  while((c = getopt(argc, argv, "e:u:d:l:f")) != -1) {
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
        strcpy(upath, optarg);
        break;
      case 'f':
        forcelisten = 1;
        break;
      case 'd':
        if(strlen(optarg) > 256) {
          fprintf(stderr, "Too long path to sqlite3 DB!");
          exit(1);
        }
        strcpy(dbpath, optarg);
        break;
      case 'l':
        if(strlen(optarg) > 256) {
          fprintf(stderr, "Too long path to log file!");
          exit(1);
        }
        strcpy(logfile, optarg);
        break;
      default:
        fprintf(stderr, "Usage: %s [-f] [-e dev] [-u path] [-d path]\n", argv[0]);
        fprintf(stderr, "  -f       Force listen on UNIX socket.\n");
        fprintf(stderr, "  -e dev   Ethernet device for MambaNet communication.\n");
        fprintf(stderr, "  -u path  Path to UNIX socket.\n");
        fprintf(stderr, "  -l path  Path to log file.\n");
        fprintf(stderr, "  -d path  Path to SQLite3 database file.\n");
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
  mbnForceAddress(mbn, 0x0001FFFF);
  mbnSetAddressTableChangeCallback(mbn, mAddressTableChange);
  mbnSetSensorDataResponseCallback(mbn, mSensorDataResponse);
  mbnSetActuatorDataResponseCallback(mbn, mActuatorDataResponse);
  mbnSetReceiveMessageCallback(mbn, mReceiveMessage);

  /* initialize UNIX listen socket */
  if(conn_init(upath, forcelisten, err)) {
    fprintf(stderr, "%s", err);
    fprintf(stderr, "Are you sure no other address server is running?\n");
    fprintf(stderr, "Use -f to ignore this error and open the socket anyway.\n");
    mbnFree(mbn);
    exit(1);
  }

  /* open database */
  if(db_init(dbpath, err)) {
    fprintf(stderr, "%s", err);
    conn_free();
    mbnFree(mbn);
    exit(1);
  }

  /* open log file */
  if((logfd = fopen(logfile, "a")) == NULL) {
    perror("Opening log file");
    conn_free();
    mbnFree(mbn);
    db_free();
    exit(1);
  }
  writelog("-------------------------------");
  writelog("Axum Address Server Initialized");
}


void trapsig(int sig) {
  if(sig != SIGHUP) {
    main_quit = sig;
    return;
  }
  writelog("SIGHUP received, re-opening log file");
  fclose(logfd);
  logfd = fopen(logfile, "a");
}


int main(int argc, char **argv) {
  struct sigaction act;

  act.sa_handler = trapsig;
  act.sa_flags = 0;
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGHUP, &act, NULL);
  main_quit = 0;

  init(argc, argv);
  while(!main_quit && !conn_loop())
    ;

  /* free */
  writelog("Closing Address Server");
  conn_free();
  mbnFree(mbn);
  db_free();
  fclose(logfd);
  return 0;
}

