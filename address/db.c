
#include "main.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <mbn.h>

sqlite3 *sqldb;


int db_init(char *dbpath, char *err) {
  char *dberr;

  if(sqlite3_open(dbpath, &sqldb) != SQLITE_OK) {
    sprintf(err, "Opening database: %s\n", sqlite3_errmsg(sqldb));
    sqlite3_close(sqldb);
    return 0;
  }
  if(sqlite3_exec(sqldb, "\
    CREATE TABLE IF NOT EXISTS nodes (\
      MambaNetAddress    INT NOT NULL PRIMARY KEY,\
      Name               VARCHAR(32),\
      ManufacturerID     INT NOT NULL,\
      ProductID          INT NOT NULL,\
      UniqueIDPerProduct INT NOT NULL,\
      EngineAddr         INT NOT NULL,\
      Services           INT NOT NULL,\
      Active             INT NOT NULL,\
      HardwareParent     BLOB(6) NOT NULL DEFAULT X'000000000000',\
      flags              INT NOT NULL,\
      FirstSeen          INT NOT NULL,\
      LastSeen           INT NOT NULL\
    )", NULL, NULL, &dberr
  ) != SQLITE_OK) {
    sprintf(err, "Creating table: %s\n", dberr);
    sqlite3_free(dberr);
    sqlite3_close(sqldb);
    return 1;
  }
  if(sqlite3_exec(sqldb, "UPDATE nodes SET Active = 0", NULL, NULL, &dberr) != SQLITE_OK) {
    sprintf(err, "Clearing active bit: %s\n", dberr);
    sqlite3_free(dberr);
    sqlite3_close(sqldb);
    return 1;
  }
  return 0;
}


void db_free() {
  sqlite3_close(sqldb);
}


int db_parserow(sqlite3_stmt *st, struct db_node *res) {
  int n;
  const unsigned char *str;

  n = sqlite3_step(st);
  if(n == SQLITE_DONE)
    return 0;
  if(n != SQLITE_ROW) {
    writelog("SQL Error: %s", sqlite3_errmsg(sqldb));
    return 0;
  }
  if(res == NULL)
    return 1;

  memset((void *)res, 0, sizeof(struct db_node));
  res->MambaNetAddr       =  (unsigned long)sqlite3_column_int(st, 0);
  if((str = sqlite3_column_text(st, 1)) == NULL || strlen((char *)str) > 32)
    res->Name[0] = 0;
  else
    strcpy(res->Name, (char *)str);
  res->ManufacturerID     = (unsigned short)sqlite3_column_int(st, 2);
  res->ProductID          = (unsigned short)sqlite3_column_int(st, 3);
  res->UniqueIDPerProduct = (unsigned short)sqlite3_column_int(st, 4);
  res->EngineAddr         =  (unsigned long)sqlite3_column_int(st, 5);
  res->Services           =  (unsigned char)sqlite3_column_int(st, 6);
  res->Active             =  sqlite3_column_int(st, 7) ? 1 : 0;
  res->Parent[0] = res->Parent[1] = res->Parent[2] = 0;
  if((str = (unsigned char *)sqlite3_column_blob(st, 8)) != NULL) {
    res->Parent[0] = (unsigned short)(str[0]<<8) + str[1];
    res->Parent[1] = (unsigned short)(str[2]<<8) + str[3];
    res->Parent[2] = (unsigned short)(str[4]<<8) + str[5];
  }
  res->flags              =  (unsigned char)sqlite3_column_int(st, 9);
  res->FirstSeen          =         (time_t)sqlite3_column_int64(st, 10);
  res->LastSeen           =         (time_t)sqlite3_column_int64(st, 11);
  return 1;
}


int db_getnode(struct db_node *res, unsigned long addr) {
  sqlite3_stmt *stmt;
  char q[100];
  int n;

  sprintf(q, "SELECT * FROM nodes WHERE MambaNetAddress = %ld", addr);
  if(sqlite3_prepare_v2(sqldb, q, -1, &stmt, NULL) != SQLITE_OK) {
    writelog("SQL Error for \"%s\": %s", q, sqlite3_errmsg(sqldb));
    return 0;
  }
  n = db_parserow(stmt, res);
  sqlite3_finalize(stmt);
  return n;
}


int db_searchnodes(struct db_node *match, int matchfields, int limit, int offset, int order, struct db_node **res) {
  sqlite3_stmt *stmt;
  char q[500];
  int i, s;

  strcpy(q, "SELECT * FROM nodes WHERE 1");
  if(matchfields & DB_NAME)
    sqlite3_snprintf(50, &(q[strlen(q)]), " AND Name = %Q", match->Name);
  if(matchfields & DB_MAMBANETADDR)
    sqlite3_snprintf(32, &(q[strlen(q)]), " AND MambaNetAddress = %ld", match->MambaNetAddr);
  if(matchfields & DB_MANUFACTURERID)
    sqlite3_snprintf(32, &(q[strlen(q)]), " AND ManufacturerID = %d", match->ManufacturerID);
  if(matchfields & DB_PRODUCTID)
    sqlite3_snprintf(32, &(q[strlen(q)]), " AND ProductID = %d", match->ProductID);
  if(matchfields & DB_UNIQUEID)
    sqlite3_snprintf(40, &(q[strlen(q)]), " AND UniqueIDPerProduct = %d", match->UniqueIDPerProduct);
  if(matchfields & DB_ENGINEADDR)
    sqlite3_snprintf(32, &(q[strlen(q)]), " AND EngineAddr = %ld", match->EngineAddr);
  if(matchfields & DB_SERVICES)
    sqlite3_snprintf(32, &(q[strlen(q)]), " AND Services = %d", match->Services);
  if(matchfields & DB_ACTIVE)
    sqlite3_snprintf(32, &(q[strlen(q)]), " AND Active = %d", match->Active);
  if(matchfields & DB_PARENT)
    sqlite3_snprintf(50, &(q[strlen(q)]), " AND HardwareParent = X'%04X%04X%04X'",
       match->Parent[0], match->Parent[1], match->Parent[2]);

  if(!order || order == DB_DESC)
    order |= DB_MAMBANETADDR;
  if(!limit)
    limit = 99999;
  sqlite3_snprintf(100, &(q[strlen(q)]), " ORDER BY %s%s LIMIT %d OFFSET %d",
    order & DB_NAME       ? "Name"     : order & DB_MAMBANETADDR ? "MambaNetAddress" :
      order & DB_SERVICES ? "Services" : order & DB_ACTIVE       ? "Active"          :
      order & DB_MANUFACTURERID || order & DB_PRODUCTID || order & DB_UNIQUEID ?
        "(ManufacturerID<<32)+(ProductID<<16)+UniqueIDPerProduct" : "HardwareParent",
    order & DB_DESC ? " DESC" : " ASC", limit, offset
  );
  if(sqlite3_prepare_v2(sqldb, q, -1, &stmt, NULL) != SQLITE_OK) {
    writelog("SQL Error for \"%s\": %s", q, sqlite3_errmsg(sqldb));
    return 0;
  }

  s = 5*sizeof(struct db_node);
  *res = malloc(s);
  for(i=0; db_parserow(stmt, &((*res)[i])) == 1; i++)
    if((i+2)*sizeof(struct db_node) > (unsigned int)s) {
      s *= 2;
      *res = realloc(*res, s);
    }
  if(!i) {
    free(*res);
    *res = NULL;
  }
  sqlite3_finalize(stmt);
  return i;
}


int db_setnode(unsigned long addr, struct db_node *node) {
  char *q, *qf, *err;

  if(addr)
    qf = "UPDATE nodes\
      SET\
        MambaNetAddress = %ld,\
        Name = %Q,\
        ManufacturerID = %d,\
        ProductID = %d,\
        UniqueIDPerProduct = %d,\
        EngineAddr = %ld,\
        Services = %d,\
        Active = %d,\
        HardwareParent = X'%04X%04X%04X',\
        flags = %d,\
        FirstSeen = %lld,\
        LastSeen = %lld\
      WHERE MambaNetAddress = %d";
  else
    qf = "INSERT INTO nodes\
      VALUES(%ld, %Q, %d, %d, %d, %ld, %d, %d, X'%04X%04X%04X', %d, %lld, %lld)";

  q = sqlite3_mprintf(qf,
    node->MambaNetAddr, node->Name[0] == 0 ? NULL : node->Name,
    node->ManufacturerID, node->ProductID, node->UniqueIDPerProduct,
    node->EngineAddr, node->Services & ~MBN_ADDR_SERVICES_VALID, node->Active ? 1 : 0,
    node->Parent[0], node->Parent[1], node->Parent[2], node->flags,
    (long long)node->FirstSeen, (long long)node->LastSeen, addr
  );
  if(sqlite3_exec(sqldb, q, NULL, NULL, &err) != SQLITE_OK) {
    writelog("SQL Error for \"%s\": %s", q, err);
    sqlite3_free(err);
    sqlite3_free(q);
    return 0;
  }
  sqlite3_free(q);
  return 1;
}


void db_rmnode(unsigned long addr) {
  char *err, *q;
  q = sqlite3_mprintf("DELETE FROM nodes WHERE MambaNetAddress = %ld", addr);
  if(sqlite3_exec(sqldb, q, NULL, NULL, &err) != SQLITE_OK) {
    writelog("SQL Error for \"%s\": %s", q, err);
    sqlite3_free(err);
  }
  sqlite3_free(q);
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
    sqlite3_mutex_enter(sqlite3_db_mutex(sqldb));
  else
    sqlite3_mutex_leave(sqlite3_db_mutex(sqldb));
}

