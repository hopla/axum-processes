#ifndef _db_h
#define _db_h

#include "engine.h"

void db_open(char *dbstr);
int db_get_fd();
int db_get_matrix_sources();
int db_read_slot_config();
int db_read_src_preset(unsigned short int first_preset, unsigned short int last_preset);
int db_read_src_config(unsigned short int first_src, unsigned short int last_src);
int db_read_module_config(unsigned char first_mod, unsigned char last_mod, unsigned char console);
int db_read_buss_config(unsigned char first_buss, unsigned char last_buss, unsigned char console);
int db_read_monitor_buss_config(unsigned char first_mon_buss, unsigned char last_mon_buss, unsigned char console);
int db_read_extern_src_config(unsigned char first_dsp_card, unsigned char last_dsp_card);
int db_read_talkback_config(unsigned char first_tb, unsigned char last_tb);
int db_read_global_config();
int db_read_dest_config(unsigned short int first_dest, unsigned short int last_dest);
int db_read_db_to_position();
int db_read_routing_preset(unsigned char first_mod, unsigned char last_mod);
int db_read_buss_preset(unsigned short int first_preset, unsigned short int last_preset);
int db_read_buss_preset_rows(unsigned short int first_preset, unsigned short int last_preset);
int db_read_monitor_buss_preset_rows(unsigned short int first_preset, unsigned short int last_preset);
int db_read_console_preset(unsigned short int first_preset, unsigned short int last_preset);


int db_read_template_info(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned char in_powerup_state);
int db_read_node_defaults(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj, bool DoNotCheckCurrentDefault, bool SetFirmwareDefaults);
int db_read_node_config(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj);

int db_insert_slot_config(unsigned char slot_nr, unsigned long int addr, unsigned char input_ch_cnt, unsigned char output_ch_cnt);
int db_delete_slot_config(unsigned char slot_nr);
int db_update_slot_config_input_ch_cnt(unsigned long int addr, unsigned char cnt);
int db_update_slot_config_output_ch_cnt(unsigned long int addr, unsigned char cnt);
int db_empty_slot_config();

void db_lock(int);
void db_processnotifies();
void db_close();

//int db_load_engine_functions();

void db_processnotifies();

//notify callbacks
void db_event_templates_changed(char myself, char *arg);
void db_event_address_removed(char myself, char *arg);
void db_event_slot_config_changed(char myself, char *arg);
void db_event_src_config_changed(char myself, char *arg);
void db_event_module_config_changed(char myself, char *arg);
void db_event_buss_config_changed(char myself, char *arg);
void db_event_monitor_buss_config_changed(char myself, char *arg);
void db_event_extern_src_config_changed(char myself, char *arg);
void db_event_talkback_config_changed(char myself, char *arg);
void db_event_global_config_changed(char myself, char *arg);
void db_event_dest_config_changed(char myself, char *arg);
void db_event_node_config_changed(char myself, char *arg);
void db_event_defaults_changed(char myself, char *arg);
void db_event_src_preset_changed(char myself, char *arg);
void db_event_routing_preset_changed(char myself, char *arg);
void db_event_buss_preset_changed(char myself, char *arg);
void db_event_buss_preset_rows_changed(char myself, char *arg);
void db_event_monitor_buss_preset_rows_changed(char myself, char *arg);
void db_event_console_preset_changed(char myself, char *arg);

#endif
