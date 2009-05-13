

#ifndef _db_h
#define _db_h

int  db_init(char *, char *);
void db_free();
int db_load_engine_functions();
void db_lock(int);

#endif
