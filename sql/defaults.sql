
-- global_config

INSERT INTO global_config (samplerate, ext_clock, headroom, level_reserve)
  VALUES (48000, false, 20.0, 10.0);


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

