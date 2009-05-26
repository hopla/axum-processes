

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
int db_read_talkback_config();
int db_read_global_config();
int db_read_dest_config();
int db_read_db_to_position();

int db_read_template_info(ONLINE_NODE_INFORMATION_STRUCT *node_info);
int db_read_node_defaults(ONLINE_NODE_INFORMATION_STRUCT *node_info);
int db_read_node_configuration(ONLINE_NODE_INFORMATION_STRUCT *node_info);
int db_update_rack_organization(unsigned char slot_nr, unsigned long int addr, unsigned char input_ch_cnt, unsigned char output_ch_cnt);
int db_update_rack_organization_input_ch_cnt(unsigned long int addr, unsigned char cnt); 
int db_update_rack_organization_output_ch_cnt(unsigned long int addr, unsigned char cnt);

int db_load_engine_functions();
void db_lock(int);

#endif
