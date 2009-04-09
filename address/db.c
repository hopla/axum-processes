

#include <stdio.h>
#include <stdlib.h>
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

