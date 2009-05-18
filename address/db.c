
#include "common.h"
#include "main.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/select.h>

#include <libpq-fe.h>
#include <pthread.h>
#include <mbn.h>


char lastnotify[50];
PGconn *sqldb;
pthread_mutex_t dbmutex = PTHREAD_MUTEX_INITIALIZER;


void db_processnotifies();


int db_init(char *conninfo, char *err) {
  PGresult *res;

  sqldb = PQconnectdb(conninfo);
  if(PQstatus(sqldb) != CONNECTION_OK) {
    sprintf(err, "Opening database: %s\n", PQerrorMessage(sqldb));
    PQfinish(sqldb);
    return 1;
  }

  res = PQexec(sqldb, "UPDATE addresses SET active = false");
  if(res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
    sprintf(err, "Clearing active bit: %s\n", PQerrorMessage(sqldb));
    PQclear(res);
    PQfinish(sqldb);
    return 1;
  }
  PQclear(res);

  res = PQexec(sqldb, "LISTEN change");
  if(res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
    sprintf(err, "LISTENing on change: %s\n", PQerrorMessage(sqldb));
    PQclear(res);
    PQfinish(sqldb);
    return 1;
  }
  PQclear(res);

  /* get timestamp of the most recent change or the current time if the table is empty.
   * This ensures that we have a timestamp in a format suitable for comparing to other
   * timestamps, and in the same time and timezone as the PostgreSQL server. */
  res = PQexec(sqldb, "SELECT MAX(timestamp) FROM recent_changes UNION SELECT NOW() LIMIT 1");
  if(res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) {
    sprintf(err, "Getting last timestamp: %s\n", PQerrorMessage(sqldb));
    PQclear(res);
    PQfinish(sqldb);
    return 1;
  }
  strcpy(lastnotify, PQgetvalue(res, 0, 0));
  PQclear(res);
  return 0;
}


void db_free() {
  PQfinish(sqldb);
}


/* assumes a SELECT * FROM addresses as columns */
int db_parserow(PGresult *q, int row, struct db_node *res) {
  if(res == NULL)
    return 1;

  memset((void *)res, 0, sizeof(struct db_node));
  sscanf(PQgetvalue(q, row, 0), "%ld", &(res->MambaNetAddr));
  if(PQgetisnull(q, row, 1))
    res->Name[0] = 0;
  else
    strcpy(res->Name, PQgetvalue(q, row, 1));
  sscanf(PQgetvalue(q, row, 2),  "(%hd,%hd,%hd)", &(res->ManufacturerID), &(res->ProductID), &(res->UniqueIDPerProduct));
  sscanf(PQgetvalue(q, row, 3),  "%ld",  &(res->EngineAddr));
  sscanf(PQgetvalue(q, row, 4),  "%hhd", &(res->Services));
  res->Active = strcmp(PQgetvalue(q, row, 5), "f") == 0 ? 0 : 1;
  sscanf(PQgetvalue(q, row, 6),  "(%hd,%hd,%hd)", res->Parent, res->Parent+1, res->Parent+2);
  if(strcmp(PQgetvalue(q, row, 7), "f") != 0)
    res->flags |= DB_FLAGS_SETNAME;
  if(strcmp(PQgetvalue(q, row, 8), "f") != 0)
    res->flags |= DB_FLAGS_REFRESH;
  sscanf(PQgetvalue(q, row, 9),  "%ld", &(res->FirstSeen));
  sscanf(PQgetvalue(q, row, 10), "%ld", &(res->LastSeen));
  sscanf(PQgetvalue(q, row, 11), "%d",  &(res->AddressRequests));
  return 1;
}


int db_getnode(struct db_node *res, unsigned long addr) {
  PGresult *qs;
  char str[20];
  const char *params[1];
  int n;

  sprintf(str, "%ld", addr);
  params[0] = str;
  qs = PQexecParams(sqldb, "SELECT * FROM addresses WHERE addr = $1",
      1, NULL, params, NULL, NULL, 0);
  if(qs == NULL || PQresultStatus(qs) != PGRES_TUPLES_OK) {
    log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
  n = !PQntuples(qs) ? 0 : db_parserow(qs, 0, res);
  PQclear(qs);
  db_processnotifies();
  return n;
}


int db_nodebyid(struct db_node *res, unsigned short id_man, unsigned short id_prod, unsigned short id_id) {
  PGresult *qs;
  char str[3][20];
  const char *params[3] = {(const char *)&(str[0]), (const char *)&(str[1]), (const char *)&(str[2])};
  int n;

  sprintf(str[0], "%hd", id_man);
  sprintf(str[1], "%hd", id_prod);
  sprintf(str[2], "%hd", id_id);
  qs = PQexecParams(sqldb, "SELECT * FROM addresses WHERE (id).man = $1 AND (id).prod = $2 AND (id).id = $3",
      3, NULL, params, NULL, NULL, 0);
  if(qs == NULL || PQresultStatus(qs) != PGRES_TUPLES_OK) {
    log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
  n = !PQntuples(qs) ? 0 : db_parserow(qs, 0, res);
  PQclear(qs);
  db_processnotifies();
  return n;
}


int db_setnode(unsigned long addr, struct db_node *node) {
  PGresult *qs;
  char str[13][33];
  const char *params[13];
  int i;

  /* create arguments array */
  for(i=0;i<13;i++)
    params[i] = (const char *)str[i];

  sprintf(str[0], "%ld", node->MambaNetAddr);
  if(node->Name[0] == 0)
    params[1] = NULL;
  else
    strcpy(str[1], node->Name);
  sprintf(str[2],  "(%hd,%hd,%hd)", node->ManufacturerID, node->ProductID, node->UniqueIDPerProduct);
  sprintf(str[3],  "%ld", node->EngineAddr);
  sprintf(str[4],  "%d", node->Services & ~MBN_ADDR_SERVICES_VALID);
  sprintf(str[5],  "%c", node->Active ? 't' : 'f');
  sprintf(str[6],  "(%hd,%hd,%hd)", node->Parent[0], node->Parent[1], node->Parent[2]);
  sprintf(str[7],  "%c", node->flags & DB_FLAGS_SETNAME ? 't' : 'f');
  sprintf(str[8],  "%c", node->flags & DB_FLAGS_REFRESH ? 't' : 'f');
  sprintf(str[9],  "%ld", node->FirstSeen);
  sprintf(str[10], "%ld", node->LastSeen);
  sprintf(str[11], "%d", node->AddressRequests);
  sprintf(str[12], "%ld", node->MambaNetAddr);

  /* execute query */
  qs = PQexecParams(sqldb,
    addr ? "UPDATE addresses SET addr = $1, name = $2, id = $3, engine_addr = $4, services = $5,\
              active = $6, parent = $7, setname = $8, refresh = $9, firstseen = $10, lastseen = $11,\
              addr_requests = $12 WHERE addr = $13"
         : "INSERT INTO addresses VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)",
    addr ? 13 : 12, NULL, params, NULL, NULL, 0);
  if(qs == NULL || PQresultStatus(qs) != PGRES_COMMAND_OK) {
    log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
  PQclear(qs);
  db_processnotifies();
  return 1;
}


void db_rmnode(unsigned long addr) {
  PGresult *qs;
  char str[20];
  const char *params[1];

  sprintf(str, "%ld", addr);
  params[0] = str;
  qs = PQexecParams(sqldb, "DELETE FROM addresses WHERE addr = $1", 1, NULL, params, NULL, NULL, 0);
  if(qs == NULL || PQresultStatus(qs) != PGRES_COMMAND_OK)
    log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
  PQclear(qs);
  db_processnotifies();
}


unsigned long db_newaddress() {
  unsigned long addr;
  /* this isn't the most efficient method... */
  for(addr=1; db_getnode(NULL, addr); addr++)
    ;
  return addr;
}


int db_loop() {
  int s = PQsocket(sqldb);
  int n;
  fd_set rd;
  if(s < 0) {
    log_write("Invalid PostgreSQL socket!");
    return 1;
  }

  FD_ZERO(&rd);
  FD_SET(s, &rd);
  n = select(s+1, &rd, NULL, NULL, NULL);
  if(n == 0 || (n < 0 && errno == EINTR))
    return 0;
  if(n < 0) {
    log_write("select() failed: %s\n", strerror(errno));
    return 1;
  }
  db_lock(1);
  PQconsumeInput(sqldb);
  db_processnotifies();
  db_lock(0);
  return 0;
}


void db_event_removed(char myself, char *arg) {
  struct mbn_address_node *n;
  int addr;

  sscanf(arg, "%d", &addr);

  /* if the address is currently online, reset its valid bit,
   * otherwise simply ignore the removal */
  if((n = mbnNodeStatus(mbn, addr)) != NULL)
    set_address_struct(0, *n);
  log_write("Address reserveration for %08X has been removed", addr);
  return;
  myself++;
}


void db_event_setengine(char myself, char *arg) {
  struct db_node node;
  union mbn_data dat;
  int addr;

  sscanf(arg, "%d", &addr);
  if(myself || !db_getnode(&node, addr))
    return;

  dat.UInt = node.EngineAddr;
  if(node.Active)
    mbnSetActuatorData(mbn, addr, MBN_NODEOBJ_ENGINEADDRESS, MBN_DATATYPE_UINT, 4, dat, 1);
  log_write("Setting engine address of %08X to %08X", addr, node.EngineAddr);
}


void db_event_setaddress(char myself, char *arg) {
  struct db_node node;
  int old, new;

  if(myself || sscanf(arg, "%d %d", &old, &new) != 2 || !db_getnode(&node, new))
    return;

  set_address_struct(new, node);
  log_write("Changing address of %08X to %08X", old, new);
}


void db_event_setname(char myself, char *arg) {
  struct db_node node;
  union mbn_data dat;
  int addr;

  sscanf(arg, "%d", &addr);
  /* if the node is online, set its name and reset the setname flag */
  if(myself || mbnNodeStatus(mbn, addr) != NULL) {
    db_getnode(&node, addr);
    node.flags &= ~DB_FLAGS_SETNAME;
    db_setnode(addr, &node);
    dat.Octets = (unsigned char *)node.Name;
    mbnSetActuatorData(mbn, addr, MBN_NODEOBJ_NAME, MBN_DATATYPE_OCTETS, 32, dat, 1);
  }
}


void db_event_refresh(char myself, char *arg) {
  PGresult *qs;
  int addr;
  char str[20];
  const char *params[1] = { (const char *)str };

  sscanf(arg, "%d", &addr);
  /* if the node is online, fetch the name & parent and reset the refresh flag */
  if(myself || mbnNodeStatus(mbn, addr) != NULL) {
    mbnGetActuatorData(mbn, addr, MBN_NODEOBJ_NAME, 1);
    mbnGetSensorData(mbn, addr, MBN_NODEOBJ_HWPARENT, 1);
    sprintf(str, "%d", addr);
    qs = PQexecParams(sqldb, "UPDATE addresses SET refresh = FALSE WHERE addr = $1", 1, NULL, params, NULL, NULL, 0);
    if(qs == NULL || PQresultStatus(qs) != PGRES_COMMAND_OK)
      log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
  }
}


void db_processnotifies() {
  PGresult *qs;
  PGnotify *not;
  const char *params[1] = { (const char *)lastnotify };
  char *cmd, *arg, myself;
  int i, pid;

  /* we don't actually check the struct returned by PQnotifies() */
  if((not = PQnotifies(sqldb)) == NULL)
    return;
  /* clear any other received notifications */
  do
    PQfreemem(not);
  while((not = PQnotifies(sqldb)) != NULL);

  /* check the recent_changes table */
  qs = PQexecParams(sqldb, "SELECT change, arguments, timestamp, pid\
      FROM recent_changes WHERE timestamp > $1 ORDER BY timestamp",
      1, NULL, params, NULL, NULL, 0);
  if(qs == NULL || PQresultStatus(qs) != PGRES_TUPLES_OK) {
    log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return;
  }
  for(i=0; i<PQntuples(qs); i++) {
    cmd = PQgetvalue(qs, i, 0);
    arg = PQgetvalue(qs, i, 1);
    sscanf(PQgetvalue(qs, i, 3), "%d", &pid);
    myself = pid == PQbackendPID(sqldb) ? 1 : 0;

    if(strcmp(cmd, "address_removed") == 0)
      db_event_removed(myself, arg);
    if(strcmp(cmd, "address_set_engine") == 0)
      db_event_setengine(myself, arg);
    if(strcmp(cmd, "address_set_addr") == 0)
      db_event_setaddress(myself, arg);
    if(strcmp(cmd, "address_set_name") == 0)
      db_event_setname(myself, arg);
    if(strcmp(cmd, "address_refresh") == 0)
      db_event_refresh(myself, arg);
  }
  /* update lastnotify variable */
  strcpy(lastnotify, PQgetvalue(qs, i-1, 2));
  PQclear(qs);
}


void db_lock(int l) {
  PGresult *qs;
  if(l) {
    pthread_mutex_lock(&dbmutex);
    qs = PQexec(sqldb, "BEGIN");
  } else {
    qs = PQexec(sqldb, "COMMIT");
    pthread_mutex_unlock(&dbmutex);
  }
  if(qs == NULL || PQresultStatus(qs) != PGRES_COMMAND_OK)
    log_write("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
  PQclear(qs);
}

