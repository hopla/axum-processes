
-- global_config

INSERT INTO global_config (samplerate, headroom, level_reserve, ext_clock_addr)
  VALUES (48000, 20.0, 10.0, 0);


-- buss_config

INSERT INTO buss_config (number, label) VALUES (1, 'Prog');
INSERT INTO buss_config (number, label) VALUES (2, 'Sub');
INSERT INTO buss_config (number, label) VALUES (3, 'CUE');
INSERT INTO buss_config (number, label) VALUES (4, 'Comm');
INSERT INTO buss_config (number, label) VALUES (5, 'Aux 1');
INSERT INTO buss_config (number, label) VALUES (6, 'Aux 2');
INSERT INTO buss_config (number, label) VALUES (7, 'Aux 3');
INSERT INTO buss_config (number, label) VALUES (8, 'Aux 4');
INSERT INTO buss_config (number, label) VALUES (9, 'Aux 5');
INSERT INTO buss_config (number, label) VALUES (10, 'Aux 6');
INSERT INTO buss_config (number, label) VALUES (11, 'Aux 7');
INSERT INTO buss_config (number, label) VALUES (12, 'Aux 8');
INSERT INTO buss_config (number, label) VALUES (13, 'Aux 9');
INSERT INTO buss_config (number, label) VALUES (14, 'Aux 10');
INSERT INTO buss_config (number, label) VALUES (15, 'Aux 11');
INSERT INTO buss_config (number, label) VALUES (16, 'Aux 12');


-- console_config

INSERT INTO console_config (number, name, location, contact) VALUES (1, 'AXUM 1', 'Unkown', 'Unknown');
INSERT INTO console_config (number, name, location, contact) VALUES (2, 'AXUM 2', 'Unkown', 'Unknown');
INSERT INTO console_config (number, name, location, contact) VALUES (3, 'AXUM 3', 'Unkown', 'Unknown');
INSERT INTO console_config (number, name, location, contact) VALUES (4, 'AXUM 4', 'Unkown', 'Unknown');


-- monitor_buss_config

INSERT INTO monitor_buss_config (number, label) VALUES (1, 'CRM');
INSERT INTO monitor_buss_config (number, label) VALUES (2, 'Std 1');
INSERT INTO monitor_buss_config (number, label) VALUES (3, 'Std 2');
INSERT INTO monitor_buss_config (number, label) VALUES (4, 'Std 3');
INSERT INTO monitor_buss_config (number, label) VALUES (5, 'Std 4');
INSERT INTO monitor_buss_config (number, label) VALUES (6, 'Std 5');
INSERT INTO monitor_buss_config (number, label) VALUES (7, 'Std 6');
INSERT INTO monitor_buss_config (number, label) VALUES (8, 'Std 7');
INSERT INTO monitor_buss_config (number, label) VALUES (9, 'Std 8');
INSERT INTO monitor_buss_config (number, label) VALUES (10, 'Std 9');
INSERT INTO monitor_buss_config (number, label) VALUES (11, 'Std 10');
INSERT INTO monitor_buss_config (number, label) VALUES (12, 'Std 11');
INSERT INTO monitor_buss_config (number, label) VALUES (13, 'Std 12');
INSERT INTO monitor_buss_config (number, label) VALUES (14, 'Std 13');
INSERT INTO monitor_buss_config (number, label) VALUES (15, 'Std 14');
INSERT INTO monitor_buss_config (number, label) VALUES (16, 'Std 15');


-- extern_src_config

INSERT INTO extern_src_config (number, ext1, ext2, ext3, ext4, ext5, ext6, ext7, ext8) VALUES(1, 0, 0, 0, 0, 0, 0, 0, 0);
INSERT INTO extern_src_config (number, ext1, ext2, ext3, ext4, ext5, ext6, ext7, ext8) VALUES(2, 0, 0, 0, 0, 0, 0, 0, 0);
INSERT INTO extern_src_config (number, ext1, ext2, ext3, ext4, ext5, ext6, ext7, ext8) VALUES(3, 0, 0, 0, 0, 0, 0, 0, 0);
INSERT INTO extern_src_config (number, ext1, ext2, ext3, ext4, ext5, ext6, ext7, ext8) VALUES(4, 0, 0, 0, 0, 0, 0, 0, 0);


-- talkback_config

INSERT INTO talkback_config (number, source) VALUES (1, 0);
INSERT INTO talkback_config (number, source) VALUES (2, 0);
INSERT INTO talkback_config (number, source) VALUES (3, 0);
INSERT INTO talkback_config (number, source) VALUES (4, 0);
INSERT INTO talkback_config (number, source) VALUES (5, 0);
INSERT INTO talkback_config (number, source) VALUES (6, 0);
INSERT INTO talkback_config (number, source) VALUES (7, 0);
INSERT INTO talkback_config (number, source) VALUES (8, 0);
INSERT INTO talkback_config (number, source) VALUES (9, 0);
INSERT INTO talkback_config (number, source) VALUES (10, 0);
INSERT INTO talkback_config (number, source) VALUES (11, 0);
INSERT INTO talkback_config (number, source) VALUES (12, 0);
INSERT INTO talkback_config (number, source) VALUES (13, 0);
INSERT INTO talkback_config (number, source) VALUES (14, 0);
INSERT INTO talkback_config (number, source) VALUES (15, 0);
INSERT INTO talkback_config (number, source) VALUES (16, 0);


-- module_config

INSERT INTO module_config (number) SELECT * FROM generate_series(1, 128);
INSERT INTO routing_preset (mod_number, mod_input) SELECT *, 'A' FROM generate_series(1, 128);
INSERT INTO routing_preset (mod_number, mod_input) SELECT *, 'B' FROM generate_series(1, 128);
INSERT INTO routing_preset (mod_number, mod_input) SELECT *, 'C' FROM generate_series(1, 128);
INSERT INTO routing_preset (mod_number, mod_input) SELECT *, 'D' FROM generate_series(1, 128);

