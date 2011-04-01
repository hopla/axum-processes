
-- P R O C E D U R E S

CREATE OR REPLACE FUNCTION notify_changes() RETURNS trigger AS $$
BEGIN
  -- remove changes older than an hour
  DELETE FROM recent_changes WHERE timestamp < (NOW() - INTERVAL '1 hour');
  -- send notify
  NOTIFY change;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION before_addresses_change() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'UPDATE' THEN
    IF (OLD.id).man <> (NEW.id).man OR (OLD.id).prod <> (NEW.id).prod THEN
      RAISE EXCEPTION 'Changing (addresses.id).man/.prod is not allowed';
    END IF;
    IF (OLD.id).id <> (NEW.id).id AND OLD.firm_major = NEW.firm_major THEN
      DELETE FROM addresses WHERE (id).man = (NEW.id).man AND (id).prod = (NEW.id).prod AND (id).id = (NEW.id).id AND firm_major = NEW.firm_major;
      INSERT INTO recent_changes (change, arguments) VALUES ('unique_id_changed', OLD.addr::text);
    END IF;
  END IF;
  RETURN NEW;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION addresses_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES ('address_removed', OLD.addr::text);
  ELSIF TG_OP = 'UPDATE' THEN
    IF OLD.engine_addr <> NEW.engine_addr THEN
      INSERT INTO recent_changes (change, arguments) VALUES ('address_set_engine', NEW.addr::text);
    END IF;
    IF OLD.addr <> NEW.addr THEN
      INSERT INTO recent_changes (change, arguments) VALUES ('address_set_addr', OLD.addr::text||' '||NEW.addr::text);
    END IF;
    IF OLD.name <> NEW.name THEN
      UPDATE addresses SET setname = TRUE WHERE addr = NEW.addr;
    END IF;
    IF OLD.setname = FALSE AND NEW.setname = TRUE THEN
      INSERT INTO recent_changes (change, arguments) VALUES ('address_set_name', NEW.addr::text);
    END IF;
    IF OLD.refresh = FALSE AND NEW.refresh = TRUE THEN
      INSERT INTO recent_changes (change, arguments) VALUES ('address_refresh', NEW.addr::text);
    END IF;
    IF OLD.user_level_from_console <> NEW.user_level_from_console THEN
      INSERT INTO recent_changes (change, arguments) VALUES('address_user_level', NEW.addr::text);
    END IF;
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;



CREATE OR REPLACE FUNCTION templates_changed() RETURNS trigger AS $$
DECLARE
  arg text;
BEGIN
  arg := OLD.man_id || ' ' || OLD.prod_id || ' ' || OLD.firm_major;
  -- the argument of the templates_removed change don't include the object number,
  -- so if we delete multiple rows from the same (man_id,prod_id,firm_id), make
  -- sure to only insert one row into the recent_changes table
  PERFORM 1 FROM recent_changes WHERE change = 'template_removed' AND arguments = arg AND timestamp = NOW();
  IF NOT FOUND THEN
    INSERT INTO recent_changes (change, arguments) VALUES('template_removed', arg);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION defaults_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('defaults_changed', OLD.addr::text||' '||OLD.object::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('defaults_changed', NEW.addr::text||' '||NEW.object::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION node_config_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('node_config_changed', OLD.addr::text||' '||OLD.object::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('node_config_changed', NEW.addr::text||' '||NEW.object::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION slot_config_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('slot_config_changed', OLD.slot_nr::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('slot_config_changed', NEW.slot_nr::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION src_config_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    UPDATE module_config SET source_a = 0 WHERE source_a = OLD.number+288;
    UPDATE module_config SET source_b = 0 WHERE source_b = OLD.number+288;
    UPDATE module_config SET source_c = 0 WHERE source_c = OLD.number+288;
    UPDATE module_config SET source_d = 0 WHERE source_d = OLD.number+288;
    UPDATE module_config SET source_e = 0 WHERE source_e = OLD.number+288;
    UPDATE module_config SET source_f = 0 WHERE source_f = OLD.number+288;
    UPDATE module_config SET source_g = 0 WHERE source_g = OLD.number+288;
    UPDATE module_config SET source_h = 0 WHERE source_h = OLD.number+288;
    UPDATE module_config SET insert_source = 0 WHERE insert_source = OLD.number+288;
    UPDATE dest_config SET source = 0 WHERE source = OLD.number+288;
    UPDATE dest_config SET mix_minus_source = 0 WHERE mix_minus_source = OLD.number+288;
    UPDATE extern_src_config SET ext1 = 0 WHERE ext1 = OLD.number+288;
    UPDATE extern_src_config SET ext2 = 0 WHERE ext2 = OLD.number+288;
    UPDATE extern_src_config SET ext3 = 0 WHERE ext3 = OLD.number+288;
    UPDATE extern_src_config SET ext4 = 0 WHERE ext4 = OLD.number+288;
    UPDATE extern_src_config SET ext5 = 0 WHERE ext5 = OLD.number+288;
    UPDATE extern_src_config SET ext6 = 0 WHERE ext6 = OLD.number+288;
    UPDATE extern_src_config SET ext7 = 0 WHERE ext7 = OLD.number+288;
    UPDATE extern_src_config SET ext8 = 0 WHERE ext8 = OLD.number+288;
    UPDATE monitor_buss_config SET default_selection = 0 WHERE default_selection = OLD.number+288;
    UPDATE talkback_config SET source = 0 WHERE source = OLD.number+288;
    INSERT INTO recent_changes (change, arguments) VALUES('src_config_changed', OLD.number::text);
  ELSIF TG_OP = 'UPDATE' THEN
    IF (OLD.number <> NEW.number OR
       OLD.label <> NEW.label OR
       OLD.input1_addr <> NEW.input1_addr OR
       OLD.input1_sub_ch <> NEW.input1_sub_ch OR
       OLD.input2_addr <> NEW.input2_addr OR
       OLD.input2_sub_ch <> NEW.input2_sub_ch OR
       OLD.input_phantom <> NEW.input_phantom OR
       OLD.input_pad <> NEW.input_pad OR
       OLD.input_gain <> NEW.input_gain OR
       OLD.redlight1 <> NEW.redlight1 OR
       OLD.redlight2 <> NEW.redlight2 OR
       OLD.redlight3 <> NEW.redlight3 OR
       OLD.redlight4 <> NEW.redlight4 OR
       OLD.redlight5 <> NEW.redlight5 OR
       OLD.redlight6 <> NEW.redlight6 OR
       OLD.redlight7 <> NEW.redlight7 OR
       OLD.redlight8 <> NEW.redlight8 OR
       OLD.monitormute1 <> NEW.monitormute1 OR
       OLD.monitormute2 <> NEW.monitormute2 OR
       OLD.monitormute3 <> NEW.monitormute3 OR
       OLD.monitormute4 <> NEW.monitormute4 OR
       OLD.monitormute5 <> NEW.monitormute5 OR
       OLD.monitormute6 <> NEW.monitormute6 OR
       OLD.monitormute7 <> NEW.monitormute7 OR
       OLD.monitormute8 <> NEW.monitormute8 OR
       OLD.monitormute9 <> NEW.monitormute9 OR
       OLD.monitormute10 <> NEW.monitormute10 OR
       OLD.monitormute11 <> NEW.monitormute11 OR
       OLD.monitormute12 <> NEW.monitormute12 OR
       OLD.monitormute13 <> NEW.monitormute13 OR
       OLD.monitormute14 <> NEW.monitormute14 OR
       OLD.monitormute15 <> NEW.monitormute15 OR
       OLD.monitormute16 <> NEW.monitormute16 OR
       OLD.default_src_preset <> NEW.default_src_preset OR
       OLD.start_trigger <> NEW.start_trigger OR
       OLD.stop_trigger <> NEW.stop_trigger OR
       OLD.pool1 <> NEW.pool1 OR
       OLD.pool2 <> NEW.pool2 OR
       OLD.pool3 <> NEW.pool3 OR
       OLD.pool4 <> NEW.pool4 OR
       OLD.pool5 <> NEW.pool5 OR
       OLD.pool6 <> NEW.pool6 OR
       OLD.pool7 <> NEW.pool7 OR
       OLD.pool8 <> NEW.pool8 OR
       OLD.related_dest <> NEW.related_dest)
    THEN
      INSERT INTO recent_changes (change, arguments) VALUES('src_config_changed', NEW.number::text);
    END IF;
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('src_config_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION module_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('module_config_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION buss_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('buss_config_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION monitor_buss_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('monitor_buss_config_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION extern_src_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('extern_src_config_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION talkback_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('talkback_config_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION global_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('global_config_changed', '');
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION dest_config_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('dest_config_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('dest_config_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION src_preset_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    UPDATE module_config SET source_a_preset = NULL WHERE source_a_preset = OLD.number;
    UPDATE module_config SET source_b_preset = NULL WHERE source_b_preset = OLD.number;
    UPDATE module_config SET source_c_preset = NULL WHERE source_c_preset = OLD.number;
    UPDATE module_config SET source_d_preset = NULL WHERE source_d_preset = OLD.number;
    UPDATE module_config SET source_e_preset = NULL WHERE source_e_preset = OLD.number;
    UPDATE module_config SET source_f_preset = NULL WHERE source_f_preset = OLD.number;
    UPDATE module_config SET source_g_preset = NULL WHERE source_g_preset = OLD.number;
    UPDATE module_config SET source_h_preset = NULL WHERE source_h_preset = OLD.number;
    UPDATE src_config SET default_src_preset = NULL WHERE default_src_preset = OLD.number;
    INSERT INTO recent_changes (change, arguments) VALUES('src_preset_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('src_preset_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION routing_preset_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('routing_preset_changed', OLD.mod_number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('routing_preset_changed', NEW.mod_number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION buss_preset_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('buss_preset_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('buss_preset_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION buss_preset_rows_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('buss_preset_rows_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('buss_preset_rows_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION monitor_buss_preset_rows_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('monitor_buss_preset_rows_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('monitor_buss_preset_rows_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION console_preset_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('console_preset_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('console_preset_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION console_config_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('console_config_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('console_config_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION functions_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('functions_changed', OLD.func::text);
  ELSIF TG_OP = 'UPDATE' THEN
    IF OLD.user_level0 <> NEW.user_level0 OR
       OLD.user_level1 <> NEW.user_level1 OR
       OLD.user_level2 <> NEW.user_level2 OR
       OLD.user_level3 <> NEW.user_level3 OR
       OLD.user_level4 <> NEW.user_level4 OR
       OLD.user_level5 <> NEW.user_level5 THEN
      INSERT INTO recent_changes (change, arguments) VALUES('functions_changed', NEW.func::text);
    END IF;
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('functions_changed', NEW.func::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION preset_pool_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('preset_pool_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('preset_pool_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION src_pool_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('src_pool_changed', OLD.number::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('src_pool_changed', NEW.number::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


-- T R I G G E R S

CREATE TRIGGER recent_changes_notify            AFTER INSERT ON recent_changes                                FOR EACH STATEMENT EXECUTE PROCEDURE notify_changes();
CREATE TRIGGER template_change_notify           AFTER DELETE ON templates                                     FOR EACH ROW EXECUTE PROCEDURE templates_changed();
CREATE TRIGGER before_addresses_change_notify   BEFORE UPDATE ON addresses                                    FOR EACH ROW EXECUTE PROCEDURE before_addresses_change();
CREATE TRIGGER addresses_change_notify          AFTER DELETE OR UPDATE ON addresses                           FOR EACH ROW EXECUTE PROCEDURE addresses_changed();
CREATE TRIGGER defaults_change_notify           AFTER INSERT OR DELETE OR UPDATE ON defaults                  FOR EACH ROW EXECUTE PROCEDURE defaults_changed();
CREATE TRIGGER node_config_change_notify        AFTER INSERT OR DELETE OR UPDATE ON node_config               FOR EACH ROW EXECUTE PROCEDURE node_config_changed();
CREATE TRIGGER slot_config_notify               AFTER INSERT OR DELETE OR UPDATE ON slot_config               FOR EACH ROW EXECUTE PROCEDURE slot_config_changed();
CREATE TRIGGER src_config_notify                AFTER INSERT OR DELETE OR UPDATE ON src_config                FOR EACH ROW EXECUTE PROCEDURE src_config_changed();
CREATE TRIGGER module_config_notify             AFTER UPDATE ON module_config                                 FOR EACH ROW EXECUTE PROCEDURE module_config_changed();
CREATE TRIGGER buss_config_notify               AFTER UPDATE ON buss_config                                   FOR EACH ROW EXECUTE PROCEDURE buss_config_changed();
CREATE TRIGGER monitor_buss_config_notify       AFTER UPDATE ON monitor_buss_config                           FOR EACH ROW EXECUTE PROCEDURE monitor_buss_config_changed();
CREATE TRIGGER extern_src_config_notify         AFTER UPDATE ON extern_src_config                             FOR EACH ROW EXECUTE PROCEDURE extern_src_config_changed();
CREATE TRIGGER talkback_config_notify           AFTER UPDATE ON talkback_config                               FOR EACH ROW EXECUTE PROCEDURE talkback_config_changed();
CREATE TRIGGER global_config_notify             AFTER UPDATE ON global_config                                 FOR EACH ROW EXECUTE PROCEDURE global_config_changed();
CREATE TRIGGER dest_config_notify               AFTER INSERT OR DELETE OR UPDATE ON dest_config               FOR EACH ROW EXECUTE PROCEDURE dest_config_changed();
CREATE TRIGGER src_preset_notify                AFTER INSERT OR DELETE OR UPDATE ON src_preset                FOR EACH ROW EXECUTE PROCEDURE src_preset_changed();
CREATE TRIGGER routing_preset_notify            AFTER INSERT OR DELETE OR UPDATE ON routing_preset            FOR EACH ROW EXECUTE PROCEDURE routing_preset_changed();
CREATE TRIGGER buss_preset_notify               AFTER INSERT OR DELETE OR UPDATE ON buss_preset               FOR EACH ROW EXECUTE PROCEDURE buss_preset_changed();
CREATE TRIGGER buss_preset_rows_notify          AFTER INSERT OR DELETE OR UPDATE ON buss_preset_rows          FOR EACH ROW EXECUTE PROCEDURE buss_preset_rows_changed();
CREATE TRIGGER monitor_buss_preset_rows_notify  AFTER INSERT OR DELETE OR UPDATE ON monitor_buss_preset_rows  FOR EACH ROW EXECUTE PROCEDURE monitor_buss_preset_rows_changed();
CREATE TRIGGER console_preset_notify            AFTER INSERT OR DELETE OR UPDATE ON console_preset            FOR EACH ROW EXECUTE PROCEDURE console_preset_changed();
CREATE TRIGGER console_config_notify            AFTER UPDATE ON console_config                                FOR EACH ROW EXECUTE PROCEDURE console_config_changed();
CREATE TRIGGER functions_notify                 AFTER UPDATE ON functions                                     FOR EACH ROW EXECUTE PROCEDURE functions_changed();
CREATE TRIGGER preset_pool_notify               AFTER UPDATE ON preset_pool                                   FOR EACH ROW EXECUTE PROCEDURE preset_pool_changed();
CREATE TRIGGER src_pool_notify                  AFTER UPDATE ON src_pool                                      FOR EACH ROW EXECUTE PROCEDURE src_pool_changed();


