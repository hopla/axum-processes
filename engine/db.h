

#ifndef _db_h
#define _db_h

int  db_init(char *, char *);
void db_free();
int db_check_engine_functions();
int db_insert_engine_functions(const char *table, int function_number, const char *name, int rcv_type, int xmt_type); 
int db_read_slot_config();
int db_read_src_config();
int db_read_module_config();
int db_read_buss_config();
int db_read_monitor_buss_config();
int db_read_extern_src_config();
int db_load_engine_functions();
void db_lock(int);

#endif
