

-- returns the number of DSP cards present

CREATE OR REPLACE FUNCTION dsp_count() RETURNS integer AS $$
DECLARE
  r integer;
BEGIN
  SELECT INTO r COALESCE((
    SELECT COUNT(*) FROM slot_config s
     JOIN addresses a ON a.addr = s.addr
     WHERE (id).man = 1 AND (id).prod = 20
       AND s.slot_nr >= 16 AND s.slot_nr <= 20
    ), 0);
  RETURN r;
END
$$ LANGUAGE plpgsql STABLE;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION src_config_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT number FROM src_config ORDER BY pos )
  LOOP
    UPDATE src_config SET pos = cnt_pos WHERE number = _record.number;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  INSERT INTO recent_changes (change, arguments) VALUES('src_config_renumbered', cnt_pos);
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION dest_config_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT number FROM dest_config ORDER BY pos )
  LOOP
    UPDATE dest_config SET pos = cnt_pos WHERE number = _record.number;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION functions_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT rcv_type, xmt_type, (func).type AS type, (func).seq AS seq, (func).func AS func FROM functions ORDER BY pos )
  LOOP
    UPDATE functions SET pos = cnt_pos WHERE rcv_type = _record.rcv_type AND xmt_type = _record.xmt_type AND (func).type = _record.type AND (func).func = _record.func;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION src_preset_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT number FROM src_preset ORDER BY pos )
  LOOP
    UPDATE src_preset SET pos = cnt_pos WHERE number = _record.number;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION buss_preset_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT number FROM buss_preset ORDER BY pos )
  LOOP
    UPDATE buss_preset SET pos = cnt_pos WHERE number = _record.number;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION console_preset_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT number FROM console_preset ORDER BY pos )
  LOOP
    UPDATE console_preset SET pos = cnt_pos WHERE number = _record.number;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Renumbers the position column to be continues from '1'

CREATE OR REPLACE FUNCTION users_renumber() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT number FROM users ORDER BY pos )
  LOOP
    UPDATE users SET pos = cnt_pos WHERE number = _record.number;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql;


-- Drops constrains using the table/column name

CREATE OR REPLACE FUNCTION drop_constraints(table_name_to_check character varying, column_name_to_check character varying) RETURNS integer AS $$
DECLARE
  con varchar;
  cnt integer;
BEGIN
  cnt := 0;
  FOR con IN (SELECT constraint_name FROM information_schema.constraint_column_usage WHERE table_catalog = 'axum' AND table_name = table_name_to_check AND column_name = column_name_to_check)
  LOOP
    EXECUTE 'ALTER TABLE ' || table_name_to_check || ' DROP CONSTRAINT ' || con;
    cnt := cnt+1;
  END LOOP;
  RETURN cnt;
END
$$ LANGUAGE plpgsql;


-- Renumbers the functions to the first default (order by func type/number).

CREATE OR REPLACE FUNCTION functions_renumber_default() RETURNS integer AS $$
DECLARE
  _record RECORD;
  cnt_pos smallint;
BEGIN
  cnt_pos := 1;
  FOR _record IN ( SELECT rcv_type, xmt_type, (func).type AS type, (func).seq AS seq, (func).func AS func FROM functions ORDER BY (func).type, (func).func )
  LOOP
    UPDATE functions SET pos = cnt_pos WHERE rcv_type = _record.rcv_type AND xmt_type = _record.xmt_type AND (func).type = _record.type AND (func).func = _record.func;
    cnt_pos := cnt_pos + 1;
  END LOOP;
  RETURN cnt_pos;
END
$$ LANGUAGE plpgsql


-- a VIEW with all channels available on the matrix

CREATE OR REPLACE VIEW matrix_sources AS
((((( SELECT 1 AS pos, 'none' AS type, (-1) AS number, 'mute' AS label, true AS active, src_pool.pool1, src_pool.pool2, src_pool.pool3, src_pool.pool4, src_pool.pool5, src_pool.pool6, src_pool.pool7, src_pool.pool8
       FROM src_pool
       WHERE src_pool.number = (-1)
      UNION
       SELECT 2 AS pos, 'none' AS type, 0 AS number, 'none' AS label, true AS active, src_pool.pool1, src_pool.pool2, src_pool.pool3, src_pool.pool4, src_pool.pool5, src_pool.pool6, src_pool.pool7, src_pool.pool8
        FROM src_pool
        WHERE src_pool.number = 0)
      UNION
       SELECT 2 + src_config.pos AS pos, 'source' AS type, src_config.number + 288 AS number, src_config.label, true AS active, src_config.pool1, src_config.pool2, src_config.pool3, src_config.pool4, src_config.pool5, src_config.pool6, src_config.pool7, src_config.pool8
        FROM src_config)
      UNION
       SELECT 2 + buss_config.number + (( SELECT max(src_config.pos) AS max
        FROM src_config)) AS pos, 'buss' AS type, buss_config.number, buss_config.label, true AS active, src_pool.pool1, src_pool.pool2, src_pool.pool3, src_pool.pool4, src_pool.pool5, src_pool.pool6, src_pool.pool7, src_pool.pool8
        FROM buss_config
        JOIN src_pool ON buss_config.number = src_pool.number)
      UNION
       SELECT 2 + monitor_buss_config.number + (( SELECT max(src_config.pos) AS max
        FROM src_config)) + 16 AS pos, 'monitor buss' AS type, monitor_buss_config.number + 16 AS number, monitor_buss_config.label, monitor_buss_config.number <= (dsp_count() * 4) AS active, src_pool.pool1, src_pool.pool2, src_pool.pool3, src_pool.pool4, src_pool.pool5, src_pool.pool6, src_pool.pool7, src_pool.pool8
        FROM monitor_buss_config
        JOIN src_pool ON (monitor_buss_config.number + 16) = src_pool.number)
      UNION
       SELECT 2 + g.n + (( SELECT max(src_config.pos) AS max
        FROM src_config)) + 32 AS pos, 'insert out' AS type, g.n + 32 AS number, ('Module '::text || g.n) || ' insert out'::text AS label, g.n <= (dsp_count() * 32) AS active, src_pool.pool1, src_pool.pool2, src_pool.pool3, src_pool.pool4, src_pool.pool5, src_pool.pool6, src_pool.pool7, src_pool.pool8
        FROM generate_series(1, 128) g(n)
        JOIN src_pool ON (g.n + 32) = src_pool.number)
      UNION
       SELECT 2 + g.n + (( SELECT max(src_config.pos) AS max
        FROM src_config)) + 160 AS pos, 'n-1' AS type, g.n + 160 AS number, ('Module '::text || g.n) || ' N-1'::text AS label, g.n <= (dsp_count() * 32) AS active, src_pool.pool1, src_pool.pool2, src_pool.pool3, src_pool.pool4, src_pool.pool5, src_pool.pool6, src_pool.pool7, src_pool.pool8
        FROM generate_series(1, 128) g(n)
        JOIN src_pool ON (g.n + 160) = src_pool.number;


-- a VIEW for the processing presets

CREATE OR REPLACE VIEW processing_presets AS
( SELECT 1 AS pos, (-1) AS number, 'Default' AS label, preset_pool.pool1, preset_pool.pool2, preset_pool.pool3, preset_pool.pool4, preset_pool.pool5, preset_pool.pool6, preset_pool.pool7, preset_pool.pool8
   FROM preset_pool
   WHERE preset_pool.number = (-1)
  UNION
   SELECT 2 AS pos, 0 AS number, 'None' AS label, preset_pool.pool1, preset_pool.pool2, preset_pool.pool3, preset_pool.pool4, preset_pool.pool5, preset_pool.pool6, preset_pool.pool7, preset_pool.pool8
    FROM preset_pool
    WHERE preset_pool.number = 0)
  UNION
   SELECT 2 + src_preset.pos AS pos, src_preset.number, src_preset.label, src_preset.pool1, src_preset.pool2, src_preset.pool3, src_preset.pool4, src_preset.pool5, src_preset.pool6, src_preset.pool7, src_preset.pool8
    FROM src_preset;

