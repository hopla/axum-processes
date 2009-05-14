
#include "main.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <mbn.h>

PGconn *sqldb;
pthread_mutex_t dbmutex = PTHREAD_MUTEX_INITIALIZER;


int db_init(char *conninfo, char *err) {
  PGresult *res;

  sqldb = PQconnectdb(conninfo);
  if(PQstatus(sqldb) != CONNECTION_OK) {
    sprintf(err, "Opening database: %s\n", PQerrorMessage(sqldb));
    PQfinish(sqldb);
    return 0;
  }

  res = PQexec(sqldb, "UPDATE addresses SET active = false");
  if(PQresultStatus(res) != PGRES_COMMAND_OK) {
    sprintf(err, "Clearing active bit: %s\n", PQerrorMessage(sqldb));
    PQclear(res);
    PQfinish(sqldb);
    return 1;
  }
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
    writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
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
  qs = PQexecParams(sqldb, "SELECT * FROM addresses WHERE (id).man = $1 AND (id).prod = $2 AND (id).id = $3",
      3, NULL, params, NULL, NULL, 0);
  if(qs == NULL || PQresultStatus(qs) != PGRES_TUPLES_OK) {
    writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
  n = !PQntuples(qs) ? 0 : db_parserow(qs, 0, res);
  PQclear(qs);
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
  sprintf(str[4],  "%d", node->Services);
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
    writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
  PQclear(qs);
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
    writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
  PQclear(qs);
}


unsigned long db_newaddress() {
  unsigned long addr;
  /* this isn't the most efficient method... */
  for(addr=1; db_getnode(NULL, addr); addr++)
    ;
  return addr;
}


void db_lock(int l) {
  if(l)
    pthread_mutex_lock(&dbmutex);
  else
    pthread_mutex_unlock(&dbmutex);
}

