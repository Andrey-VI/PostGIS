-- $Id: postgis_drop_before.sql.in.c 7896 2011-09-27 01:55:49Z robe $
-- These are functions where the argument names may have changed  --
-- so have to be dropped before upgrade can happen --
DROP FUNCTION IF EXISTS AddGeometryColumn(varchar,varchar,varchar,varchar,integer,varchar,integer,boolean);
DROP FUNCTION IF EXISTS ST_MakeEnvelope(float8, float8, float8, float8);
--changed name of prec arg to be consistent with ST_AsGML/KML
DROP FUNCTION IF EXISTS ST_AsX3D(geometry, integer, integer); 
