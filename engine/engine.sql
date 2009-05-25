-- create and load the tables for use with the axum software:
-- 
--  $ psql -U axum <axum_engine.sql

-- T A B L E S

CREATE TABLE rack_organization (
  slot_nr smallint NOT NULL CHECK(slot_nr>=1 AND slot_nr<42) PRIMARY KEY,
  addr integer NOT NULL,
  input_ch_cnt integer,
  output_ch_cnt integer
);

CREATE TABLE source_configuration (
  number smallint NOT NULL CHECK (number>=0 AND number<1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
  input1_addr integer NOT NULL,
  input1_sub_ch smallint NOT NULL CHECK(input1_sub_ch>=0 AND input1sub_ch<32),
  input2_addr integer NOT NULL,
  input2_sub_ch smallint NOT NULL CHECK(input2_sub_ch>=0 AND input2sub_ch<32),
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

CREATE TABLE module_configuration (
  number smallint NOT NULL CHECK(number>=0 AND number<128) PRIMARY KEY,
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

CREATE TABLE buss_configuration	(
  number smallint NOT NULL CHECK(number>=0 AND number<16) PRIMARY KEY,
  label varchar(32) NOT NULL,
  pre_on boolean NOT NULL,
  pre_level boolean NOT NULL,
  pre_balance boolean NOT NULL,
  level float NOT NULL,
  on_off boolean NOT NULL,
  interlock boolean NOT NULL,
  global_reset boolean NOT NULL
)

CREATE TABLE monitor_buss_configuration	(
  number smallint NOT NULL CHECK(number>=0 AND number<16) PRIMARY KEY,
  label varchar(32) NOT NULL,
  interlock boolean NOT NULL,
  default_selection smallint NOT NULL,
  buss_1_2 boolean NOT NULL,
  buss_3_4 boolean NOT NULL,
  buss_5_6 boolean NOT NULL,
  buss_7_8 boolean NOT NULL,
  buss_9_10 boolean NOT NULL,
  buss_11_12 boolean NOT NULL,
  buss_13_14 boolean NOT NULL,
  buss_15_16 boolean NOT NULL,
  buss_17_18 boolean NOT NULL,
  buss_19_20 boolean NOT NULL,
  buss_21_22 boolean NOT NULL,
  buss_23_24 boolean NOT NULL,
  buss_25_26 boolean NOT NULL,
  buss_27_28 boolean NOT NULL,
  buss_29_30 boolean NOT NULL,
  buss_31_32 boolean NOT NULL,
	dim_level float NOT NULL
);

CREATE TABLE extern_source_configuration (
  number smallint NOT NULL CHECK(number>=0 AND number<4) PRIMARY KEY,
  ext1 smallint NOT NULL,
  ext2 smallint NOT NULL,
  ext3 smallint NOT NULL,
  ext4 smallint NOT NULL,
  ext5 smallint NOT NULL,
  ext6 smallint NOT NULL,
  ext7 smallint NOT NULL,
  ext8 smallint NOT NULL,
);

CREATE TABLE talkback_configuration (
  number smallint NOT NULL CHECK(number>=0 AND number<16) PRIMARY KEY,
  source smallint NOT NULL
);

CREATE TABLE global_configuration (
  samplerate integer NOT NULL,
  ext_clock boolean NOT NULL,
  headroom float NOT NULL,
  level_reserve float NOT NULL
);

CREATE TABLE destination_configuration (
  number smallint NOT NULL CHECK(number>=0 AND number<1280) PRIMARY KEY,
  label varchar(32) NOT NULL,
  output1_addr integer NOT NULL,
  output1_sub_ch smallint NOT NULL CHECK(input1_sub_ch>=0 AND input1sub_ch<32),
  output2_addr integer NOT NULL,
  output2_sub_ch smallint NOT NULL CHECK(input2_sub_ch>=0 AND input2sub_ch<32),
  level float NOT NULL,
  source integer NOT NULL,
  mix_minus_source integer NOT NULL 
);

-- F O R E I G N   K E Y S

ALTER TABLE rack_organization ADD FOREIGN KEY (addr) REFERENCES addresses (addr);
ALTER TABLE source_configuration ADD FOREIGN KEY (input1_addr) REFERENCES addresses (addr);
ALTER TABLE source_configuration ADD FOREIGN KEY (input2_addr) REFERENCES addresses (addr);
ALTER TABLE module_configuration ADD FOREIGN KEY (source_a) REFERENCES source_configuration (number);
ALTER TABLE module_configuration ADD FOREIGN KEY (source_b) REFERENCES source_configuration (number);
ALTER TABLE module_configuration ADD FOREIGN KEY (insert_source) REFERENCES source_configuration (number);
ALTER TABLE destination_configuration ADD FOREIGN KEY (output1_addr) REFERENCES addresses (addr);
ALTER TABLE destination_configuration ADD FOREIGN KEY (output2_addr) REFERENCES addresses (addr);
ALTER TABLE destination_configuration ADD FOREIGN KEY (mix_minus_source) REFERENCES addresses (addr);

-- T R I G G E R S

CREATE TRIGGER rack_organization_notify AFTER UPDATE ON rack_organization FOR EACH ROW EXECUTE PROCEDURE rack_organization_changed();
CREATE TRIGGER source_configuration_notify AFTER UPDATE ON source_configuration FOR EACH ROW EXECUTE PROCEDURE source_configuration_changed();
CREATE TRIGGER module_configuration_notify AFTER UPDATE ON module_configuration FOR EACH ROW EXECUTE PROCEDURE module_configuration_changed();
CREATE TRIGGER buss_configuration_notify AFTER UPDATE ON buss_configuration FOR EACH ROW EXECUTE PROCEDURE buss_configuration_changed();
CREATE TRIGGER monitor_buss_configuration_notify AFTER UPDATE ON monitor_buss_configuration FOR EACH ROW EXECUTE PROCEDURE monitor_buss_configuration_changed();
CREATE TRIGGER extern_source_configuration_notify AFTER UPDATE ON extern_source_configuration FOR EACH ROW EXECUTE PROCEDURE extern_source_configuration_changed();
CREATE TRIGGER talkback_configuration_notify AFTER UPDATE ON talkback_configuration FOR EACH ROW EXECUTE PROCEDURE talkback_configuration_changed();
CREATE TRIGGER global_configuration_notify AFTER UPDATE ON global_configuration FOR EACH ROW EXECUTE PROCEDURE global_configuration_changed();
CREATE TRIGGER destination_configuration_notify AFTER UPDATE ON destination_configuration FOR EACH ROW EXECUTE PROCEDURE destination_configuration_changed();

-- P R O C E D U R E S

CREATE OR REPLACE FUNCTION rack_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('rack_organization_changed', NEW.slot_nr::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION source_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('source_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION module_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('module_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION buss_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('buss_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION monitor_buss_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('monitor_buss_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION extern_source_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('extern_source_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION extern_source_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('extern_source_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION talkback_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('talkback_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION global_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('global_organization_changed', '');
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION destination_organization_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('destination_organization_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;
