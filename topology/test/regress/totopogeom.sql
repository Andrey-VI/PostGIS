\set VERBOSITY terse
set client_min_messages to ERROR;

select 'create', createtopology('tt') > 0;

-- Invalid calls
select totopogeom('POINT(0 0)'::geometry, 'unexistent', 1);
select totopogeom('POINT(0 0)'::geometry, 'tt', 1);
select totopogeom(null, 'tt', 1);
select totopogeom('POINT(0 0)'::geometry, '', 1);
select totopogeom('POINT(0 0)'::geometry, null, 1);
select totopogeom('POINT(0 0)'::geometry, 'tt', null);

-- Create simple puntual layer (will be layer 1)
CREATE TABLE tt.f_puntal(id serial);
SELECT 'simple_puntual_layer', AddTopoGeometryColumn('tt', 'tt', 'f_puntal','g','POINT');

-- Create a hierarchical layer (will be layer 2)
CREATE TABLE tt.f_hier(id serial);
SELECT 'hierarchical_layer', AddTopoGeometryColumn('tt', 'tt', 'f_hier','g','COLLECTION', 1);

-- Create a lineal layer (will be layer 3)
CREATE TABLE tt.f_lineal(id serial);
SELECT 'simple_lineal_layer', AddTopoGeometryColumn('tt', 'tt', 'f_lineal','g','LINE');

-- Create an areal layer (will be layer 4)
CREATE TABLE tt.f_areal(id serial);
SELECT 'simple_areal_layer', AddTopoGeometryColumn('tt', 'tt', 'f_areal','g','POLYGON');

-- Create a collection layer (will be layer 5)
CREATE TABLE tt.f_coll(id serial);
SELECT 'simple_collection_layer', AddTopoGeometryColumn('tt', 'tt', 'f_coll','g','COLLECTION');

-- A couple more invalid calls
select totopogeom('POINT(0 0)'::geometry, 'tt', 30); -- non existent layer
select totopogeom('POINT(0 0)'::geometry, 'tt', 2); -- invalid (hierarchical) layer
select totopogeom('LINESTRING(0 0, 10 10)'::geometry, 'tt', 1); -- invalid (puntual) layer
select totopogeom('LINESTRING(0 0, 10 10)'::geometry, 'tt', 4); -- invalid (areal) layer
select totopogeom('MULTIPOINT(0 0, 10 10)'::geometry, 'tt', 3); -- invalid (lineal) layer
select totopogeom('MULTIPOINT(0 0, 10 10)'::geometry, 'tt', 4); -- invalid (areal) layer
select totopogeom('POLYGON((0 0, 10 10, 10 0, 0 0))'::geometry, 'tt', 1); -- invalid (puntal) layer
select totopogeom('POLYGON((0 0, 10 10, 10 0, 0 0))'::geometry, 'tt', 3); -- invalid (lineal) layer

-- Convert a point
with inp as ( select 'POINT(0 0)' ::geometry as g)
select St_AsText(g), ST_Equals(totopogeom(g, 'tt', 1)::geometry, g) from inp;

-- Convert a line
with inp as ( select 'LINESTRING(0 10, 10 10)' ::geometry as g)
select St_AsText(g), ST_Equals(totopogeom(g, 'tt', 3)::geometry, g) from inp;

-- Convert a polygon
with inp as ( select
'POLYGON((0 20, 10 20, 5 30, 0 20),(2 22, 8 22, 5 28, 2 22))'
 ::geometry as g)
select St_AsText(g), ST_Equals(totopogeom(g, 'tt', 4)::geometry, g) from inp;

-- Convert a multipoint
with inp as ( select 'MULTIPOINT((0 -10),(5 -10))' ::geometry as g)
select St_AsText(g), ST_Equals(totopogeom(g, 'tt', 1)::geometry, g) from inp;

-- Convert a multiline
with inp as ( select 'MULTILINESTRING((-1 10, -10 10),(-10 8, -2 9))' ::geometry as g)
select St_AsText(g), ST_Equals(totopogeom(g, 'tt', 3)::geometry, g) from inp;

-- Convert a multipolygon
with inp as ( select
'MULTIPOLYGON(((100 20, 110 20, 105 30, 100 20),(102 22, 108 22, 105 28, 102 22)),((80 20, 90 20, 90 60, 80 20)))'
 ::geometry as g)
select St_AsText(g), ST_Equals(totopogeom(g, 'tt', 4)::geometry, g) from inp;

-- Convert a collection
with
inp as ( select
'GEOMETRYCOLLECTION(
 POINT(-100 -100),
 LINESTRING(-100 -90,-90 -90),
 POLYGON((-100 -80,-90 -80,-95 -70,-100 -80),(-98 -78,-92 -78,-95 -72,-98 -78)),
 MULTIPOINT(-100 -110,-95 -110),
 LINESTRING EMPTY,
 MULTILINESTRING((-101 -90,-110 -90),(-110 -92,-102 -91)),
 MULTIPOLYGON(((0 -80,10 -80,5 -70,0 -80),(2 -78,8 -78,5 -72,2 -78)),((-20 -80,-10 -80,-10 -40,-20 -80)))
)'
 ::geometry as g),
tg as ( select totopogeom(g, 'tt', 5) as g from inp )
select St_AsText(inp.g), st_astext(tg.g::geometry) from inp, tg;


-- Convert some empties
SELECT ST_AsText(toTopoGeom('POINT EMPTY', 'tt', 1)::geometry);
SELECT ST_AsText(toTopoGeom('MULTIPOINT EMPTY', 'tt', 1)::geometry);
SELECT ST_AsText(toTopoGeom('LINESTRING EMPTY', 'tt', 3)::geometry);
SELECT ST_AsText(toTopoGeom('MULTILINESTRING EMPTY', 'tt', 3)::geometry);
SELECT ST_AsText(toTopoGeom('POLYGON EMPTY', 'tt', 4)::geometry);
SELECT ST_AsText(toTopoGeom('MULTIPOLYGON EMPTY', 'tt', 4)::geometry);
SELECT ST_AsText(toTopoGeom('GEOMETRYCOLLECTION EMPTY', 'tt', 5)::geometry);

DROP TABLE tt.f_coll;
DROP TABLE tt.f_areal;
DROP TABLE tt.f_lineal;
DROP TABLE tt.f_hier;
DROP TABLE tt.f_puntal;
select droptopology('tt');
