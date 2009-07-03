

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


-- a VIEW with all channels available on the matrix

CREATE OR REPLACE VIEW matrix_sources (pos, type, number, label, active) AS
   SELECT number, 'buss', number, label, true
    FROM buss_config
  UNION
   SELECT number+16, 'monitor buss', number+16, label, number <= dsp_count()*4
    FROM monitor_buss_config
  UNION
   SELECT g.n+32, 'insert out', g.n+32, 'Module '||g.n||' insert out', g.n <= dsp_count()*32
    FROM generate_series(1, 128) AS g(n)
  UNION
   SELECT g.n+160, 'n-1', g.n+160, 'Module '||g.n||' N-1', g.n <= dsp_count()*32
    FROM generate_series(1, 128) AS g(n)
  UNION
   SELECT pos+288, 'source', number+288, label, true
    FROM src_config;


