
#include "common.h"
#include "main.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/select.h>

#include <libpq-fe.h>
#include <mbn.h>


#define ADDRSELECT "addr, name, id, engine_addr, services, active, parent, setname,\
  refresh, DATE_PART('epoch', firstseen), DATE_PART('epoch', lastseen), addr_requests, firm_major"


void db_event_removed(char, char *);
void db_event_setengine(char, char *);
void db_event_setname(char, char *);
void db_event_setaddress(char, char *);
void db_event_refresh(char, char *);

struct sql_notify notifies[] = {
  { "address_removed",    db_event_removed },
  { "address_set_engine", db_event_setengine },
  { "address_set_name",   db_event_setname },
  { "address_set_address", db_event_setaddress },
  { "address_refresh",    db_event_refresh }
};


void db_init(char *conninfo) {
  PGresult *res;

  sql_open(conninfo, 5, notifies);

  /* reset active columns */
  if((res = sql_exec("UPDATE addresses SET active = false", 0, 0, NULL)) == NULL)
    exit(1);
  PQclear(res);
}


/* assumes a SELECT ${ADDRSELECT} FROM addresses as columns */
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
  if(sscanf(PQgetvalue(q, row, 12), "%hd", &(res->FirmMajor)) != 1)
    res->FirmMajor = -1;
  return 1;
}


int db_getnode(struct db_node *res, unsigned long addr) {
  PGresult *qs;
  char str[20];
  const char *params[1];
  int n;

  sprintf(str, "%ld", addr);
  params[0] = str;
  if((qs = sql_exec("SELECT " ADDRSELECT " FROM addresses WHERE addr = $1", 1, 1, params)) == NULL)
    return 0;
  n = !PQntuples(qs) ? 0 : db_parserow(qs, 0, res);
  PQclear(qs);
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
  if((qs = sql_exec("SELECT " ADDRSELECT " FROM addresses\
      WHERE (id).man = $1 AND (id).prod = $2 AND (id).id = $3", 1, 3, params)) == NULL)
    return 0;
  n = !PQntuples(qs) ? 0 : db_parserow(qs, 0, res);
  PQclear(qs);
  return n;
}


int db_setnode(unsigned long addr, struct db_node *node) {
  PGresult *qs;
  char str[14][33];
  const char *params[14];
  int i;

  /* create arguments array */
  for(i=0;i<14;i++)
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
  if(node->FirmMajor == -1)
    params[12] = NULL;
  else
    sprintf(str[12], "%hd", node->FirmMajor);
  sprintf(str[13], "%ld", node->MambaNetAddr);

  /* execute query */
  qs = sql_exec(
    addr ? "UPDATE addresses SET addr = $1, name = $2, id = $3, engine_addr = $4, services = $5,\
              active = $6, parent = $7, setname = $8, refresh = $9, firstseen = 'epoch'::timestamptz\
              + $10 * '1 second'::interval, lastseen = 'epoch'::timestamptz + $11 * '1 second'::interval,\
              addr_requests = $12, firm_major = $13 WHERE addr = $14"
         : "INSERT INTO addresses (addr, name, id, engine_addr, services, active, parent, setname, refresh,\
              firstseen, lastseen, addr_requests, firm_major) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9,\
              'epoch'::timestamptz+$10*'1 second'::interval, 'epoch'::timestamptz+$11*'1 second'::interval, $12, $13)",
    0, addr ? 14 : 13, params);
  if(qs == 0)
    return 0;
  PQclear(qs);
  return 1;
}


void db_rmnode(unsigned long addr) {
  PGresult *qs;
  char str[20];
  const char *params[1];

  sprintf(str, "%ld", addr);
  params[0] = str;
  if((qs = sql_exec("DELETE FROM addresses WHERE addr = $1", 0, 1, params)) == NULL)
    return;
  PQclear(qs);
}


unsigned long db_newaddress() {
  unsigned long addr;
  /* this isn't the most efficient method... */
  for(addr=1; db_getnode(NULL, addr); addr++)
    ;
  return addr;
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
  if(!myself && mbnNodeStatus(mbn, addr) != NULL) {
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
    mbnGetSensorData(mbn, addr, MBN_NODEOBJ_FWMAJOR, 1);
    sprintf(str, "%d", addr);
    if((qs = sql_exec("UPDATE addresses SET refresh = FALSE\
        WHERE addr = $1", 0, 1, params)) != NULL)
      PQclear(qs);
  }
}

