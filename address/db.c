
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
    CREATE TABLE IF NOT EXISTS address_table (\
      Name VARCHAR(32),\
      ManufacturerID INT,\
      ProductID INT,\
      UniqueIDPerProduct INT,\
      MambaNetAddress INT,\
      DefaultEngineMambaNetAddress INT,\
      NodeServices INT,\
      Active INT,\
      HardwareParent VARCHAR(14) DEFAULT '0000:0000:0000'\
    );", NULL, NULL, &dberr
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


int hex2int(const char *hex, int len) {
  int i, r = 0;
  for(i=0; i<len; i++) {
    r <<= 4;
    if(hex[i] >= '0' && hex[i] <= '9')
      r += hex[i]-'0';
    else if(hex[i] >= 'a' && hex[i] <= 'f')
      r += hex[i]-'a'+10;
    else if(hex[i] >= 'A' && hex[i] <= 'F')
      r = hex[i]-'A'+10;
  }
  return r;
}


int db_parserow(sqlite3_stmt *st, struct db_node *res) {
  int n;
  const unsigned char *str;

  n = sqlite3_step(st);
  if(n != SQLITE_ROW)
    return 0;

  if((str = sqlite3_column_text(st, 0)) == NULL || strlen((char *)str) > 32)
    res->Name[0] = 0;
  else
    strcpy(res->Name, (char *)str);
  res->ManufacturerID     =         (short)sqlite3_column_int(st, 1);
  res->ProductID          =         (short)sqlite3_column_int(st, 2);
  res->UniqueIDPerProduct =         (short)sqlite3_column_int(st, 3);
  res->MambaNetAddr       = (unsigned long)sqlite3_column_int(st, 4);
  res->EngineAddr         = (unsigned long)sqlite3_column_int(st, 5);
  res->Services           =          (char)sqlite3_column_int(st, 6);
  res->Active             =          (char)sqlite3_column_int(st, 7);
  if((str = sqlite3_column_text(st, 8)) == NULL || strlen((char *)str) != 14)
    memset((void *)res->Parent, 0, 14);
  else {
    res->Parent[0] = hex2int((char *)(&(str[ 0])), 2);
    res->Parent[1] = hex2int((char *)(&(str[ 2])), 2);
    res->Parent[2] = hex2int((char *)(&(str[ 5])), 2);
    res->Parent[3] = hex2int((char *)(&(str[ 7])), 2);
    res->Parent[4] = hex2int((char *)(&(str[10])), 2);
    res->Parent[5] = hex2int((char *)(&(str[12])), 2);
  }
  return 1;
}


int db_getnode(struct db_node *res, unsigned long addr) {
  sqlite3_stmt *stmt;
  char q[100];
  int n;

  sprintf(q, "SELECT * FROM address_table WHERE MambaNetAddress = %ld", addr);
  sqlite3_prepare_v2(sqldb, q, -1, &stmt, NULL);
  n = db_parserow(stmt, res);
  sqlite3_finalize(stmt);
  return n;
}

