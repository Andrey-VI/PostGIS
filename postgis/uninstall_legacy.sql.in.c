-- $Id: uninstall_legacy.sql.in.c 7065 2011-04-26 12:35:02Z robe $
-- Uninstall Legacy functions useful when you have your applications finally set up to not use legacy functions  --
#include "sqldefines.h"
--- start functions that in theory should never have been used or internal like stuff deprecated
#include "postgis_drop.sql.in.c"
-- Drop Affine family of deprecated functions --
DROP FUNCTION IF EXISTS Affine(geometry,float8,float8,float8,float8,float8,float8,float8,float8,float8,float8,float8,float8);
DROP FUNCTION IF EXISTS Affine(geometry,float8,float8,float8,float8,float8,float8);
DROP FUNCTION IF EXISTS RotateZ(geometry,float8);
DROP FUNCTION IF EXISTS Rotate(geometry,float8);
DROP FUNCTION IF EXISTS RotateX(geometry,float8);
DROP FUNCTION IF EXISTS RotateY(geometry,float8);
DROP FUNCTION IF EXISTS Scale(geometry,float8,float8,float8);
DROP FUNCTION IF EXISTS Scale(geometry,float8,float8);
DROP FUNCTION IF EXISTS Translate(geometry,float8,float8,float8);
DROP FUNCTION IF EXISTS Translate(geometry,float8,float8);
DROP FUNCTION IF EXISTS TransScale(geometry,float8,float8,float8,float8);

-- Other functions --
DROP FUNCTION IF EXISTS AddPoint(geometry,geometry);
DROP FUNCTION IF EXISTS AddPoint(geometry,geometry, integer);
DROP FUNCTION IF EXISTS Area(geometry);
DROP FUNCTION IF EXISTS Area2D(geometry);
DROP FUNCTION IF EXISTS AsBinary(geometry);
DROP FUNCTION IF EXISTS AsBinary(geometry,text);
DROP FUNCTION IF EXISTS AsEWKB(geometry);
DROP FUNCTION IF EXISTS AsEWKB(geometry,text);
DROP FUNCTION IF EXISTS AsEWKT(geometry);
DROP FUNCTION IF EXISTS AsGML(geometry);
DROP FUNCTION IF EXISTS AsGML(geometry,int4);
DROP FUNCTION IF EXISTS AsKML(geometry);
DROP FUNCTION IF EXISTS AsKML(geometry,int4);
DROP FUNCTION IF EXISTS AsHEXEWKB(geometry);
DROP FUNCTION IF EXISTS AsHEXEWKB(geometry,text);
DROP FUNCTION IF EXISTS AsSVG(geometry);
DROP FUNCTION IF EXISTS AsSVG(geometry,int4);
DROP FUNCTION IF EXISTS AsSVG(geometry,int4,int4);
DROP FUNCTION IF EXISTS AsText(geometry);
