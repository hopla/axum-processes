

-- returns the number of DSP cards present

CREATE OR REPLACE FUNCTION dsp_count() RETURNS integer AS $$
DECLARE
  r integer;
BEGIN
  SELECT INTO r COALESCE((
    SELECT COUNT(*) FROM slot_config s
     JOIN addresses a ON a.addr = s.addr
     WHERE (id).man = 1 AND (id).prod = 20
    ), 0);
  RETURN r;
END
$$ LANGUAGE plpgsql STABLE;


-- a VIEW with all channels available on the matrix

CREATE OR REPLACE VIEW matrix_sources (type, number, label, active) AS
   SELECT 'buss', number, label, true
    FROM buss_config
  UNION
   SELECT 'monitor bus', number+16, label, number <= dsp_count()*4
    FROM monitor_buss_config
  UNION
   SELECT 'insert out', g.n+32, 'Module '||g.n||' insert out', g.n <= dsp_count()*32
    FROM generate_series(1, 128) AS g(n)
  UNION
   SELECT 'n-1', g.n+160, 'Module '||g.n||' N-1', g.n <= dsp_count()*32
    FROM generate_series(1, 128) AS g(n)
  UNION
   SELECT 'source', number+288, label, true
    FROM src_config;


