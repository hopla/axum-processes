
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

CREATE TRIGGER recent_changes_notify AFTER INSERT ON recent_changes FOR EACH STATEMENT EXECUTE PROCEDURE notify_changes();


CREATE OR REPLACE FUNCTION addresses_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES ('address_removed', OLD.addr::text);
  ELSIF TG_OP = 'UPDATE' THEN
    IF (OLD.id).man <> (NEW.id).man OR (OLD.id).prod <> (NEW.id).prod OR (OLD.id).id <> (NEW.id).id THEN
      RAISE EXCEPTION 'Changing addresses.id is not allowed';
    END IF;
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
    INSERT INTO recent_changes (change, arguments) VALUES('defaults_changed', OLD.addr::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('defaults_changed', NEW.addr::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION node_config_changed() RETURNS trigger AS $$
BEGIN
  IF TG_OP = 'DELETE' THEN
    INSERT INTO recent_changes (change, arguments) VALUES('node_config_changed', OLD.addr::text);
  ELSE
    INSERT INTO recent_changes (change, arguments) VALUES('node_config_changed', NEW.addr::text);
  END IF;
  RETURN NULL;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION slot_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('slot_config_changed', NEW.slot_nr::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION src_config_changed() RETURNS trigger AS $$
BEGIN
  INSERT INTO recent_changes (change, arguments) VALUES('src_config_changed', NEW.number::text);
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
  INSERT INTO recent_changes (change, arguments) VALUES('dest_config_changed', NEW.number::text);
  RETURN NULL;
END
$$ LANGUAGE plpgsql;




-- T R I G G E R S

CREATE TRIGGER template_change_notify     AFTER DELETE ON templates                       FOR EACH ROW EXECUTE PROCEDURE templates_changed();
CREATE TRIGGER addresses_change_notify    AFTER DELETE OR UPDATE ON addresses             FOR EACH ROW EXECUTE PROCEDURE addresses_changed();
CREATE TRIGGER defaults_change_notify     AFTER INSERT OR DELETE OR UPDATE ON defaults    FOR EACH ROW EXECUTE PROCEDURE defaults_changed();
CREATE TRIGGER node_config_change_notify  AFTER INSERT OR DELETE OR UPDATE ON node_config FOR EACH ROW EXECUTE PROCEDURE node_config_changed();
CREATE TRIGGER slot_config_notify         AFTER UPDATE ON slot_config                     FOR EACH ROW EXECUTE PROCEDURE slot_config_changed();
CREATE TRIGGER src_config_notify          AFTER UPDATE ON src_config                      FOR EACH ROW EXECUTE PROCEDURE src_config_changed();
CREATE TRIGGER module_config_notify       AFTER UPDATE ON module_config                   FOR EACH ROW EXECUTE PROCEDURE module_config_changed();
CREATE TRIGGER buss_config_notify         AFTER UPDATE ON buss_config                     FOR EACH ROW EXECUTE PROCEDURE buss_config_changed();
CREATE TRIGGER monitor_buss_config_notify AFTER UPDATE ON monitor_buss_config             FOR EACH ROW EXECUTE PROCEDURE monitor_buss_config_changed();
CREATE TRIGGER extern_src_config_notify   AFTER UPDATE ON extern_src_config               FOR EACH ROW EXECUTE PROCEDURE extern_src_config_changed();
CREATE TRIGGER talkback_config_notify     AFTER UPDATE ON talkback_config                 FOR EACH ROW EXECUTE PROCEDURE talkback_config_changed();
CREATE TRIGGER global_config_notify       AFTER UPDATE ON global_config                   FOR EACH ROW EXECUTE PROCEDURE global_config_changed();
CREATE TRIGGER dest_config_notify         AFTER UPDATE ON dest_config                     FOR EACH ROW EXECUTE PROCEDURE dest_config_changed();
