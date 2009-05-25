

#ifndef _db_h
#define _db_h

#include <time.h>

#define DB_FLAGS_SETNAME  0x01
#define DB_FLAGS_REFRESH  0x02


struct db_node {
  char Name[32];
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned long MambaNetAddr;
  unsigned long EngineAddr;
  unsigned char Services;
  unsigned char Active;
  unsigned short Parent[3];
  time_t FirstSeen, LastSeen;
  int AddressRequests;
  unsigned char flags; /* for internal use */
};

void db_init(char *);
int  db_getnode(struct db_node *, unsigned long);
int  db_nodebyid(struct db_node *, unsigned short, unsigned short, unsigned short);
int  db_setnode(unsigned long, struct db_node *);
void db_rmnode(unsigned long);
unsigned long db_newaddress();

#define db_free sql_close
#define db_lock sql_lock
#define db_loop sql_loop

#endif
