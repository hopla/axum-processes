-- On an empty PostgreSQL installation, execute the following to
-- create and load the DB for use with the axum software:
--
--  $ createuser -U postgres -S -D -R axum
--  $ createdb -U postgres axum -O axum
--  $ psql -U axum <schema.sql
--  $ psql -U axum <db_to_position.sql
--  $ psql -U axum <defaults.sql
--  $ psql -U axum <triggers.sql


-- General TODO list (not too important)
--  - Operator classes for the custom types
--  - More sanity checking (NULL values on custom types)
--  - Foreign key: configuration(func) -> functions(func)
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
  func function_number NOT NULL,
  PRIMARY KEY(addr, object)
);

CREATE TABLE defaults (
  addr integer NOT NULL,
  object integer NOT NULL,
  data mambanet_data NOT NULL,
  PRIMARY KEY(addr, object)
);

CREATE TABLE templates (
  man_id smallint NOT NULL,
  prod_id smallint NOT NULL,
  firm_major smallint NOT NULL,
  "number" integer NOT NULL,
  "desc" varchar(32) NOT NULL,
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
  xmt_type smallint NOT NULL CHECK(xmt_type >= 0 AND xmt_type <= 6)
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
  addr_requests integer NOT NULL DEFAULT 0
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
  number smallint NOT NULL CHECK (number>=1 AND number<=1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
  input1_addr integer NOT NULL,
  input1_sub_ch smallint NOT NULL CHECK(input1_sub_ch>=1 AND input1_sub_ch<=32),
  input2_addr integer NOT NULL,
  input2_sub_ch smallint NOT NULL CHECK(input2_sub_ch>=1 AND input2_sub_ch<=32),
  phantom boolean NOT NULL,
  pad boolean NOT NULL,
  Gain float NOT NULL,
  Redlight1 boolean NOT NULL,
  Redlight2 boolean NOT NULL,
  Redlight3 boolean NOT NULL,
  Redlight4 boolean NOT NULL,
  Redlight5 boolean NOT NULL,
  Redlight6 boolean NOT NULL,
  Redlight7 boolean NOT NULL,
  Redlight8 boolean NOT NULL,
  MonitorMute1 boolean NOT NULL,
  MonitorMute2 boolean NOT NULL,
  MonitorMute3 boolean NOT NULL,
  MonitorMute4 boolean NOT NULL,
  MonitorMute5 boolean NOT NULL,
  MonitorMute6 boolean NOT NULL,
  MonitorMute7 boolean NOT NULL,
  MonitorMute8 boolean NOT NULL,
  MonitorMute9 boolean NOT NULL,
  MonitorMute10 boolean NOT NULL,
  MonitorMute11 boolean NOT NULL,
  MonitorMute12 boolean NOT NULL,
  MonitorMute13 boolean NOT NULL,
  MonitorMute14 boolean NOT NULL,
  MonitorMute15 boolean NOT NULL,
  MonitorMute16 boolean NOT NULL
);

CREATE TABLE module_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=128) PRIMARY KEY,
  source_a smallint NOT NULL,
  source_b smallint NOT NULL,
  insert_source smallint NOT NULL,
  insert_on_off_a boolean NOT NULL,
  insert_on_off_b boolean NOT NULL,
  gain float NOT NULL,
  lc_frequency smallint NOT NULL CHECK(lc_frequency>=40 AND lc_frequency<=12000),
  lc_on_off_a boolean NOT NULL,
  lc_on_off_b boolean NOT NULL,
  eq_band_1_range smallint NOT NULL CHECK(eq_band_1_range>=-18 AND eq_band_1_range<=18),
  eq_band_1_freq smallint NOT NULL CHECK(eq_band_1_freq>=10 AND eq_band_1_freq<=20000),
  eq_band_1_bw float NOT NULL CHECK(eq_band_1_bw>=0.1 AND eq_band_1_bw<=10),
  eq_band_1_slope float NOT NULL CHECK(eq_band_1_slope>=0.1 AND eq_band_1_slope<=10),
  eq_band_1_type smallint NOT NULL CHECK(eq_band_1_type>=0 AND eq_band_1_type<=7),
  eq_band_2_range smallint NOT NULL CHECK(eq_band_2_range>=-18 AND eq_band_2_range<=18),
  eq_band_2_freq smallint NOT NULL CHECK(eq_band_2_freq>=10 AND eq_band_2_freq<=20000),
  eq_band_2_bw float NOT NULL CHECK(eq_band_2_bw>=0.1 AND eq_band_2_bw<=10),
  eq_band_2_slope float NOT NULL CHECK(eq_band_2_slope>=0.1 AND eq_band_2_slope<=10),
  eq_band_2_type smallint NOT NULL CHECK(eq_band_2_type>=0 AND eq_band_2_type<=7),
  eq_band_3_range smallint NOT NULL CHECK(eq_band_3_range>=-18 AND eq_band_3_range<=18),
  eq_band_3_freq smallint NOT NULL CHECK(eq_band_3_freq>=10 AND eq_band_3_freq<=20000),
  eq_band_3_bw float NOT NULL CHECK(eq_band_3_bw>=0.1 AND eq_band_3_bw<=10),
  eq_band_3_slope float NOT NULL CHECK(eq_band_3_slope>=0.1 AND eq_band_3_slope<=10),
  eq_band_3_type smallint NOT NULL CHECK(eq_band_3_type>=0 AND eq_band_3_type<=7),
  eq_band_4_range smallint NOT NULL CHECK(eq_band_4_range>=-18 AND eq_band_4_range<=18),
  eq_band_4_freq smallint NOT NULL CHECK(eq_band_4_freq>=10 AND eq_band_4_freq<=20000),
  eq_band_4_bw float NOT NULL CHECK(eq_band_4_bw>=0.1 AND eq_band_4_bw<=10),
  eq_band_4_slope float NOT NULL CHECK(eq_band_4_slope>=0.1 AND eq_band_4_slope<=10),
  eq_band_4_type smallint NOT NULL CHECK(eq_band_4_type>=0 AND eq_band_4_type<=7),
  eq_band_5_range smallint NOT NULL CHECK(eq_band_5_range>=-18 AND eq_band_5_range<=18),
  eq_band_5_freq smallint NOT NULL CHECK(eq_band_5_freq>=10 AND eq_band_5_freq<=20000),
  eq_band_5_bw float NOT NULL CHECK(eq_band_5_bw>=0.1 AND eq_band_5_bw<=10),
  eq_band_5_slope float NOT NULL CHECK(eq_band_5_slope>=0.1 AND eq_band_5_slope<=10),
  eq_band_5_type smallint NOT NULL CHECK(eq_band_5_type>=0 AND eq_band_5_type<=7),
  eq_band_6_range smallint NOT NULL CHECK(eq_band_6_range>=-18 AND eq_band_6_range<=18),
  eq_band_6_freq smallint NOT NULL CHECK(eq_band_6_freq>=10 AND eq_band_6_freq<=20000),
  eq_band_6_bw float NOT NULL CHECK(eq_band_6_bw>=0.1 AND eq_band_6_bw<=10),
  eq_band_6_slope float NOT NULL CHECK(eq_band_6_slope>=0.1 AND eq_band_6_slope<=10),
  eq_band_6_type smallint NOT NULL CHECK(eq_band_6_type>=0 AND eq_band_6_type<=7),
  eq_on_off_a boolean NOT NULL,
  eq_on_off_b boolean NOT NULL,
  dyn_amount smallint NOT NULL,
  dyn_on_off_a boolean NOT NULL,
  dyn_on_off_b boolean NOT NULL,
  mod_level float NOT NULL,
  mod_on_off boolean NOT NULL,
  buss_1_2_level float NOT NULL,
  buss_1_2_on_off boolean NOT NULL,
  buss_1_2_pre_post boolean NOT NULL,
  buss_1_2_balance smallint NOT NULL CHECK(buss_1_2_balance>=0 AND buss_1_2_balance<1024),
  buss_1_2_assignment boolean NOT NULL,
  buss_3_4_level float NOT NULL,
  buss_3_4_on_off boolean NOT NULL,
  buss_3_4_pre_post boolean NOT NULL,
  buss_3_4_balance smallint NOT NULL CHECK(buss_3_4_balance>=0 AND buss_3_4_balance<1024),
  buss_3_4_assignment boolean NOT NULL,
  buss_5_6_level float NOT NULL,
  buss_5_6_on_off boolean NOT NULL,
  buss_5_6_pre_post boolean NOT NULL,
  buss_5_6_balance smallint NOT NULL CHECK(buss_5_6_balance>=0 AND buss_5_6_balance<1024),
  buss_5_6_assignment boolean NOT NULL,
  buss_7_8_level float NOT NULL,
  buss_7_8_on_off boolean NOT NULL,
  buss_7_8_pre_post boolean NOT NULL,
  buss_7_8_balance smallint NOT NULL CHECK(buss_7_8_balance>=0 AND buss_7_8_balance<1024),
  buss_7_8_assignment boolean NOT NULL,
  buss_9_10_level float NOT NULL,
  buss_9_10_on_off boolean NOT NULL,
  buss_9_10_pre_post boolean NOT NULL,
  buss_9_10_balance smallint NOT NULL CHECK(buss_9_10_balance>=0 AND buss_9_10_balance<1024),
  buss_9_10_assignment boolean NOT NULL,
  buss_11_12_level float NOT NULL,
  buss_11_12_on_off boolean NOT NULL,
  buss_11_12_pre_post boolean NOT NULL,
  buss_11_12_balance smallint NOT NULL CHECK(buss_11_12_balance>=0 AND buss_11_12_balance<1024),
  buss_11_12_assignment boolean NOT NULL,
  buss_13_14_level float NOT NULL,
  buss_13_14_on_off boolean NOT NULL,
  buss_13_14_pre_post boolean NOT NULL,
  buss_13_14_balance smallint NOT NULL CHECK(buss_13_14_balance>=0 AND buss_13_14_balance<1024),
  buss_13_14_assignment boolean NOT NULL,
  buss_15_16_level float NOT NULL,
  buss_15_16_on_off boolean NOT NULL,
  buss_15_16_pre_post boolean NOT NULL,
  buss_15_16_balance smallint NOT NULL CHECK(buss_15_16_balance>=0 AND buss_15_16_balance<1024),
  buss_15_16_assignment boolean NOT NULL,
  buss_17_18_level float NOT NULL,
  buss_17_18_on_off boolean NOT NULL,
  buss_17_18_pre_post boolean NOT NULL,
  buss_17_18_balance smallint NOT NULL CHECK(buss_17_18_balance>=0 AND buss_17_18_balance<1024),
  buss_17_18_assignment boolean NOT NULL,
  buss_19_20_level float NOT NULL,
  buss_19_20_on_off boolean NOT NULL,
  buss_19_20_pre_post boolean NOT NULL,
  buss_19_20_balance smallint NOT NULL CHECK(buss_19_20_balance>=0 AND buss_19_20_balance<1024),
  buss_19_20_assignment boolean NOT NULL,
  buss_21_22_level float NOT NULL,
  buss_21_22_on_off boolean NOT NULL,
  buss_21_22_pre_post boolean NOT NULL,
  buss_21_22_balance smallint NOT NULL CHECK(buss_21_22_balance>=0 AND buss_21_22_balance<1024),
  buss_21_22_assignment boolean NOT NULL,
  buss_23_24_level float NOT NULL,
  buss_23_24_on_off boolean NOT NULL,
  buss_23_24_pre_post boolean NOT NULL,
  buss_23_24_balance smallint NOT NULL CHECK(buss_23_24_balance>=0 AND buss_23_24_balance<1024),
  buss_23_24_assignment boolean NOT NULL,
  buss_25_26_level float NOT NULL,
  buss_25_26_on_off boolean NOT NULL,
  buss_25_26_pre_post boolean NOT NULL,
  buss_25_26_balance smallint NOT NULL CHECK(buss_25_26_balance>=0 AND buss_25_26_balance<1024),
  buss_25_26_assignment boolean NOT NULL,
  buss_27_28_level float NOT NULL,
  buss_27_28_on_off boolean NOT NULL,
  buss_27_28_pre_post boolean NOT NULL,
  buss_27_28_balance smallint NOT NULL CHECK(buss_27_28_balance>=0 AND buss_27_28_balance<1024),
  buss_27_28_assignment boolean NOT NULL,
  buss_29_30_level float NOT NULL,
  buss_29_30_on_off boolean NOT NULL,
  buss_29_30_pre_post boolean NOT NULL,
  buss_29_30_balance smallint NOT NULL CHECK(buss_29_30_balance>=0 AND buss_29_30_balance<1024),
  buss_29_30_assignment boolean NOT NULL,
  buss_31_32_level float NOT NULL,
  buss_31_32_on_off boolean NOT NULL,
  buss_31_32_pre_post boolean NOT NULL,
  buss_31_32_balance smallint NOT NULL CHECK(buss_31_32_balance>=0 AND buss_31_32_balance<1024),
  buss_31_32_assignment boolean NOT NULL
);

CREATE TABLE buss_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=16) PRIMARY KEY,
  label varchar(32) NOT NULL,
  pre_on boolean NOT NULL DEFAULT FALSE,
  pre_level boolean NOT NULL DEFAULT FALSE,
  pre_balance boolean NOT NULL DEFAULT FALSE,
  level float NOT NULL DEFAULT 0.0,
  on_off boolean NOT NULL DEFAULT TRUE,
  interlock boolean NOT NULL DEFAULT FALSE,
  global_reset boolean NOT NULL DEFAULT FALSE
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
  dim_level float NOT NULL DEFAULT -20.0
);

CREATE TABLE extern_src_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=4) PRIMARY KEY,
  ext1 smallint NOT NULL,
  ext2 smallint NOT NULL,
  ext3 smallint NOT NULL,
  ext4 smallint NOT NULL,
  ext5 smallint NOT NULL,
  ext6 smallint NOT NULL,
  ext7 smallint NOT NULL,
  ext8 smallint NOT NULL
);

CREATE TABLE talkback_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=16) PRIMARY KEY,
  source smallint NOT NULL
);

CREATE TABLE global_config (
  samplerate integer NOT NULL,
  ext_clock boolean NOT NULL,
  headroom float NOT NULL,
  level_reserve float NOT NULL
);

CREATE TABLE dest_config (
  number smallint NOT NULL CHECK(number>=1 AND number<=1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
  output1_addr integer NOT NULL,
  output1_sub_ch smallint NOT NULL CHECK(output1_sub_ch>=0 AND output1_sub_ch<32),
  output2_addr integer NOT NULL,
  output2_sub_ch smallint NOT NULL CHECK(output2_sub_ch>=0 AND output2_sub_ch<32),
  level float NOT NULL,
  source integer NOT NULL,
  mix_minus_source integer NOT NULL
);

CREATE TABLE db_to_position (
  db float PRIMARY KEY CHECK(db >= -140.0 AND db < 10.0),
  position smallint CHECK(position >= 0 AND position < 1024)
);





-- F O R E I G N   K E Y S

ALTER TABLE configuration ADD FOREIGN KEY (addr) REFERENCES addresses (addr);
ALTER TABLE defaults      ADD FOREIGN KEY (addr) REFERENCES addresses (addr);
ALTER TABLE slot_config   ADD FOREIGN KEY (addr) REFERENCES addresses (addr);
ALTER TABLE src_config    ADD FOREIGN KEY (input1_addr) REFERENCES addresses (addr);
ALTER TABLE src_config    ADD FOREIGN KEY (input2_addr) REFERENCES addresses (addr);
ALTER TABLE module_config ADD FOREIGN KEY (source_a) REFERENCES src_config (number);
ALTER TABLE module_config ADD FOREIGN KEY (source_b) REFERENCES src_config (number);
ALTER TABLE module_config ADD FOREIGN KEY (insert_source) REFERENCES src_config (number);
ALTER TABLE dest_config   ADD FOREIGN KEY (output1_addr) REFERENCES addresses (addr);
ALTER TABLE dest_config   ADD FOREIGN KEY (output2_addr) REFERENCES addresses (addr);
ALTER TABLE dest_config   ADD FOREIGN KEY (mix_minus_source) REFERENCES addresses (addr);


