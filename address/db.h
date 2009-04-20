

#ifndef _db_h

#define DB_NAME           0x001
#define DB_MANUFACTURERID 0x002
#define DB_PRODUCTID      0x004
#define DB_UNIQUEID       0x008
#define DB_MAMBANETADDR   0x010
#define DB_ENGINEADDR     0x020
#define DB_SERVICES       0x040
#define DB_ACTIVE         0x080
#define DB_PARENT         0x100
#define DB_DESC           0x200 /* descending order */

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
  unsigned char flags; /* for internal use */
};

int  db_init(char *, char *);
void db_free();
int  db_getnode(struct db_node *, unsigned long);
int  db_searchnodes(struct db_node *, int, int, int, int, struct db_node *);
int  db_setnode(unsigned long, struct db_node *);
void db_rmnode(unsigned long);
unsigned long db_newaddress();

#endif
