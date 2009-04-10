

#ifndef _db_h

struct db_node {
  char Name[32];
  short ManufacturerID;
  short ProductID;
  short UniqueIDPerProduct;
  unsigned long MambaNetAddr;
  unsigned long EngineAddr;
  unsigned char Services;
  unsigned char Active;
  unsigned char Parent[6];
};

int db_init(char *, char *);
void db_free();
int db_getnode(struct db_node *, unsigned long);

#endif
