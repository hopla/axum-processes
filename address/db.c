
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

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
      HardwareParent     BLOB(6) NOT NULL DEFAULT X'000000000000'\
    )", NULL, NULL, &dberr
  ) != SQLITE_OK) {
    sprintf(err, "Creating table: %s\n", dberr);
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
  if(n != SQLITE_ROW)
    return 0;
  if(res == NULL)
    return 1;

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
  res->Parent[0] = res->Parent[1] = res->Parent[2] = 0;
  if((str = (unsigned char *)sqlite3_column_blob(st, 7)) != NULL) {
    res->Parent[0] = (unsigned short)(str[0]<<8) + str[1];
    res->Parent[1] = (unsigned short)(str[2]<<8) + str[3];
    res->Parent[2] = (unsigned short)(str[4]<<8) + str[5];
  }
  return 1;
}


int db_getnode(struct db_node *res, unsigned long addr) {
  sqlite3_stmt *stmt;
  char q[100];
  int n;

  sprintf(q, "SELECT * FROM nodes WHERE MambaNetAddress = %ld", addr);
  sqlite3_prepare_v2(sqldb, q, -1, &stmt, NULL);
  n = db_parserow(stmt, res);
  sqlite3_finalize(stmt);
  return n;
}


int db_setnode(unsigned long addr, struct db_node *node) {
  char *q, *qf, *err;

  if(db_getnode(NULL, addr))
    qf = "UPDATE nodes\
      SET\
        MambaNetAddress = %ld,\
        Name = %Q,\
        ManufacturerID = %d,\
        ProductID = %d,\
        UniqueIDPerProduct = %d,\
        EngineAddr = %ld,\
        Services = %d,\
        HardwareParent = X'%04X%04X%04X'\
      WHERE MambaNetAddress = %d";
  else
    qf = "INSERT INTO nodes\
      VALUES(%ld, %Q, %d, %d, %d, %ld, %d, X'%04X%04X%04X')";

  q = sqlite3_mprintf(qf,
    node->MambaNetAddr, node->Name[0] == 0 ? NULL : node->Name,
    node->ManufacturerID, node->ProductID, node->UniqueIDPerProduct,
    node->EngineAddr, node->Services,
    node->Parent[0], node->Parent[1], node->Parent[2], addr
  );
  if(sqlite3_exec(sqldb, q, NULL, NULL, &err) != SQLITE_OK) {
    sqlite3_free(err);
    sqlite3_free(q);
    return 0;
  }
  sqlite3_free(q);
  return 1;
}


