
#include "common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/select.h>

#include <mbn.h>
#include <libpq-fe.h>
#include <pthread.h>

#define GET_NUM 5 /* maxumum number of concurrent requests */
#define DEBUG 0

#define DEFAULT_GTW_PATH  "/tmp/axum-gateway"
#define DEFAULT_ETH_DEV   "eth0"
#define DEFAULT_DB_STR    "dbname='axum' user='axum'"
#define DEFAULT_LOG_FILE  "/var/log/axum-learner.log"


struct mbn_node_info this_node = {
  0, 0,
  "MambaNet Learner",
  "Axum Learner Application",
  0x0001, 0x0010, 0x001,
  0, 0, 2, 0,
  0, 0, 0, 0,
  {0,0,0}, 0
};
struct mbn_handler *mbn;

struct get_action {
  char act; /* 0=get sensor data, 1=get object information */
  unsigned long addr;
  unsigned short object;
  char active;
  struct get_action *next;
};
struct get_action *get_queue = NULL;
/* deadlock warning: don't use sql_lock() within this mutex */
pthread_mutex_t get_queue_mutex;

/* simple node info list, temporary place for getting the number
 * ob objects and firmware major revision */
struct node_info {
  unsigned long addr;
  int objects, fwmajor;
} *nodes = NULL;
int nodesl = 0;


char *data2str(unsigned char type, union mbn_data dat) {
  static char d[256];
  switch(type) {
    case MBN_DATATYPE_NODATA: sprintf(d, "[no data]"); break;
    case MBN_DATATYPE_UINT:   sprintf(d, "%lu", dat.UInt); break;
    case MBN_DATATYPE_SINT:   sprintf(d, "%ld", dat.SInt); break;
    case MBN_DATATYPE_STATE:  sprintf(d, "0x%08lX", dat.State); break;
    case MBN_DATATYPE_OCTETS: sprintf(d, "\"%s\"", dat.Octets); break;
    case MBN_DATATYPE_FLOAT:  sprintf(d, "%f", dat.Float); break;
    case MBN_DATATYPE_BITS:
      sprintf(d, "0x%02X%02X%02X%02X%02X%02X%02X%02X",
        dat.Bits[0], dat.Bits[1], dat.Bits[2], dat.Bits[3],
        dat.Bits[4], dat.Bits[5], dat.Bits[6], dat.Bits[7]);
      break;
    case MBN_DATATYPE_OBJINFO: sprintf(d, "[object info]"); break;
    case MBN_DATATYPE_ERROR:   sprintf(d, "ERROR:\"%s\"", dat.Error); break;
    default: sprintf(d, "[unknown datatype]");
  }
  return d;
}


void minmax(char *res, unsigned char type, union mbn_data dat) {
  if(type == MBN_DATATYPE_SINT)
    sprintf(res, "(%ld,)", dat.SInt);
  else if(type != MBN_DATATYPE_FLOAT)
    sprintf(res, "(%ld,)", dat.UInt);
  else
    sprintf(res, "(,%f)", dat.Float);
}


void add_queue(char act, unsigned long addr, unsigned short obj) {
  struct get_action **last;
  pthread_mutex_lock(&get_queue_mutex);
  last = &get_queue;
  while(*last != NULL)
    last = &((*last)->next);
  *last = calloc(1, sizeof(struct get_action));
  (*last)->act = act;
  (*last)->addr = addr;
  (*last)->object = obj;
  pthread_mutex_unlock(&get_queue_mutex);
}


void process_queue() {
  struct get_action *a;
  int i, active=0, sent=0;

  pthread_mutex_lock(&get_queue_mutex);
  for(a=get_queue,i=0; a!=NULL&&i<GET_NUM; a=a->next,i++) {
    if(a->active != 0) {
      active++;
      continue;
    }
    sent++;
    if(a->act == 0)
      mbnGetSensorData(mbn, a->addr, a->object, 1);
    else
      mbnGetObjectInformation(mbn, a->addr, a->object, 1);
    if(DEBUG)
      log_write("GET: %08lX[%5d] %s", a->addr, a->object, a->act ? "object information" : "sensor data");
    a->active = 1;
  }
  pthread_mutex_unlock(&get_queue_mutex);
  if(DEBUG)
    log_write("active = %d, sent = %d", active, sent);
}


int remove_queue(unsigned long addr, unsigned short obj) {
  struct get_action *n, *last;
  int i;

  pthread_mutex_lock(&get_queue_mutex);
  for(n=last=get_queue,i=0; n!=NULL&&i<GET_NUM; n=n->next,i++) {
    if(n->active == 1 && n->addr == addr && n->object == obj)
      break;
    last = n;
  }
  if(n == NULL || i == GET_NUM) {
    pthread_mutex_unlock(&get_queue_mutex);
    return 0;
  }

  if(get_queue == n)
    get_queue = n->next;
  else
    last->next = n->next;
  pthread_mutex_unlock(&get_queue_mutex);

  return 1;
}


int add_node(unsigned long addr) {
  int i;

  pthread_mutex_lock(&get_queue_mutex);
  /* first, check if it's already in the list */
  for(i=0; i<nodesl; i++)
    if(nodes[i].addr == addr)
      break;
  /* otherwise, get a free slot */
  if(i == nodesl)
    for(i=0; i<nodesl; i++)
      if(nodes[i].addr == 0)
        break;
  /* no free slot? reserve more memory */
  if(i == nodesl) {
    nodes = realloc(nodes, sizeof(struct node_info)*nodesl*2);
    memset((void *)nodes+nodesl, 0, sizeof(struct node_info)*nodesl);
    nodesl *= 2;
  }
  /* set address and reset fwmajor and objects */
  nodes[i].addr = addr;
  nodes[i].fwmajor = nodes[i].objects = -1;

  /* and now get the major firmware version and number of objects */
  add_queue(0, addr, MBN_NODEOBJ_FWMAJOR);
  add_queue(0, addr, MBN_NODEOBJ_NUMBEROFOBJECTS);
  pthread_mutex_unlock(&get_queue_mutex);
  return i;
}


void mAddressTableChange(struct mbn_handler *m, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct get_action *n, *a;

  /* online */
  if(old == NULL && new != NULL)
    add_node(new->MambaNetAddr);

  /* offline */
  if(old != NULL && new == NULL) {
    /* remove all queued get operations (this method isn't the most efficient) */
    pthread_mutex_lock(&get_queue_mutex);
    for(n=get_queue; n!=NULL; n=a) {
      a = n->next;
      if(n->addr == old->MambaNetAddr)
        remove_queue(n->addr, n->object);
    }
    pthread_mutex_unlock(&get_queue_mutex);
  }
  return;
  m++;
}


int mSensorDataResponse(struct mbn_handler *m, struct mbn_message *msg, unsigned short obj, unsigned char type, union mbn_data dat) {
  struct mbn_address_node *node, *tmp;
  PGresult *res;
  char str[4][20];
  const char *params[4] = { (const char *)&(str[0]), (const char *)&(str[1]), (const char *)&(str[2]), (const char *)&(str[3]) };
  int n, i;

  log_write("SensorDataResponse: %08lX[%5d] = (%2d) %s", msg->AddressFrom, obj, type, data2str(type, dat));

  if(!remove_queue(msg->AddressFrom, obj))
    return 1;
  if((node = mbnNodeStatus(mbn, msg->AddressFrom)) == NULL)
    return 1;

  /* we should receive both firmware and number of objects, wait
   * for the other to arrive if we only have one of the values */
  for(n=0; n<nodesl; n++)
    if(nodes[n].addr == msg->AddressFrom)
      break;
  if(n == nodesl)
    return 1;

  if(obj == MBN_NODEOBJ_FWMAJOR)
    nodes[n].fwmajor = dat.UInt;
  else
    nodes[n].objects = dat.UInt;
  if(nodes[n].fwmajor == -1 || nodes[n].objects == -1)
    return 0;

  /* doesn't have any objects? ignore */
  if(nodes[n].objects == 0)
    return 0;

  /* ignore this node if we already have another node with the same man/prod/fwmajor. */
  for(i=0; i<nodesl; i++) {
    if(i == n || nodes[i].addr == 0 || nodes[i].fwmajor != nodes[n].fwmajor)
      continue;
    if((tmp = mbnNodeStatus(mbn, nodes[i].addr)) == NULL)
      continue;
    if(tmp->ManufacturerID == node->ManufacturerID && tmp->ProductID == node->ProductID)
      return 0;
  }

  /* we have both, check database for which objects we need to fetch */
  sql_lock(1);
  sprintf(str[0], "%d", nodes[n].objects+1023);
  sprintf(str[1], "%hd", node->ManufacturerID);
  sprintf(str[2], "%hd", node->ProductID);
  sprintf(str[3], "%hd", nodes[n].fwmajor);
  /* sql magic, returns the objects we don't have in the database */
  if((res = sql_exec("SELECT n.a FROM generate_series(1024, $1) AS n(a)\
        WHERE NOT EXISTS(SELECT 1 FROM templates t WHERE man_id = $2 AND prod_id = $3\
        AND firm_major = $4 AND t.number = n.a)", 1, 4, params)) == NULL) {
    sql_lock(0);
    return 0;
  }
  for(i=0; i<PQntuples(res); i++) {
    sscanf(PQgetvalue(res, i, 0), "%d", &n);
    add_queue(1, msg->AddressFrom, n);
  }
  PQclear(res);

  /* delete any objects outside the range, which might have been inserted
   * by someone else or because of a change in the node */
  if((res = sql_exec("DELETE FROM templates WHERE man_id = $2 AND prod_id = $3\
       AND firm_major = $4 AND (number < 1024 OR number > $1)", 0, 4, params)) != NULL)
    PQclear(res);
  sql_lock(0);

  return 0;
  m++;
}


int mObjectInformationResponse(struct mbn_handler *m, struct mbn_message *msg, unsigned short obj, struct mbn_object *nfo) {
  struct mbn_address_node *node;
  char str[15][34];
  const char *params[15];
  int i, fwmajor = -1;

  /* the lacking -e is intentional, to make the log aligned with the other messages */
  log_write("InformationRespons: %08lX[%5d]", msg->AddressFrom, obj);
  if(!remove_queue(msg->AddressFrom, obj))
    return 1;

  /* get firmware major revision and node information */
  for(i=0; i<nodesl; i++)
    if(nodes[i].addr == msg->AddressFrom) {
      fwmajor = nodes[i].fwmajor;
      break;
    }
  if(i == nodesl)
    return 1;
  if((node = mbnNodeStatus(mbn, msg->AddressFrom)) == NULL)
    return 1;

  /* create params list */
  for(i=0; i<15; i++)
    params[i] = (const char *)str[i];
  sprintf(str[0], "%hd", node->ManufacturerID);
  sprintf(str[1], "%hd", node->ProductID);
  sprintf(str[2], "%hd", (short)fwmajor);
  sprintf(str[3], "%hd", obj);
  sprintf(str[4], "%s",  nfo->Description);
  sprintf(str[5], "%hd", nfo->Services);
  sprintf(str[6], "%hd", nfo->SensorType);
  if(nfo->SensorType == MBN_DATATYPE_NODATA)
    params[7] = params[8] = params[9] = NULL;
  else {
    sprintf(str[7], "%d", nfo->SensorSize);
    minmax(str[8], nfo->SensorType, nfo->SensorMin);
    minmax(str[9], nfo->SensorType, nfo->SensorMax);
  }
  sprintf(str[10], "%hd", nfo->ActuatorType);
  if(nfo->ActuatorType == MBN_DATATYPE_NODATA)
    params[11] = params[12] = params[13] = params[14] = NULL;
  else {
    sprintf(str[11], "%d", nfo->ActuatorSize);
    minmax(str[12], nfo->ActuatorType, nfo->ActuatorMin);
    minmax(str[13], nfo->ActuatorType, nfo->ActuatorMax);
    minmax(str[14], nfo->ActuatorType, nfo->ActuatorDefault);
  }

  sql_lock(1);
  sql_exec("INSERT INTO templates (man_id, prod_id, firm_major, number, description, services, sensor_type, sensor_size,\
    sensor_min, sensor_max, actuator_type, actuator_size, actuator_min, actuator_max, actuator_def)\
    VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)", 0, 15, params);
  sql_lock(0);

  return 0;
  m++;
}


void mAcknowledgeTimeout(struct mbn_handler *m, struct mbn_message *msg) {
  log_write("AcknowledgeTimeout: %08lX[%5d] get %s", msg->AddressTo, msg->Message.Object.Number,
    msg->Message.Object.Action == MBN_OBJ_ACTION_GET_INFO ? "object information" : "sensor data");
  remove_queue(msg->AddressTo, msg->Message.Object.Number);
  return;
  m++;
}


void mObjectError(struct mbn_handler *m, struct mbn_message *msg, unsigned short obj, char *err) {
  log_write("ObjectError       : %08lX[%5d] = %s", msg->AddressFrom, obj, err);
  remove_queue(msg->AddressFrom, obj);
  return;
  m++;
}


void template_removed(char myself, char *arg) {
  struct mbn_address_node *a;
  int man, prod, firm;

  if(myself)
    return;
  sscanf(arg, "%d %d %d", &man, &prod, &firm);
  log_write("Detected removal of template objects for %04X_%04X_%02X", man, prod, firm);
  /* look for online nodes matching the man_id and prod_id, and re-fetch their info */
  for(a=NULL; (a=mbnNextNode(mbn, a))!=NULL; )
    if(a->ManufacturerID == man && a->ProductID == prod)
      add_node(a->MambaNetAddr);
}


void init(int argc, char *argv[]) {
  char dbstr[256], ethdev[50], err[MBN_ERRSIZE];
  struct mbn_interface *itf;
  pthread_mutexattr_t mattr;
  int c;

  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&get_queue_mutex, &mattr);

  static struct sql_notify notifies[] = {
    { "template_removed",  template_removed }
  };

  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(dbstr, DEFAULT_DB_STR);
  strcpy(log_file, DEFAULT_LOG_FILE);
  strcpy(hwparent_path, DEFAULT_GTW_PATH);

  while((c = getopt(argc, argv, "e:d:l:g:i:")) != -1) {
    switch(c) {
      case 'e':
        if(strlen(optarg) > 50) {
          fprintf(stderr, "Too long device name.\n");
          exit(1);
        }
        strcpy(ethdev, optarg);
        break;
      case 'd':
        if(strlen(optarg) > 256) {
          fprintf(stderr, "Too long database connection string!\n");
          exit(1);
        }
        strcpy(dbstr, optarg);
        break;
      case 'g':
        strcpy(hwparent_path, optarg);
        break;
      case 'l':
        strcpy(log_file, optarg);
        break;
      case 'i':
        if(sscanf(optarg, "%hd", &(this_node.UniqueIDPerProduct)) != 1) {
          fprintf(stderr, "Invalid UniqueIDPerProduct");
          exit(1);
        }
        break;
      default:
        fprintf(stderr, "Usage: %s [-e dev] [-g path] [-l path] [-d str] [-i id]\n", argv[0]);
        fprintf(stderr, "  -e dev   Ethernet device for MambaNet communication.\n");
        fprintf(stderr, "  -g path  Hardware parent or path to gateway socket.\n");
        fprintf(stderr, "  -l path  Path to log file.\n");
        fprintf(stderr, "  -d str   PostgreSQL database connection options.\n");
        fprintf(stderr, "  -i id    UniqueIDPerProduct for the MambaNet node\n");
        exit(1);
    }
  }

  daemonize();
  log_open();
  hwparent(&this_node);
  sql_open(dbstr, 1, notifies);

  if((itf = mbnEthernetOpen(ethdev, err)) == NULL) {
    fprintf(stderr, "Opening %s: %s\n", ethdev, err);
    sql_close();
    log_close();
    exit(1);
  }
  if((mbn = mbnInit(&this_node, NULL, itf, err)) == NULL) {
    fprintf(stderr, "mbnInit: %s\n", err);
    sql_close();
    log_close();
    exit(1);
  }
  mbnSetAddressTableChangeCallback(mbn, mAddressTableChange);
  mbnSetSensorDataResponseCallback(mbn, mSensorDataResponse);
  mbnSetAcknowledgeTimeoutCallback(mbn, mAcknowledgeTimeout);
  mbnSetObjectInformationResponseCallback(mbn, mObjectInformationResponse);
  mbnSetObjectErrorCallback(mbn, mObjectError);

  daemonize_finish();
  log_write("------------------------");
  log_write("Axum Learner Initialized");
}


int main(int argc, char *argv[]) {
  struct timeval tv;
  fd_set rd;
  init(argc, argv);

  /* init nodes list */
  nodesl = 50;
  nodes = calloc(nodesl, sizeof(struct node_info));

  while(!main_quit) {
    /* poll PG socket (we don't need to receive the events asynchronously,
     * so just polling instead of waiting is fine here) */
    sql_lock(1);
    tv.tv_sec = tv.tv_usec = 0;
    FD_ZERO(&rd);
    FD_SET(PQsocket(sql_conn), &rd);
    if(select(PQsocket(sql_conn)+1, &rd, NULL, NULL, &tv) > 0) {
      PQconsumeInput(sql_conn);
      sql_processnotifies();
    }
    sql_lock(0);

    /* process queue */
    process_queue();
    sleep(1);
  }

  log_write("Closing Learner");
  mbnFree(mbn);
  sql_close();
  log_close();
  return 0;
}

