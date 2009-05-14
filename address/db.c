
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
  char q[700], str[131], *qf;

  /* TODO: clean method using PQexecParams() */
  if(node->Name[0] == 0)
    strcpy(str, "NULL");
  else {
    str[0] = '\'';
    PQescapeStringConn(sqldb, str+1, node->Name, strlen(node->Name), NULL);
    strcat(str, "'");
  }

  if(addr)
    qf = "UPDATE addresses\
      SET\
        addr = %ld,\
        name = %s,\
        id = (%hd,%hd,%hd),\
        engine_addr = %ld,\
        services = %d,\
        active = '%c',\
        parent = (%hd,%hd,%hd),\
        setname = '%c',\
        refresh = '%c',\
        firstseen = %lld,\
        lastseen = %lld,\
        addr_requests = %d\
      WHERE addr = %d";
  else
    qf = "INSERT INTO addresses\
      VALUES(%ld, %s, (%hd,%hd,%hd), %ld, %d, '%c', (%hd,%hd,%hd), '%c', '%c', %lld, %lld, %d)";

  sprintf(q, qf,
    node->MambaNetAddr, str, node->ManufacturerID, node->ProductID, node->UniqueIDPerProduct,
    node->EngineAddr, node->Services & ~MBN_ADDR_SERVICES_VALID, node->Active ? 't' : 'f',
    node->Parent[0], node->Parent[1], node->Parent[2], node->flags & DB_FLAGS_SETNAME ? 't' : 'f',
    node->flags & DB_FLAGS_REFRESH ? 't' : 'f', (long long)node->FirstSeen,
    (long long)node->LastSeen, node->AddressRequests, addr
  );
  qs = PQexec(sqldb, q);
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

