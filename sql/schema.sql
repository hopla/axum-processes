-- On an empty PostgreSQL installation, execute the following to
-- create and load the DB for use with the axum software:
--
--  $ createuser -U postgres -S -D -R axum
--  $ createdb -U postgres axum -O axum
--  $ psql -U axum <schema.sql
--  $ psql -U axum <db_to_position.sql
--  $ psql -U axum <defaults.sql
--  $ psql -U axum <functions.sql
--  $ psql -U axum <triggers.sql
--  $ psql -U axum <util.sql


-- General TODO list (not too important)
--  - Operator classes for the custom types
--  - More sanity checking (NULL values on custom types)
--  - Foreign key: node_config(func) -> functions(func)
--  - more triggers


CREATE LANGUAGE plpgsql;



-- T Y P E S

CREATE TYPE function_number AS (
  type integer,
  seq integer,
  func integer
);

CREATE TYPE mambanet_data AS (
  int integer,
  fl float,
  bits bit varying(64),
  str varchar(64)
);

CREATE TYPE mambanet_minmax AS (
  int integer,
  fl float
);

CREATE TYPE mambanet_unique_id AS (
  man smallint,
  prod smallint,
  id smallint
);




-- T A B L E S

CREATE TABLE node_config (
  addr integer NOT NULL,
  object integer NOT NULL,
  firm_major integer NOT NULL,
  func function_number NOT NULL,
  label varchar(16),
  PRIMARY KEY(addr, object, firm_major)
);

CREATE TABLE defaults (
  addr integer NOT NULL,
  object integer NOT NULL,
  firm_major integer NOT NULL,
  data mambanet_data NOT NULL,
  PRIMARY KEY(addr, object, firm_major)
);

CREATE TABLE templates (
  man_id smallint NOT NULL,
  prod_id smallint NOT NULL,
  firm_major smallint NOT NULL,
  number integer NOT NULL,
  description varchar(32) NOT NULL,
  services smallint NOT NULL CHECK (services >= 0 AND services <= 3),
  sensor_type smallint NOT NULL CHECK(sensor_type >= 0 AND sensor_type <= 6),
  sensor_size smallint CHECK(sensor_size >= 0 AND sensor_size <= 64),
  sensor_min mambanet_minmax,
  sensor_max mambanet_minmax,
  actuator_type smallint NOT NULL CHECK(actuator_type >= 0 AND actuator_type <= 6),
  actuator_size smallint CHECK(actuator_size >= 0 AND actuator_size <= 64),
  actuator_min mambanet_minmax,
  actuator_max mambanet_minmax,
  actuator_def mambanet_minmax,
  PRIMARY KEY(man_id, prod_id, firm_major, number)
);

CREATE TABLE functions (
  func function_number NOT NULL,
  name varchar(64) NOT NULL,
  rcv_type smallint NOT NULL CHECK(rcv_type >= 0 AND rcv_type <= 6),
  xmt_type smallint NOT NULL CHECK(xmt_type >= 0 AND xmt_type <= 6),
  label varchar(16) NOT NULL DEFAULT 'No label',
  pos smallint NOT NULL DEFAULT 9999,
);
CREATE UNIQUE INDEX functions_unique ON functions (rcv_type, xmt_type, ((func).type), ((func).seq), ((func).func));

CREATE TABLE addresses (
  addr integer NOT NULL PRIMARY KEY,
  name VARCHAR(32),
  id mambanet_unique_id NOT NULL,
  engine_addr integer NOT NULL,
  services smallint NOT NULL,
  active boolean NOT NULL DEFAULT FALSE,
  parent mambanet_unique_id NOT NULL DEFAULT ROW(0,0,0),
  setname boolean NOT NULL DEFAULT FALSE,
  refresh boolean NOT NULL DEFAULT FALSE,
  firstseen timestamp NOT NULL DEFAULT NOW(),
  lastseen timestamp NOT NULL DEFAULT NOW(),
  addr_requests integer NOT NULL DEFAULT 0,
  firm_major smallint
);
CREATE UNIQUE INDEX addresses_unique_id ON addresses (((id).man), ((id).prod), ((id).id));

CREATE TABLE recent_changes (
  change varchar(32) NOT NULL,
  arguments varchar(64) NOT NULL,
  timestamp timestamp NOT NULL DEFAULT NOW(),
  pid integer NOT NULL DEFAULT pg_backend_pid()
);

CREATE TABLE slot_config (
  slot_nr smallint NOT NULL CHECK(slot_nr>=1 AND slot_nr<=42) PRIMARY KEY,
  addr integer NOT NULL,
  input_ch_cnt integer,
  output_ch_cnt integer
);

CREATE TABLE src_config (
  pos smallint NOT NULL DEFAULT 9999,
  number smallint NOT NULL CHECK (number>=1 AND number<=1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
  input1_addr integer NOT NULL,
  input1_sub_ch smallint NOT NULL CHECK(input1_sub_ch>=0 AND input1_sub_ch<=32),
  input2_addr integer NOT NULL,
  input2_sub_ch smallint NOT NULL CHECK(input2_sub_ch>=0 AND input2_sub_ch<=32),
  input_phantom boolean NOT NULL DEFAULT FALSE,
  input_pad boolean NOT NULL DEFAULT FALSE,
  input_gain float NOT NULL DEFAULT 30 CHECK(input_gain >= 20::double precision AND input_gain <= 75::double precision),
  default_src_preset smallint DEFAULT NULL CHECK(default_src_preset>=1 AND default_src_preset<=1280),
  start_trigger smallint DEFAULT 0 NOT NULL CHECK(start_trigger>=0 AND start_trigger <=3),
  stop_trigger smallint DEFAULT 0 NOT NULL CHECK(stop_trigger>=0 AND stop_trigger <=3);
  redlight1 boolean NOT NULL DEFAULT FALSE,
  redlight2 boolean NOT NULL DEFAULT FALSE,
  redlight3 boolean NOT NULL DEFAULT FALSE,
  redlight4 boolean NOT NULL DEFAULT FALSE,
  redlight5 boolean NOT NULL DEFAULT FALSE,
  redlight6 boolean NOT NULL DEFAULT FALSE,
  redlight7 boolean NOT NULL DEFAULT FALSE,
  redlight8 boolean NOT NULL DEFAULT FALSE,
  monitormute1 boolean NOT NULL DEFAULT FALSE,
  monitormute2 boolean NOT NULL DEFAULT FALSE,
  monitormute3 boolean NOT NULL DEFAULT FALSE,
  monitormute4 boolean NOT NULL DEFAULT FALSE,
  monitormute5 boolean NOT NULL DEFAULT FALSE,
  monitormute6 boolean NOT NULL DEFAULT FALSE,
  monitormute7 boolean NOT NULL DEFAULT FALSE,
  monitormute8 boolean NOT NULL DEFAULT FALSE,
  monitormute9 boolean NOT NULL DEFAULT FALSE,
  monitormute10 boolean NOT NULL DEFAULT FALSE,
  monitormute11 boolean NOT NULL DEFAULT FALSE,
  monitormute12 boolean NOT NULL DEFAULT FALSE,
  monitormute13 boolean NOT NULL DEFAULT FALSE,
  monitormute14 boolean NOT NULL DEFAULT FALSE,
  monitormute15 boolean NOT NULL DEFAULT FALSE,
  monitormute16 boolean NOT NULL DEFAULT FALSE
);

CREATE TABLE module_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=128) PRIMARY KEY,
  source_a smallint NOT NULL DEFAULT 0,
  source_a_preset smallint,
  source_b smallint NOT NULL DEFAULT 0,
  source_b_preset smallint,
  source_c smallint NOT NULL DEFAULT 0,
  source_c_preset smallint,
  source_d smallint NOT NULL DEFAULT 0,
  source_d_preset smallint,
  source_e smallint NOT NULL DEFAULT 0,
  source_e_preset smallint,
  source_f smallint NOT NULL DEFAULT 0,
  source_f_preset smallint,
  source_g smallint NOT NULL DEFAULT 0,
  source_g_preset smallint,
  source_h smallint NOT NULL DEFAULT 0,
  source_h_preset smallint,
  overrule_active boolean NOT NULL DEFAULT FALSE,
  use_insert_preset boolean NOT NULL DEFAULT FALSE,
  insert_source smallint NOT NULL DEFAULT 0,
  insert_on_off boolean NOT NULL DEFAULT FALSE,
  use_gain_preset boolean NOT NULL DEFAULT FALSE,
  gain float NOT NULL DEFAULT 0,
  use_lc_preset boolean NOT NULL DEFAULT FALSE,
  lc_frequency smallint NOT NULL DEFAULT 80 CHECK(lc_frequency>=40 AND lc_frequency<=12000),
  lc_on_off boolean NOT NULL DEFAULT FALSE,
  use_phase_preset boolean NOT NULL DEFAULT FALSE,
  phase smallint NOT NULL DEFAULT 3 CHECK(phase>=0 AND phase<=3),
  phase_on_off boolean NOT NULL DEFAULT FALSE,
  use_mono_preset boolean NOT NULL DEFAULT FALSE,
  mono smallint NOT NULL DEFAULT 3 CHECK(mono>=0 AND mono<=3),
  mono_on_off boolean NOT NULL DEFAULT FALSE,
  use_eq_preset boolean NOT NULL DEFAULT FALSE,
  eq_band_1_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_1_range>=0 AND eq_band_1_range<=18),
  eq_band_1_level float NOT NULL DEFAULT 0 CHECK(eq_band_1_level>=eq_band_1_range AND eq_band_1_level<=eq_band_1_range),
  eq_band_1_freq smallint NOT NULL DEFAULT 7000 CHECK(eq_band_1_freq>=10 AND eq_band_1_freq<=20000),
  eq_band_1_bw float NOT NULL DEFAULT 1 CHECK(eq_band_1_bw>=0.1 AND eq_band_1_bw<=10),
  eq_band_1_slope float NOT NULL DEFAULT 1 CHECK(eq_band_1_slope>=0.1 AND eq_band_1_slope<=10),
  eq_band_1_type smallint NOT NULL DEFAULT 4 CHECK(eq_band_1_type>=0 AND eq_band_1_type<=7),
  eq_band_2_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_2_range>=0 AND eq_band_2_range<=18),
  eq_band_2_level float NOT NULL DEFAULT 0 CHECK(eq_band_2_level>=-eq_band_2_range AND eq_band_2_level<=eq_band_2_range),
  eq_band_2_freq smallint NOT NULL DEFAULT 2000 CHECK(eq_band_2_freq>=10 AND eq_band_2_freq<=20000),
  eq_band_2_bw float NOT NULL DEFAULT 3 CHECK(eq_band_2_bw>=0.1 AND eq_band_2_bw<=10),
  eq_band_2_slope float NOT NULL DEFAULT 1 CHECK(eq_band_2_slope>=0.1 AND eq_band_2_slope<=10),
  eq_band_2_type smallint NOT NULL DEFAULT 3 CHECK(eq_band_2_type>=0 AND eq_band_2_type<=7),
  eq_band_3_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_3_range>=0 AND eq_band_3_range<=18),
  eq_band_3_level float NOT NULL DEFAULT 0 CHECK(eq_band_3_level>=-eq_band_3_range AND eq_band_3_level<=eq_band_3_range),
  eq_band_3_freq smallint NOT NULL DEFAULT 300 CHECK(eq_band_3_freq>=10 AND eq_band_3_freq<=20000),
  eq_band_3_bw float NOT NULL DEFAULT 1 CHECK(eq_band_3_bw>=0.1 AND eq_band_3_bw<=10),
  eq_band_3_slope float NOT NULL DEFAULT 1 CHECK(eq_band_3_slope>=0.1 AND eq_band_3_slope<=10),
  eq_band_3_type smallint NOT NULL DEFAULT 2 CHECK(eq_band_3_type>=0 AND eq_band_3_type<=7),
  eq_band_4_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_4_range>=0 AND eq_band_4_range<=18),
  eq_band_4_level float NOT NULL DEFAULT 0 CHECK(eq_band_4_level>=-eq_band_4_range AND eq_band_4_level<=eq_band_4_range),
  eq_band_4_freq smallint NOT NULL DEFAULT 120 CHECK(eq_band_4_freq>=10 AND eq_band_4_freq<=20000),
  eq_band_4_bw float NOT NULL DEFAULT 1 CHECK(eq_band_4_bw>=0.1 AND eq_band_4_bw<=10),
  eq_band_4_slope float NOT NULL DEFAULT 1 CHECK(eq_band_4_slope>=0.1 AND eq_band_4_slope<=10),
  eq_band_4_type smallint NOT NULL DEFAULT 0 CHECK(eq_band_4_type>=0 AND eq_band_4_type<=7),
  eq_band_5_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_5_range>=0 AND eq_band_5_range<=18),
  eq_band_5_level float NOT NULL DEFAULT 0 CHECK(eq_band_5_level>=-eq_band_5_range AND eq_band_5_level<=eq_band_5_range),
  eq_band_5_freq smallint NOT NULL DEFAULT 12000 CHECK(eq_band_5_freq>=10 AND eq_band_5_freq<=20000),
  eq_band_5_bw float NOT NULL DEFAULT 1 CHECK(eq_band_5_bw>=0.1 AND eq_band_5_bw<=10),
  eq_band_5_slope float NOT NULL DEFAULT 1 CHECK(eq_band_5_slope>=0.1 AND eq_band_5_slope<=10),
  eq_band_5_type smallint NOT NULL DEFAULT 0 CHECK(eq_band_5_type>=0 AND eq_band_5_type<=7),
  eq_band_6_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_6_range>=0 AND eq_band_6_range<=18),
  eq_band_6_level float NOT NULL DEFAULT 0 CHECK(eq_band_6_level>=-eq_band_6_range AND eq_band_6_level<=eq_band_6_range),
  eq_band_6_freq smallint NOT NULL DEFAULT 120 CHECK(eq_band_6_freq>=10 AND eq_band_6_freq<=20000),
  eq_band_6_bw float NOT NULL DEFAULT 1 CHECK(eq_band_6_bw>=0.1 AND eq_band_6_bw<=10),
  eq_band_6_slope float NOT NULL DEFAULT 1 CHECK(eq_band_6_slope>=0.1 AND eq_band_6_slope<=10),
  eq_band_6_type smallint NOT NULL DEFAULT 0 CHECK(eq_band_6_type>=0 AND eq_band_6_type<=7),
  eq_on_off boolean NOT NULL DEFAULT FALSE,
  use_dyn_preset boolean NOT NULL DEFAULT FALSE,
  d_exp_threshold float NOT NULL DEFAULT -30 CHECK(d_exp_threshold>=-50 AND d_exp_threshold<=0),
  agc_amount smallint NOT NULL DEFAULT 0 CHECK(agc_amount>=0 AND agc_amount<=100),
  agc_threshold float NOT NULL DEFAULT -20 CHECK(agc_threshold>=-30 AND agc_threshold<=0),
  dyn_on_off boolean NOT NULL DEFAULT FALSE,
  use_mod_preset boolean NOT NULL DEFAULT FALSE,
  mod_pan smallint NOT NULL DEFAULT 512 CHECK(mod_pan>=0 AND mod_pan<=1023),
  mod_level float NOT NULL DEFAULT -140 CHECK(mod_level>=-140 AND mod_level<=10),
  mod_on_off boolean NOT NULL DEFAULT FALSE,
  console smallint NOT NULL DEFAULT 1 CHECK(console>=1 AND console<=4),
  buss_1_2_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_1_2_level float NOT NULL DEFAULT 0 CHECK(buss_1_2_level>=-140 AND buss_1_2_level<=10),
  buss_1_2_on_off boolean NOT NULL DEFAULT TRUE,
  buss_1_2_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_1_2_balance smallint NOT NULL DEFAULT 512] CHECK(buss_1_2_balance>=0 AND buss_1_2_balance<=1023),
  buss_1_2_assignment boolean NOT NULL DEFAULT TRUE,
  buss_3_4_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_3_4_level float NOT NULL DEFAULT 0 CHECK(buss_3_4_level>=-140 AND buss_3_4_level<=10),
  buss_3_4_on_off boolean NOT NULL DEFAULT TRUE,
  buss_3_4_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_3_4_balance smallint NOT NULL DEFAULT 512] CHECK(buss_3_4_balance>=0 AND buss_3_4_balance<=1023),
  buss_3_4_assignment boolean NOT NULL DEFAULT TRUE,
  buss_5_6_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_5_6_level float NOT NULL DEFAULT 0 CHECK(buss_5_6_level>=-140 AND buss_5_6_level<=10),
  buss_5_6_on_off boolean NOT NULL DEFAULT TRUE,
  buss_5_6_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_5_6_balance smallint NOT NULL DEFAULT 512] CHECK(buss_5_6_balance>=0 AND buss_5_6_balance<=1023),
  buss_5_6_assignment boolean NOT NULL DEFAULT TRUE,
  buss_7_8_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_7_8_level float NOT NULL DEFAULT 0 CHECK(buss_7_8_level>=-140 AND buss_7_8_level<=10),
  buss_7_8_on_off boolean NOT NULL DEFAULT TRUE,
  buss_7_8_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_7_8_balance smallint NOT NULL DEFAULT 512] CHECK(buss_7_8_balance>=0 AND buss_7_8_balance<=1023),
  buss_7_8_assignment boolean NOT NULL DEFAULT TRUE,
  buss_9_10_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_9_10_level float NOT NULL DEFAULT 0 CHECK(buss_9_10_level>=-140 AND buss_9_10_level<=10),
  buss_9_10_on_off boolean NOT NULL DEFAULT TRUE,
  buss_9_10_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_9_10_balance smallint NOT NULL DEFAULT 512] CHECK(buss_9_10_balance>=0 AND buss_9_10_balance<=1023),
  buss_9_10_assignment boolean NOT NULL DEFAULT TRUE,
  buss_11_12_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_11_12_level float NOT NULL DEFAULT 0 CHECK(buss_11_12_level>=-140 AND buss_11_12_level<=10),
  buss_11_12_on_off boolean NOT NULL DEFAULT TRUE,
  buss_11_12_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_11_12_balance smallint NOT NULL DEFAULT 512] CHECK(buss_11_12_balance>=0 AND buss_11_12_balance<=1023),
  buss_11_12_assignment boolean NOT NULL DEFAULT TRUE,
  buss_13_14_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_13_14_level float NOT NULL DEFAULT 0 CHECK(buss_13_14_level>=-140 AND buss_13_14_level<=10),
  buss_13_14_on_off boolean NOT NULL DEFAULT TRUE,
  buss_13_14_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_13_14_balance smallint NOT NULL DEFAULT 512] CHECK(buss_13_14_balance>=0 AND buss_13_14_balance<=1023),
  buss_13_14_assignment boolean NOT NULL DEFAULT TRUE,
  buss_15_16_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_15_16_level float NOT NULL DEFAULT 0 CHECK(buss_15_16_level>=-140 AND buss_15_16_level<=10),
  buss_15_16_on_off boolean NOT NULL DEFAULT TRUE,
  buss_15_16_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_15_16_balance smallint NOT NULL DEFAULT 512] CHECK(buss_15_16_balance>=0 AND buss_15_16_balance<=1023),
  buss_15_16_assignment boolean NOT NULL DEFAULT TRUE,
  buss_17_18_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_17_18_level float NOT NULL DEFAULT 0 CHECK(buss_17_18_level>=-140 AND buss_17_18_level<=10),
  buss_17_18_on_off boolean NOT NULL DEFAULT TRUE,
  buss_17_18_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_17_18_balance smallint NOT NULL DEFAULT 512] CHECK(buss_17_18_balance>=0 AND buss_17_18_balance<=1023),
  buss_17_18_assignment boolean NOT NULL DEFAULT TRUE,
  buss_19_20_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_19_20_level float NOT NULL DEFAULT 0 CHECK(buss_19_20_level>=-140 AND buss_19_20_level<=10),
  buss_19_20_on_off boolean NOT NULL DEFAULT TRUE,
  buss_19_20_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_19_20_balance smallint NOT NULL DEFAULT 512] CHECK(buss_19_20_balance>=0 AND buss_19_20_balance<=1023),
  buss_19_20_assignment boolean NOT NULL DEFAULT TRUE,
  buss_21_22_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_21_22_level float NOT NULL DEFAULT 0 CHECK(buss_21_22_level>=-140 AND buss_21_22_level<=10),
  buss_21_22_on_off boolean NOT NULL DEFAULT TRUE,
  buss_21_22_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_21_22_balance smallint NOT NULL DEFAULT 512] CHECK(buss_21_22_balance>=0 AND buss_21_22_balance<=1023),
  buss_21_22_assignment boolean NOT NULL DEFAULT TRUE,
  buss_23_24_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_23_24_level float NOT NULL DEFAULT 0 CHECK(buss_23_24_level>=-140 AND buss_23_24_level<=10),
  buss_23_24_on_off boolean NOT NULL DEFAULT TRUE,
  buss_23_24_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_23_24_balance smallint NOT NULL DEFAULT 512] CHECK(buss_23_24_balance>=0 AND buss_23_24_balance<=1023),
  buss_23_24_assignment boolean NOT NULL DEFAULT TRUE,
  buss_25_26_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_25_26_level float NOT NULL DEFAULT 0 CHECK(buss_25_26_level>=-140 AND buss_25_26_level<=10),
  buss_25_26_on_off boolean NOT NULL DEFAULT TRUE,
  buss_25_26_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_25_26_balance smallint NOT NULL DEFAULT 512] CHECK(buss_25_26_balance>=0 AND buss_25_26_balance<=1023),
  buss_25_26_assignment boolean NOT NULL DEFAULT TRUE,
  buss_27_28_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_27_28_level float NOT NULL DEFAULT 0 CHECK(buss_27_28_level>=-140 AND buss_27_28_level<=10),
  buss_27_28_on_off boolean NOT NULL DEFAULT TRUE,
  buss_27_28_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_27_28_balance smallint NOT NULL DEFAULT 512] CHECK(buss_27_28_balance>=0 AND buss_27_28_balance<=1023),
  buss_27_28_assignment boolean NOT NULL DEFAULT TRUE,
  buss_29_30_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_29_30_level float NOT NULL DEFAULT 0 CHECK(buss_29_30_level>=-140 AND buss_29_30_level<=10),
  buss_29_30_on_off boolean NOT NULL DEFAULT TRUE,
  buss_29_30_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_29_30_balance smallint NOT NULL DEFAULT 512] CHECK(buss_29_30_balance>=0 AND buss_29_30_balance<=1023),
  buss_29_30_assignment boolean NOT NULL DEFAULT TRUE,
  buss_31_32_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_31_32_level float NOT NULL DEFAULT 0 CHECK(buss_31_32_level>=-140 AND buss_31_32_level<=10),
  buss_31_32_on_off boolean NOT NULL DEFAULT TRUE,
  buss_31_32_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_31_32_balance smallint NOT NULL DEFAULT 512] CHECK(buss_31_32_balance>=0 AND buss_31_32_balance<=1023),
  buss_31_32_assignment boolean NOT NULL DEFAULT TRUE
);

CREATE TABLE buss_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=16) PRIMARY KEY,
  label varchar(32) NOT NULL,
  pre_on boolean NOT NULL DEFAULT FALSE,
  pre_balance boolean NOT NULL DEFAULT FALSE,
  mono boolean NOT NULL DEFAULT FALSE,
  level float NOT NULL DEFAULT 0.0,
  on_off boolean NOT NULL DEFAULT TRUE,
  interlock boolean NOT NULL DEFAULT FALSE,
  exclusive boolean NOT NULL DEFAULT FALSE,
  global_reset boolean NOT NULL DEFAULT FALSE,
  console smallint NOT NULL DEFAULT 1 CHECK(console>=1 AND console<=4)
);

CREATE TABLE monitor_buss_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=16) PRIMARY KEY,
  label varchar(32) NOT NULL,
  interlock boolean NOT NULL DEFAULT TRUE,
  default_selection smallint NOT NULL DEFAULT 0,
  buss_1_2 boolean NOT NULL DEFAULT FALSE,
  buss_3_4 boolean NOT NULL DEFAULT FALSE,
  buss_5_6 boolean NOT NULL DEFAULT FALSE,
  buss_7_8 boolean NOT NULL DEFAULT FALSE,
  buss_9_10 boolean NOT NULL DEFAULT FALSE,
  buss_11_12 boolean NOT NULL DEFAULT FALSE,
  buss_13_14 boolean NOT NULL DEFAULT FALSE,
  buss_15_16 boolean NOT NULL DEFAULT FALSE,
  buss_17_18 boolean NOT NULL DEFAULT FALSE,
  buss_19_20 boolean NOT NULL DEFAULT FALSE,
  buss_21_22 boolean NOT NULL DEFAULT FALSE,
  buss_23_24 boolean NOT NULL DEFAULT FALSE,
  buss_25_26 boolean NOT NULL DEFAULT FALSE,
  buss_27_28 boolean NOT NULL DEFAULT FALSE,
  buss_29_30 boolean NOT NULL DEFAULT FALSE,
  buss_31_32 boolean NOT NULL DEFAULT FALSE,
  dim_level float NOT NULL DEFAULT -20.0 CHECK(dim_level>=-140 AND dim_level<=0),
  console smallint NOT NULL DEFAULT 1 CHECK(console>=1 AND console<=4)
);

CREATE TABLE extern_src_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=4) PRIMARY KEY,
  ext1 smallint NOT NULL DEFAULT 0,
  ext2 smallint NOT NULL DEFAULT 0,
  ext3 smallint NOT NULL DEFAULT 0,
  ext4 smallint NOT NULL DEFAULT 0,
  ext5 smallint NOT NULL DEFAULT 0,
  ext6 smallint NOT NULL DEFAULT 0,
  ext7 smallint NOT NULL DEFAULT 0,
  ext8 smallint NOT NULL DEFAULT 0,
  safe1 boolean NOT NULL DEFAULT TRUE,
  safe2 boolean NOT NULL DEFAULT TRUE,
  safe3 boolean NOT NULL DEFAULT TRUE,
  safe4 boolean NOT NULL DEFAULT TRUE,
  safe5 boolean NOT NULL DEFAULT TRUE,
  safe6 boolean NOT NULL DEFAULT TRUE,
  safe7 boolean NOT NULL DEFAULT TRUE,
  safe8 boolean NOT NULL DEFAULT TRUE
);

CREATE TABLE talkback_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=16) PRIMARY KEY,
  source smallint NOT NULL
);

CREATE TABLE global_config (
  samplerate integer NOT NULL DEFAULT 48000,
  ext_clock boolean NOT NULL DEFAULT FALSE,
  headroom float NOT NULL DEFAULT 20.0,
  level_reserve float NOT NULL DEFAULT 0.0,
  auto_momentary boolean NOT NULL DEFAULT TRUE,
  startup_state boolean NOT NULL DEFAULT FALSE,
  date_time varchar(19) NOT NULL DEFAULT '0000-00-00 00:00:00'
);

CREATE TABLE dest_config (
  pos smallint NOT NULL DEFAULT 9999,
  number smallint NOT NULL CHECK(number>=1 AND number<=1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
  output1_addr integer,
  output1_sub_ch smallint NOT NULL CHECK(output1_sub_ch>=0 AND output1_sub_ch<32),
  output2_addr integer,
  output2_sub_ch smallint NOT NULL CHECK(output2_sub_ch>=0 AND output2_sub_ch<32),
  level float NOT NULL DEFAULT 0,
  source integer NOT NULL DEFAULT 0,
  routing smallint NOT NULL DEFAULT 0 CHECK(routing>=0 AND routing<=3),
  mix_minus_source integer NOT NULL DEFAULT 0
);

CREATE TABLE db_to_position (
  db float PRIMARY KEY CHECK(db >= -140.0 AND db < 10.0),
  position smallint CHECK(position >= 0 AND position < 1024)
);

CREATE TABLE predefined_node_config (
  man_id smallint NOT NULL,
  prod_id smallint NOT NULL,
  firm_major smallint NOT NULL,
  cfg_name varchar(32) NOT NULL,
  object integer NOT NULL,
  func function_number NOT NULL,
  label varchar(16),
  PRIMARY KEY(man_id, prod_id, firm_major, cfg_name, object)
);

CREATE TABLE predefined_node_defaults (
  man_id smallint NOT NULL,
  prod_id smallint NOT NULL,
  firm_major smallint NOT NULL,
  cfg_name varchar(32) NOT NULL,
  object integer NOT NULL,
  data mambanet_data NOT NULL,
  PRIMARY KEY(man_id, prod_id, firm_major, cfg_name, object)
);

CREATE TABLE src_preset (
  pos smallint NOT NULL DEFAULT 9999,
  number smallint NOT NULL CHECK (number>=1 AND number<=1280) PRIMARY KEY,
  label varchar(32) NOT NULL DEFAULT 'preset',
  use_gain_preset boolean NOT NULL DEFAULT FALSE,
  gain float NOT NULL DEFAULT 0 CHECK(gain>=-20 AND gain<20),
  use_lc_preset boolean NOT NULL DEFAULT FALSE,
  lc_on_off boolean NOT NULL DEFAULT FALSE,
  lc_frequency smallint NOT NULL DEFAULT 80 CHECK(lc_frequency>=40 AND lc_frequency<=12000),
  use_insert_preset boolean NOT NULL DEFAULT FALSE,
  insert_on_off boolean NOT NULL DEFAULT FALSE,
  use_phase_preset boolean NOT NULL DEFAULT FALSE,
  phase_on_off boolean NOT NULL DEFAULT FALSE,
  phase smallint NOT NULL DEFAULT 3 CHECK(phase>=0 AND phase<=3),
  use_mono_preset boolean NOT NULL DEFAULT FALSE,
  mono_on_off boolean NOT NULL DEFAULT FALSE,
  mono smallint NOT NULL DEFAULT 3 CHECK(mono>=0 AND mono<=3),
  use_eq_preset boolean NOT NULL DEFAULT FALSE,
  eq_on_off boolean NOT NULL DEFAULT FALSE,
  eq_band_1_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_1_range>=0 AND eq_band_1_range<=18),
  eq_band_1_level float NOT NULL DEFAULT 0 CHECK(eq_band_1_level>=-eq_band_1_range AND eq_band_1_level<=eq_band_1_range),
  eq_band_1_freq smallint NOT NULL DEFAULT 7000 CHECK(eq_band_1_freq>=10 AND eq_band_1_freq<=20000),
  eq_band_1_bw float NOT NULL DEFAULT 1 CHECK(eq_band_1_bw>=0.1 AND eq_band_1_bw<=10),
  eq_band_1_slope float NOT NULL DEFAULT 1 CHECK(eq_band_1_slope>=0.1 AND eq_band_1_slope<=10),
  eq_band_1_type smallint NOT NULL DEFAULT 4 CHECK(eq_band_1_type>=0 AND eq_band_1_type<=7),
  eq_band_2_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_2_range>=0 AND eq_band_2_range<=18),
  eq_band_2_level float NOT NULL DEFAULT 0 CHECK(eq_band_2_level>=-eq_band_2_range AND eq_band_2_level<=eq_band_2_range),
  eq_band_2_freq smallint NOT NULL DEFAULT 2000 CHECK(eq_band_2_freq>=10 AND eq_band_2_freq<=20000),
  eq_band_2_bw float NOT NULL DEFAULT 3 CHECK(eq_band_2_bw>=0.1 AND eq_band_2_bw<=10),
  eq_band_2_slope float NOT NULL DEFAULT 1 CHECK(eq_band_2_slope>=0.1 AND eq_band_2_slope<=10),
  eq_band_2_type smallint NOT NULL DEFAULT 3 CHECK(eq_band_2_type>=0 AND eq_band_2_type<=7),
  eq_band_3_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_3_range>=0 AND eq_band_3_range<=18),
  eq_band_3_level float NOT NULL DEFAULT 0 CHECK(eq_band_3_level>=-eq_band_3_range AND eq_band_3_level<=eq_band_3_range),
  eq_band_3_freq smallint NOT NULL DEFAULT 300 CHECK(eq_band_3_freq>=10 AND eq_band_3_freq<=20000),
  eq_band_3_bw float NOT NULL DEFAULT 1 CHECK(eq_band_3_bw>=0.1 AND eq_band_3_bw<=10),
  eq_band_3_slope float NOT NULL DEFAULT 1 CHECK(eq_band_3_slope>=0.1 AND eq_band_3_slope<=10),
  eq_band_3_type smallint NOT NULL DEFAULT 2 CHECK(eq_band_3_type>=0 AND eq_band_3_type<=7),
  eq_band_4_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_4_range>=0 AND eq_band_4_range<=18),
  eq_band_4_level float NOT NULL DEFAULT 0 CHECK(eq_band_4_level>=-eq_band_4_range AND eq_band_4_level<=eq_band_4_range),
  eq_band_4_freq smallint NOT NULL DEFAULT 120 CHECK(eq_band_4_freq>=10 AND eq_band_4_freq<=20000),
  eq_band_4_bw float NOT NULL DEFAULT 1 CHECK(eq_band_4_bw>=0.1 AND eq_band_4_bw<=10),
  eq_band_4_slope float NOT NULL DEFAULT 1 CHECK(eq_band_4_slope>=0.1 AND eq_band_4_slope<=10),
  eq_band_4_type smallint NOT NULL DEFAULT 0 CHECK(eq_band_4_type>=0 AND eq_band_4_type<=7),
  eq_band_5_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_5_range>=0 AND eq_band_5_range<=18),
  eq_band_5_level float NOT NULL DEFAULT 0 CHECK(eq_band_5_level>=-eq_band_5_range AND eq_band_5_level<=eq_band_5_range),
  eq_band_5_freq smallint NOT NULL DEFAULT 12000 CHECK(eq_band_5_freq>=10 AND eq_band_5_freq<=20000),
  eq_band_5_bw float NOT NULL DEFAULT 1 CHECK(eq_band_5_bw>=0.1 AND eq_band_5_bw<=10),
  eq_band_5_slope float NOT NULL DEFAULT 1 CHECK(eq_band_5_slope>=0.1 AND eq_band_5_slope<=10),
  eq_band_5_type smallint NOT NULL DEFAULT 0 CHECK(eq_band_5_type>=0 AND eq_band_5_type<=7),
  eq_band_6_range smallint NOT NULL DEFAULT 18 CHECK(eq_band_6_range>=0 AND eq_band_6_range<=18),
  eq_band_6_level float NOT NULL DEFAULT 0 CHECK(eq_band_6_level>=-eq_band_6_range AND eq_band_6_level<=eq_band_6_range),
  eq_band_6_freq smallint NOT NULL DEFAULT 120 CHECK(eq_band_6_freq>=10 AND eq_band_6_freq<=20000),
  eq_band_6_bw float NOT NULL DEFAULT 1 CHECK(eq_band_6_bw>=0.1 AND eq_band_6_bw<=10),
  eq_band_6_slope float NOT NULL DEFAULT 1 CHECK(eq_band_6_slope>=0.1 AND eq_band_6_slope<=10),
  eq_band_6_type smallint NOT NULL DEFAULT 0 CHECK(eq_band_6_type>=0 AND eq_band_6_type<=7),
  use_dyn_preset boolean NOT NULL DEFAULT FALSE,
  dyn_on_off boolean NOT NULL DEFAULT FALSE,
  d_exp_threshold float NOT NULL DEFAULT -30 CHECK(d_exp_threshold>=-50 AND d_exp_threshold<=0),
  agc_amount smallint NOT NULL DEFAULT 0 CHECK(agc_amount>=0 AND agc_amount<=100),
  agc_threshold float NOT NULL DEFAULT -20 CHECK(agc_threshold>=-30 AND agc_threshold<=0),
  use_mod_preset boolean NOT NULL DEFAULT FALSE,
  mod_pan smallint NOT NULL DEFAULT 512 CHECK(mod_pan>=0 AND mod_pan<=1023),
  mod_on_off boolean NOT NULL DEFAULT FALSE,
  mod_lvl float NOT NULL DEFAULT -140 CHECK(mod_lvl>=-140 AND mod_lvl<=10),
);

CREATE TABLE routing_preset (
  mod_number smallint NOT NULL CHECK (mod_number>=1 AND mod_number<=128),
  mod_preset varchar(1) NOT NULL CHECK(mod_preset>='A' AND mod_preset<='H'),
  buss_1_2_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_1_2_level float NOT NULL DEFAULT 0 CHECK(buss_1_2_level>=-140 AND buss_1_2_level<=10),
  buss_1_2_on_off boolean NOT NULL DEFAULT TRUE,
  buss_1_2_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_1_2_balance smallint NOT NULL DEFAULT 512 CHECK(buss_1_2_balance>=0 AND buss_1_2_balance<=1023),
  buss_3_4_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_3_4_level float NOT NULL DEFAULT 0 CHECK(buss_3_4_level>=-140 AND buss_3_4_level<=10),
  buss_3_4_on_off boolean NOT NULL DEFAULT TRUE,
  buss_3_4_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_3_4_balance smallint NOT NULL DEFAULT 512 CHECK(buss_3_4_balance>=0 AND buss_3_4_balance<=1023),
  buss_5_6_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_5_6_level float NOT NULL DEFAULT 0 CHECK(buss_5_6_level>=-140 AND buss_5_6_level<=10),
  buss_5_6_on_off boolean NOT NULL DEFAULT TRUE,
  buss_5_6_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_5_6_balance smallint NOT NULL DEFAULT 512 CHECK(buss_5_6_balance>=0 AND buss_5_6_balance<=1023),
  buss_7_8_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_7_8_level float NOT NULL DEFAULT 0 CHECK(buss_7_8_level>=-140 AND buss_7_8_level<=10),
  buss_7_8_on_off boolean NOT NULL DEFAULT TRUE,
  buss_7_8_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_7_8_balance smallint NOT NULL DEFAULT 512 CHECK(buss_7_8_balance>=0 AND buss_7_8_balance<=1023),
  buss_9_10_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_9_10_level float NOT NULL DEFAULT 0 CHECK(buss_9_10_level>=-140 AND buss_9_10_level<=10),
  buss_9_10_on_off boolean NOT NULL DEFAULT TRUE,
  buss_9_10_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_9_10_balance smallint NOT NULL DEFAULT 512 CHECK(buss_9_10_balance>=0 AND buss_9_10_balance<=1023),
  buss_11_12_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_11_12_level float NOT NULL DEFAULT 0 CHECK(buss_11_12_level>=-140 AND buss_11_12_level<=10),
  buss_11_12_on_off boolean NOT NULL DEFAULT TRUE,
  buss_11_12_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_11_12_balance smallint NOT NULL DEFAULT 512 CHECK(buss_11_12_balance>=0 AND buss_11_12_balance<=1023),
  buss_13_14_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_13_14_level float NOT NULL DEFAULT 0 CHECK(buss_13_14_level>=-140 AND buss_13_14_level<=10),
  buss_13_14_on_off boolean NOT NULL DEFAULT TRUE,
  buss_13_14_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_13_14_balance smallint NOT NULL DEFAULT 512 CHECK(buss_13_14_balance>=0 AND buss_13_14_balance<=1023),
  buss_15_16_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_15_16_level float NOT NULL DEFAULT 0 CHECK(buss_15_16_level>=-140 AND buss_15_16_level<=10),
  buss_15_16_on_off boolean NOT NULL DEFAULT TRUE,
  buss_15_16_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_15_16_balance smallint NOT NULL DEFAULT 512 CHECK(buss_15_16_balance>=0 AND buss_15_16_balance<=1023),
  buss_17_18_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_17_18_level float NOT NULL DEFAULT 0 CHECK(buss_17_18_level>=-140 AND buss_17_18_level<=10),
  buss_17_18_on_off boolean NOT NULL DEFAULT TRUE,
  buss_17_18_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_17_18_balance smallint NOT NULL DEFAULT 512 CHECK(buss_17_18_balance>=0 AND buss_17_18_balance<=1023),
  buss_19_20_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_19_20_level float NOT NULL DEFAULT 0 CHECK(buss_19_20_level>=-140 AND buss_19_20_level<=10),
  buss_19_20_on_off boolean NOT NULL DEFAULT TRUE,
  buss_19_20_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_19_20_balance smallint NOT NULL DEFAULT 512 CHECK(buss_19_20_balance>=0 AND buss_19_20_balance<=1023),
  buss_21_22_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_21_22_level float NOT NULL DEFAULT 0 CHECK(buss_21_22_level>=-140 AND buss_21_22_level<=10),
  buss_21_22_on_off boolean NOT NULL DEFAULT TRUE,
  buss_21_22_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_21_22_balance smallint NOT NULL DEFAULT 512 CHECK(buss_21_22_balance>=0 AND buss_21_22_balance<=1023),
  buss_23_24_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_23_24_level float NOT NULL DEFAULT 0 CHECK(buss_23_24_level>=-140 AND buss_23_24_level<=10),
  buss_23_24_on_off boolean NOT NULL DEFAULT TRUE,
  buss_23_24_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_23_24_balance smallint NOT NULL DEFAULT 512 CHECK(buss_23_24_balance>=0 AND buss_23_24_balance<=1023),
  buss_25_26_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_25_26_level float NOT NULL DEFAULT 0 CHECK(buss_25_26_level>=-140 AND buss_25_26_level<=10),
  buss_25_26_on_off boolean NOT NULL DEFAULT TRUE,
  buss_25_26_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_25_26_balance smallint NOT NULL DEFAULT 512 CHECK(buss_25_26_balance>=0 AND buss_25_26_balance<=1023),
  buss_27_28_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_27_28_level float NOT NULL DEFAULT 0 CHECK(buss_27_28_level>=-140 AND buss_27_28_level<=10),
  buss_27_28_on_off boolean NOT NULL DEFAULT TRUE,
  buss_27_28_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_27_28_balance smallint NOT NULL DEFAULT 512 CHECK(buss_27_28_balance>=0 AND buss_27_28_balance<=1023),
  buss_29_30_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_29_30_level float NOT NULL DEFAULT 0 CHECK(buss_29_30_level>=-140 AND buss_29_30_level<=10),
  buss_29_30_on_off boolean NOT NULL DEFAULT TRUE,
  buss_29_30_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_29_30_balance smallint NOT NULL DEFAULT 512 CHECK(buss_29_30_balance>=0 AND buss_29_30_balance<=1023),
  buss_31_32_use_preset boolean NOT NULL DEFAULT FALSE,
  buss_31_32_level float NOT NULL DEFAULT 0 CHECK(buss_31_32_level>=-140 AND buss_31_32_level<=10),
  buss_31_32_on_off boolean NOT NULL DEFAULT TRUE,
  buss_31_32_pre_post boolean NOT NULL DEFAULT FALSE,
  buss_31_32_balance smallint NOT NULL DEFAULT 512 CHECK(buss_31_32_balance>=0 AND buss_31_32_balance<=1023),
  PRIMARY KEY(mod_number, mod_preset)
);

CREATE TABLE buss_preset (
  pos smallint NOT NULL DEFAULT 9999,
  number smallint NOT NULL CHECK (number>=1 AND number<=1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
);

nCREATE TABLE buss_preset_rows (
  number smallint NOT NULL CHECK(number>=1 AND number<=1280),
  buss smallint NOT NULL CHECK(buss>=1 AND buss<=16),
  use_preset boolean NOT NULL DEFAULT FALSE,
  level float NOT NULL DEFAULT 0.0,
  on_off boolean NOT NULL DEFAULT TRUE,
  PRIMARY KEY(number, buss)
);

CREATE TABLE monitor_buss_preset_rows (
  number smallint NOT NULL CHECK(number>=1 AND number<=1280),
  monitor_buss smallint NOT NULL CHECK(monitor_buss>=1 AND monitor_buss<=16),
  use_preset boolean[24] NOT NULL DEFAULT ARRAY[FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE],
  on_off boolean[24] NOT NULL DEFAULT ARRAY[FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE],
  PRIMARY KEY(number, monitor_buss)
)

CREATE TABLE console_preset (
  pos smallint NOT NULL DEFAULT 9999,
  number smallint NOT NULL CHECK(number>=1 AND number<=32) PRIMARY KEY,
  label varchar(32) NOT NULL DEFAULT 'Preset',
  console1 boolean NOT NULL DEFAULT FALSE,
  console2 boolean NOT NULL DEFAULT FALSE,
  console3 boolean NOT NULL DEFAULT FALSE,
  console4 boolean NOT NULL DEFAULT FALSE,
  mod_preset varchar(1) DEFAULT 'A' CHECK(mod_preset>='A' AND mod_preset<='H'),
  buss_preset smallint CHECK(buss_preset>=1 AND buss_preset<=1280),
  safe_recall_time float NOT NULL DEFAULT 1 CHECK(safe_recall_time>=0 AND safe_recall_time<=force_recall_time),
  forced_recall_time float NOT NULL DEFAULT 3 CHECK(forced_recall_time>=safe_recall_time AND forced_recall_time<=10)
);


-- F O R E I G N   K E Y S

ALTER TABLE node_config               ADD FOREIGN KEY (addr)               REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE defaults                  ADD FOREIGN KEY (addr)               REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE slot_config               ADD FOREIGN KEY (addr)               REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE src_config                ADD FOREIGN KEY (input1_addr)        REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE src_config                ADD FOREIGN KEY (input2_addr)        REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE dest_config               ADD FOREIGN KEY (output1_addr)       REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE dest_config               ADD FOREIGN KEY (output2_addr)       REFERENCES addresses (addr)      ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE module_config             ADD FOREIGN KEY (source_a_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_b_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_c_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_d_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_e_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_f_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_g_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE module_config             ADD FOREIGN KEY (source_h_preset)    REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE src_config                ADD FOREIGN KEY (default_src_preset) REFERENCES src_preset (number)   ON DELETE SET NULL;
ALTER TABLE buss_preset_rows          ADD FOREIGN KEY (number)             REFERENCES buss_preset (number)  ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE monitor_buss_preset_rows  ADD FOREIGN KEY (number)             REFERENCES buss_preset (number)  ON DELETE CASCADE ON UPDATE CASCADE;


