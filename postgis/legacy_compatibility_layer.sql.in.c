-- $Id: legacy.sql.in.c 7548 2011-07-02 08:58:38Z robe $
-- Legacy functions without chip functions --
#include "sqldefines.h"
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION AsBinary(geometry)
	RETURNS bytea
	AS 'MODULE_PATHNAME','LWGEOM_asBinary'
	LANGUAGE 'C' IMMUTABLE STRICT;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION AsBinary(geometry,text)
	RETURNS bytea
	AS 'MODULE_PATHNAME','LWGEOM_asBinary'
	LANGUAGE 'C' IMMUTABLE STRICT;

-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION AsText(geometry)
	RETURNS TEXT
	AS 'MODULE_PATHNAME','LWGEOM_asText'
	LANGUAGE 'C' IMMUTABLE STRICT;	

-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION Estimated_Extent(text,text,text) RETURNS box2d AS
	'MODULE_PATHNAME', 'geometry_estimated_extent'
	LANGUAGE 'C' IMMUTABLE STRICT SECURITY DEFINER;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION Estimated_Extent(text,text) RETURNS box2d AS
	'MODULE_PATHNAME', 'geometry_estimated_extent'
	LANGUAGE 'C' IMMUTABLE STRICT SECURITY DEFINER;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION GeomFromText(text, int4)
	RETURNS geometry AS 'SELECT ST_GeomFromText($1, $2)'
	LANGUAGE 'SQL' IMMUTABLE STRICT;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION GeomFromText(text)
	RETURNS geometry AS 'SELECT ST_GeomFromText($1)'
	LANGUAGE 'SQL' IMMUTABLE STRICT;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION ndims(geometry)
	RETURNS smallint
	AS 'MODULE_PATHNAME', 'LWGEOM_ndims'
	LANGUAGE 'C' IMMUTABLE STRICT;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION SetSRID(geometry,int4)
	RETURNS geometry
	AS 'MODULE_PATHNAME','LWGEOM_set_srid'
	LANGUAGE 'C' IMMUTABLE STRICT;
	
-- Deprecation in 1.2.3
CREATE OR REPLACE FUNCTION SRID(geometry)
	RETURNS int4
	AS 'MODULE_PATHNAME','LWGEOM_get_srid'
	LANGUAGE 'C' IMMUTABLE STRICT;
	
