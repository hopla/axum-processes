
#include "main.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <json.h>
#include <mbn.h>

#define MAX_CONNECTIONS   10
#define READBUFSIZE     2048 /* should be large enough to store one command */
#define WRITEBUFSIZE   16384 /* should be large enough to store several replies */
 /* required memory for buffers = (MAX_CONNECTIONS+1) * (READBUFSIZE + WRITEBUFSIZE) */

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


int hex2int(const char *hex, int len) {
  int i, r = 0;
  for(i=0; i<len; i++) {
    r <<= 4;
    if(hex[i] >= '0' && hex[i] <= '9')
      r += hex[i]-'0';
    else if(hex[i] >= 'a' && hex[i] <= 'f')
      r += hex[i]-'a'+10;
    else if(hex[i] >= 'A' && hex[i] <= 'F')
      r += hex[i]-'A'+10;
  }
  return r;
}


int conn_parsefilter(struct json_object *arg, int client, struct db_node *match, int *fields) {
  struct json_object *obj;
  char *str;

  *fields = 0;
  if((obj = json_object_object_get(arg, "Name")) != NULL) {
    strcpy(match->Name, json_object_get_string(obj));
    *fields |= DB_NAME;
  }
  if((obj = json_object_object_get(arg, "UniqueID")) != NULL) {
    str = json_object_get_string(obj);
    if(strlen(str) != 14) {
      conn_send(client, "ERROR {\"msg\":\"Incorrect UniqueID\"}");
      return 1;
    }
    match->ManufacturerID     = hex2int(&(str[ 0]), 4);
    match->ProductID          = hex2int(&(str[ 5]), 4);
    match->UniqueIDPerProduct = hex2int(&(str[10]), 4);
    if(match->ManufacturerID)     *fields |= DB_MANUFACTURERID;
    if(match->ProductID)          *fields |= DB_PRODUCTID;
    if(match->UniqueIDPerProduct) *fields |= DB_UNIQUEID;
  }
  if((obj = json_object_object_get(arg, "Services")) != NULL) {
    match->Services = json_object_get_int(obj);
    *fields |= DB_SERVICES;
  }
  if((obj = json_object_object_get(arg, "Active")) != NULL) {
    match->Active = json_object_get_boolean(obj);
    *fields |= DB_ACTIVE;
  }
  if((obj = json_object_object_get(arg, "MambaNetAddr")) != NULL) {
    str = json_object_get_string(obj);
    if(strlen(str) != 8) {
      conn_send(client, "ERROR {\"msg\":\"Incorrect MambaNetAddr\"}");
      return 1;
    }
    match->MambaNetAddr = hex2int(str, 8);
    *fields |= DB_MAMBANETADDR;
  }
  if((obj = json_object_object_get(arg, "EngineAddr")) != NULL) {
    str = json_object_get_string(obj);
    if(strlen(str) != 8) {
      conn_send(client, "ERROR {\"msg\":\"Incorrect EngineAddr\"}");
      return 1;
    }
    match->EngineAddr = hex2int(str, 8);
    *fields |= DB_ENGINEADDR;
  }
  if((obj = json_object_object_get(arg, "Parent")) != NULL) {
    str = json_object_get_string(obj);
    if(strlen(str) != 14) {
      conn_send(client, "ERROR {\"msg\":\"Incorrect Parent ID\"}");
      return 1;
    }
    match->Parent[0] = hex2int(&(str[ 0]), 4);
    match->Parent[1] = hex2int(&(str[ 5]), 4);
    match->Parent[2] = hex2int(&(str[10]), 4);
    *fields |= DB_PARENT;
  }
  return 0;
}


void conn_cmd_get(int client, struct json_object *arg) {
  struct json_object *obj, *arr;
  struct db_node match, *res;
  char tmp[WRITEBUFSIZE], *str;
  int i, n, fields,
      limit = 1,
      offset = 0,
      order = 0;

  /* basic options */
  if((obj = json_object_object_get(arg, "limit")) != NULL)
    limit = json_object_get_int(obj);
  if((obj = json_object_object_get(arg, "offset")) != NULL)
    offset = json_object_get_int(obj);
  if((obj = json_object_object_get(arg, "order")) != NULL) {
    str = json_object_get_string(obj);
    if(strstr(str, " DESC"))        order |= DB_DESC;
    if(strstr(str, "Name"))         order |= DB_NAME;
    if(strstr(str, "UniqueID"))     order |= DB_MANUFACTURERID | DB_PRODUCTID | DB_UNIQUEID;
    if(strstr(str, "MambaNetAddr")) order |= DB_MAMBANETADDR;
    if(strstr(str, "Services"))     order |= DB_SERVICES;
    if(strstr(str, "Parent"))       order |= DB_PARENT;
    if(strstr(str, "Active"))       order |= DB_ACTIVE;
    if(strstr(str, "EngineAddr"))   order |= DB_ENGINEADDR;
  }

  /* filters */
  if(conn_parsefilter(arg, client, &match, &fields))
    return;

  /* perform search */
  if(limit <= 0 || limit > 100)
    limit = 1;
  res = malloc(sizeof(struct db_node)*(limit+1));
  n = db_searchnodes(&match, fields, limit, offset, order, res);

  /* convert to JSON */
  arg = json_object_new_object();
  arr = json_object_new_array();
  json_object_object_add(arg, "result", arr);
  for(i=0; i<n; i++) {
    obj = json_object_new_object();
    json_object_object_add(obj, "Name", json_object_new_string(res[i].Name));
    sprintf(tmp, "%08lX", res[i].MambaNetAddr);
    json_object_object_add(obj, "MambaNetAddr", json_object_new_string(tmp));
    sprintf(tmp, "%04X:%04X:%04X", res[i].ManufacturerID, res[i].ProductID, res[i].UniqueIDPerProduct);
    json_object_object_add(obj, "UniqueID", json_object_new_string(tmp));
    sprintf(tmp, "%04X:%04X:%04X", res[i].Parent[0], res[i].Parent[1], res[i].Parent[2]);
    json_object_object_add(obj, "Parent", json_object_new_string(tmp));
    sprintf(tmp, "%08lX", res[i].EngineAddr);
    json_object_object_add(obj, "EngineAddr", json_object_new_string(tmp));
    json_object_object_add(obj, "Services", json_object_new_int(res[i].Services));
    json_object_object_add(obj, "Active", json_object_new_boolean(res[i].Active));
    json_object_array_add(arr, obj);
  }

  /* serialize & send */
  snprintf(tmp, WRITEBUFSIZE, "NODES %s", json_object_to_json_string(arg));
  conn_send(client, tmp);
  json_object_put(arg);
}


void conn_cmd_setname(int client, struct json_object *arg) {
  struct json_object *obj;
  struct db_node node;
  union mbn_data dat;
  char *str;
  unsigned long addr;

  if((obj = json_object_object_get(arg, "MambaNetAddr")) == NULL)
    return conn_send(client, "ERROR {\"msg\":\"No MambaNetAddr specified\"}");
  str = json_object_get_string(obj);
  if(strlen(str) != 8)
    return conn_send(client, "ERROR {\"msg\":\"Incorrect MambaNetAddr\"}");
  addr = hex2int(str, 8);

  if(!db_getnode(&node, addr))
    return conn_send(client, "ERROR {\"msg\":\"Node not found\"}");

  if((obj = json_object_object_get(arg, "Name")) == NULL)
    return conn_send(client, "ERROR {\"msg\":\"No name specified\"}");
  str = json_object_get_string(obj);
  if(strlen(str) > 31)
    return conn_send(client, "ERROR {\"msg\":\"Name too long\"}");

  writelog("Renaming node %08lX to \"%s\"", addr, str);
  memset((void *)node.Name, 0, 32);
  strcpy(node.Name, str);
  dat.Octets = (unsigned char *)str;
  if(node.Active)
    mbnSetActuatorData(mbn, addr, MBN_NODEOBJ_NAME, MBN_DATATYPE_OCTETS, 32, dat, 1);
  else
    node.flags |= DB_FLAGS_SETNAME;
  db_setnode(addr, &node);
  conn_send(client, "OK {}");
}


void conn_cmd_setengine(int client, struct json_object *arg) {
  struct json_object *obj;
  struct db_node node;
  union mbn_data dat;
  char *str;
  unsigned long addr;

  if((obj = json_object_object_get(arg, "MambaNetAddr")) == NULL)
    return conn_send(client, "ERROR {\"msg\":\"No MambaNetAddr specified\"}");
  str = json_object_get_string(obj);
  if(strlen(str) != 8)
    return conn_send(client, "ERROR {\"msg\":\"Incorrect MambaNetAddr\"}");
  addr = hex2int(str, 8);

  if(!db_getnode(&node, addr))
    return conn_send(client, "ERROR {\"msg\":\"Node not found\"}");

  if((obj = json_object_object_get(arg, "EngineAddr")) == NULL)
    return conn_send(client, "ERROR {\"msg\":\"No engine address specified\"}");
  str = json_object_get_string(obj);
  if(strlen(str) != 8)
    return conn_send(client, "ERROR {\"msg\":\"Incorrect engine address\"}");
  node.EngineAddr = hex2int(str, 8);

  writelog("Setting engine address of %08lX to %08lX", addr, node.EngineAddr);
  dat.UInt = node.EngineAddr;
  if(node.Active)
    mbnSetActuatorData(mbn, addr, MBN_NODEOBJ_ENGINEADDRESS, MBN_DATATYPE_UINT, 4, dat, 1);
  db_setnode(addr, &node);
  conn_send(client, "OK {}");
}


void conn_cmd_refresh(int client, struct json_object *arg) {
  struct db_node match, res[200];
  int fields, i, n;
  if(conn_parsefilter(arg, client, &match, &fields))
    return;
  if((n = db_searchnodes(&match, fields, 200, 0, 0, res)) < 1)
    return conn_send(client, "ERROR {\"msg\":\"No nodes found\"}");
  for(i=0; i<n; i++) {
    if(res[i].Active) {
      mbnGetActuatorData(mbn, res[i].MambaNetAddr, MBN_NODEOBJ_NAME, 1);
      mbnGetSensorData(mbn, res[i].MambaNetAddr, MBN_NODEOBJ_HWPARENT, 1);
    }
    res[i].flags |= DB_FLAGS_REFRESH;
    db_setnode(res[i].MambaNetAddr, &(res[i]));
  }
  conn_send(client, "OK {}");
}


void conn_cmd_remove(int client, struct json_object *arg) {
  struct json_object *obj;
  struct db_node node;
  char *str;
  unsigned long addr;

  if((obj = json_object_object_get(arg, "MambaNetAddr")) == NULL)
    return conn_send(client, "ERROR {\"msg\":\"No MambaNetAddr specified\"}");
  str = json_object_get_string(obj);
  if(strlen(str) != 8)
    return conn_send(client, "ERROR {\"msg\":\"Incorrect MambaNetAddr\"}");
  addr = hex2int(str, 8);

  if(!db_getnode(&node, addr))
    return conn_send(client, "ERROR {\"msg\":\"Address not found in the database\"}");
  if(node.Active)
    return conn_send(client, "ERROR {\"msg\":\"Node is currently online\"}");
  db_rmnode(addr);
  writelog("Removing reservation for address %08lX", addr);
  conn_send(client, "OK {}");
}


void conn_receive(int client, char *line) {
  struct json_object *arg;
  char cmd[10];
  int i;

  for(i=0; i<10 && line[i] && line[i] != ' '; i++)
    cmd[i] = line[i];
  if(i == 10 || line[i] != ' ' || !line[i+1])
    return conn_send(client, "ERROR {\"msg\":\"Couldn't parse command\"}");
  cmd[i] = 0;
  arg = json_tokener_parse(&(line[i+1]));
  if(is_error(arg))
    return conn_send(client, "ERROR {\"msg\":\"Couldn't parse argument\"}");

  pthread_mutex_lock(&lock);
  if(strcmp(cmd, "GET") == 0)
    conn_cmd_get(client, arg);
  else if(strcmp(cmd, "SETNAME") == 0)
    conn_cmd_setname(client, arg);
  else if(strcmp(cmd, "SETENGINE") == 0)
    conn_cmd_setengine(client, arg);
  else if(strcmp(cmd, "PING") == 0) {
    mbnSendPingRequest(mbn, MBN_BROADCAST_ADDRESS);
    conn_send(client, "OK {}");
  } else if(strcmp(cmd, "REFRESH") == 0)
    conn_cmd_refresh(client, arg);
  else if(strcmp(cmd, "REMOVE") == 0)
    conn_cmd_remove(client, arg);
  else
    conn_send(client, "ERROR {\"msg\":\"Unknown command\"}");
  pthread_mutex_unlock(&lock);

  json_object_put(arg);
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

