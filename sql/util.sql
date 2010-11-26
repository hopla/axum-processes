

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
    UPDATE functions SET pos = cnt_pos WHERE rcv_type = _record.rcv_type AND xmt_type = _record.xmt_type AND (func).type = _record.type AND (func).seq = _record.seq AND (func).func = _record.func;
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


-- a VIEW with all channels available on the matrix

CREATE OR REPLACE VIEW matrix_sources (pos, type, number, label, active) AS
  SELECT 1, 'none', -1, 'none', true
  UNION
   SELECT 2, 'none', 0, 'Mute', true
  UNION
   SELECT 2+pos, 'source', number+288, label, true
    FROM src_config
  UNION
   SELECT 2+number+(SELECT MAX(pos) FROM src_config), 'buss', number, label, true
    FROM buss_config
  UNION
   SELECT 2+number+(SELECT MAX(pos) FROM src_config)+16, 'monitor buss', number+16, label, number <= dsp_count()*4
    FROM monitor_buss_config
  UNION
   SELECT 2+g.n+(SELECT MAX(pos) FROM src_config)+32, 'insert out', g.n+32, 'Module '||g.n||' insert out', g.n <= dsp_count()*32
    FROM generate_series(1, 128) AS g(n)
  UNION
   SELECT 2+g.n+(SELECT MAX(pos) FROM src_config)+160, 'n-1', g.n+160, 'Module '||g.n||' N-1', g.n <= dsp_count()*32
    FROM generate_series(1, 128) AS g(n);
