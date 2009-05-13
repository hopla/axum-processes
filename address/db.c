
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
  if(!PQntuples(qs))
    n = 0;
  else
    n = db_parserow(qs, 0, res);
  PQclear(qs);
  return n;
}


int db_searchnodes(struct db_node *match, int matchfields, int limit, int offset, int order, struct db_node **res) {
  PGresult *qs;
  char q[500], str[129];
  int i;

  /* TODO: clean method using PQexecParams() */
  strcpy(q, "SELECT * FROM addresses WHERE 't'");
  if(matchfields & DB_NAME) {
    PQescapeStringConn(sqldb, str, match->Name, strlen(match->Name), NULL);
    sprintf(&(q[strlen(q)]), " AND name = %s", str);
  }
  if(matchfields & DB_MAMBANETADDR)
    sprintf(&(q[strlen(q)]), " AND addr = %ld", match->MambaNetAddr);
  if(matchfields & DB_MANUFACTURERID)
    sprintf(&(q[strlen(q)]), " AND (id).man = %hd", match->ManufacturerID);
  if(matchfields & DB_PRODUCTID)
    sprintf(&(q[strlen(q)]), " AND (id).prod = %hd", match->ProductID);
  if(matchfields & DB_UNIQUEID)
    sprintf(&(q[strlen(q)]), " AND (id).id = %hd", match->UniqueIDPerProduct);
  if(matchfields & DB_ENGINEADDR)
    sprintf(&(q[strlen(q)]), " AND engine_addr = %ld", match->EngineAddr);
  if(matchfields & DB_SERVICES)
    sprintf(&(q[strlen(q)]), " AND services = %d", match->Services);
  if(matchfields & DB_ACTIVE)
    sprintf(&(q[strlen(q)]), " AND active = '%c'", match->Active ? 't' : 'f');
  if(matchfields & DB_PARENT)
    sprintf(&(q[strlen(q)]), " AND (parent).man = %hd AND (parent).prod = %hd AND (parent).id = %hd",
       match->Parent[0], match->Parent[1], match->Parent[2]);

  if(!order || order == DB_DESC)
    order |= DB_MAMBANETADDR;
  if(!limit)
    limit = 99999;
  sprintf(&(q[strlen(q)]), " ORDER BY %s%s LIMIT %d OFFSET %d",
    order & DB_NAME       ? "name"     : order & DB_MAMBANETADDR ? "addr" :
      order & DB_SERVICES ? "services" : order & DB_ACTIVE       ? "active"          :
      order & DB_MANUFACTURERID || order & DB_PRODUCTID || order & DB_UNIQUEID ?
        "((id).man<<32)+((id).prod<<16)+(id).id" : "((parent).man<<32)+((parent).prod<<16)+(parent).id",
    order & DB_DESC ? " DESC" : " ASC", limit, offset
  );
  qs = PQexec(sqldb, q);
  if(qs == NULL || PQresultStatus(qs) != PGRES_TUPLES_OK) {
    writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }
  if(!PQntuples(qs)) {
    PQclear(qs);
    return 0;
  }

  *res = malloc(PQntuples(qs)*sizeof(struct db_node));
  for(i=0; i<PQntuples(qs); i++)
    db_parserow(qs, i, &((*res)[i]));
  PQclear(qs);
  return i;
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

