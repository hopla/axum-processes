

#ifndef _db_h

struct db_node {
  char Name[32];
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned long MambaNetAddr;
  unsigned long EngineAddr;
  unsigned char Services;
  unsigned char Active;
  unsigned short Parent[3];
};

int db_init(char *, char *);
void db_free();
int db_getnode(struct db_node *, unsigned long);
int db_setnode(unsigned long, struct db_node *);

#endif
