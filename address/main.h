

#ifndef _main_h
#define _main_h

#include <mbn.h>

extern struct mbn_handler *mbn;

void writelog(char *, ...);
void set_address(unsigned long, unsigned short, unsigned short, unsigned short, unsigned long, unsigned char);

/* This trick allows us to call one function (set_address_struct) on any
 * structure that has the required fields, including struct mbn_address_node
 * and struct db_node. */
#define set_address_struct(addr, str) set_address(addr,\
  (str).ManufacturerID, (str).ProductID, (str).UniqueIDPerProduct, (str).EngineAddr, (str).Services)


#endif
