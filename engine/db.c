
#include "axum_engine.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <pthread.h>
//#include <mbn.h>

PGconn *sqldb;
pthread_mutex_t dbmutex = PTHREAD_MUTEX_INITIALIZER;


int db_init(char *conninfo, char *err) {
  sqldb = PQconnectdb(conninfo);
  if(PQstatus(sqldb) != CONNECTION_OK) {
    sprintf(err, "Opening database: %s\n", PQerrorMessage(sqldb));
    PQfinish(sqldb);
    return 0;
  }

  return 1;
}


void db_free() {
  PQfinish(sqldb);
}

int db_load_engine_functions()
{
  PGresult *qs;
  char q[1024];

  /* TODO: put query in query in q[] */

  qs = PQexec(sqldb, q); if(qs == NULL || PQresultStatus(qs) != PGRES_COMMAND_OK) {
    //writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qs));
    PQclear(qs);
    return 0;
  }

  /* TODO: parse query result */

  PQclear(qs);
  return 1;
}

void db_lock(int l) {
  if(l)
    pthread_mutex_lock(&dbmutex);
  else
    pthread_mutex_unlock(&dbmutex);
}

