

#ifndef _db_h
#define _db_h

int  db_init(char *, char *);
void db_free();
int db_check_engine_functions();
int db_insert_engine_functions(const char *table, int function_number, const char *name, int rcv_type, int xmt_type); 
int db_read_slot_config();
int db_read_src_config(unsigned short int first_src, unsigned short int last_src);
int db_read_module_config(unsigned char first_mod, unsigned char last_mod);
int db_read_buss_config(unsigned char first_buss, unsigned char last_buss);
int db_read_monitor_buss_config(unsigned char first_mon_buss, unsigned char last_mon_buss);
int db_read_extern_src_config(unsigned char first_dsp_card, unsigned char last_dsp_card);
int db_read_talkback_config(unsigned char first_tb, unsigned char last_tb);
int db_read_global_config();
int db_read_dest_config(unsigned short int first_dest, unsigned short int last_dest);
int db_read_db_to_position();

int db_read_template_info(ONLINE_NODE_INFORMATION_STRUCT *node_info);
int db_read_node_defaults(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj);
int db_read_node_configuration(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj);

int db_update_rack_organization(unsigned char slot_nr, unsigned long int addr, unsigned char input_ch_cnt, unsigned char output_ch_cnt);
int db_update_rack_organization_input_ch_cnt(unsigned long int addr, unsigned char cnt); 
int db_update_rack_organization_output_ch_cnt(unsigned long int addr, unsigned char cnt);

int db_load_engine_functions();
void db_lock(int);

#endif
