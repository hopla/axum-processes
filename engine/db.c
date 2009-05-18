
#include "axum_engine.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <pthread.h>

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

int db_insert_engine_functions(const char *table, int function_number, const char *name, int rcv_type, int xmt_type) {
  char q[1024];
  int type = -1;
  PGresult *qres;

  if (strcmp(table, "module_functions") == 0)  
  {
    type = MODULE_FUNCTIONS;
  }
  else if (strcmp(table, "buss_functions") == 0)
  {
    type = BUSS_FUNCTIONS;
  }
  else if (strcmp(table, "monitor_buss_functions") == 0)
  {
    type = MONITOR_BUSS_FUNCTIONS;
  }
  else if (strcmp(table, "global_functions") == 0)
  {
    type = GLOBAL_FUNCTIONS;
  }
  else if (strcmp(table, "source_functions") == 0)
  {
    type = SOURCE_FUNCTIONS;
  }
  else if (strcmp(table, "destination_functions") == 0)
  {
    type = DESTINATION_FUNCTIONS;
  }
  
  if (type == -1)
  {
    //writelog("db_insert_engine_functions error on %s:%d: %s", __FILE__, __LINE__, "table_name unknown");
    return 0;  
  }

  sprintf(q, "INSERT INTO functions VALUES (func.type=%d, func.seq=NULL, func.func=%d, name='%s' , rcv_type=%d, xmt_type=%d)", type, function_number, name, rcv_type, xmt_type);
  qres = PQexec(sqldb, q);
  if(qres == NULL || PQresultStatus(qres) != PGRES_COMMAND_OK) {
    //writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qres));
    PQclear(qres);
    return 0;
  }

  PQclear(qres);
  return 1;    
}

/*
int db_load_engine_functions() {
  PGresult *qres;
  char q[1024];
  int row_count, cnt_row;

  sprintf(q,"SELECT func, name, rcv_type, xmt_type FROM functions");
  qres = PQexec(sqldb, q); if(qres == NULL || PQresultStatus(qres) != PGRES_COMMAND_OK) {
    //writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qres));
  
    PQclear(qres);
    return 0;
  }

  //parse result of the query
  row_count = PQntuples(qres);   
  if (row_count && (PQnfields(qres)==4)) {
    for (cnt_row=0; cnt_row<row_countl cnt_row++) {
      if (sscanf(PQgetvalue(qres, cnt_row, 0, "(%d,%d,%d)", type, sequence_number, function_number)) != 3) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
      if (sscanf(PQgetvalue(qres, cnt_row, 1, "%s", function_name)) != 1) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
      if (sscanf(PQgetvalue(qres, cnt_row, 2, "%hd", function_rcv_type)) != 1) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
      if (sscanf(PQgetvalue(qres, cnt_row, 3, "%hd", function_xmt_type)) != 1) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
    }
  }

  PQclear(qs);
  return row_count;
}
*/

void db_lock(int l) {
  if(l)
    pthread_mutex_lock(&dbmutex);
  else
    pthread_mutex_unlock(&dbmutex);
}

