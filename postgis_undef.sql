begin;
--- indexing meta table stuff
delete from pg_amproc where amopclaid = (select oid from pg_opclass where opcname = 'gist_geometry_ops');
delete from pg_amop where amopclaid = (select oid from pg_opclass where opcname = 'gist_geometry_ops');
delete from pg_opclass where opcname = 'gist_geometry_ops';
--- AGGREGATEs
drop AGGREGATE extent geometry;
--- operators
drop operator <<  (geometry,geometry);
drop operator &<  (geometry,geometry);
drop operator &&  (geometry,geometry);
drop operator &>  (geometry,geometry);
drop operator >>  (geometry,geometry);
drop operator ~=  (geometry,geometry);
drop operator @  (geometry,geometry);
drop operator ~  (geometry,geometry);
drop operator =  (geometry,geometry);
drop operator <  (geometry,geometry);
drop operator >  (geometry,geometry);
--- functions
drop function POSTGIS_VERSION ();
drop function find_SRID (varchar,varchar,varchar);
drop function DropGeometryColumn (varchar,varchar,varchar);
drop function AddGeometryColumn (varchar,varchar,varchar,integer,varchar,integer);
drop function BOX3D_in (opaque);
drop function BOX3D_out (opaque);
drop function SPHEROID_in (opaque);
drop function SPHEROID_out (opaque);
drop function WKB_in (opaque);
drop function WKB_out (opaque);
drop function geometry_in (opaque);
drop function geometry_out (opaque);
drop function box3d (GEOMETRY);
drop function geometry (BOX3D);
drop function geometry (text);
drop function expand (BOX3D,float8);
drop function asbinary (GEOMETRY);
drop function asbinary (GEOMETRY,TEXT);
drop function npoints (GEOMETRY);
drop function nrings (GEOMETRY);
drop function mem_size (GEOMETRY);
drop function numb_sub_objs (GEOMETRY);
drop function summary (GEOMETRY);
drop function translate (GEOMETRY,float8,float8,float8);
drop function dimension (GEOMETRY);
drop function geometrytype (GEOMETRY);
drop function envelope (GEOMETRY);
drop function x (GEOMETRY);
drop function y (GEOMETRY);
drop function z (GEOMETRY);
drop function numpoints (GEOMETRY);
drop function pointn (GEOMETRY,INTEGER);
drop function exteriorring (GEOMETRY);
drop function numinteriorrings (GEOMETRY);
drop function interiorringn (GEOMETRY,INTEGER);
drop function numgeometries (GEOMETRY);
drop function geometryn (GEOMETRY,INTEGER);
drop function distance (GEOMETRY,GEOMETRY);
drop function astext (geometry);
drop function srid (geometry);
drop function geometryfromtext (geometry,int4);
drop function setSRID (geometry,int4);
drop function length_spheroid (GEOMETRY,SPHEROID);
drop function length3d_spheroid (GEOMETRY,SPHEROID);
drop function length3d (GEOMETRY);
drop function length (GEOMETRY);
drop function area2d (GEOMETRY);
drop function perimeter3d (GEOMETRY);
drop function perimeter (GEOMETRY);
drop function truly_inside (GEOMETRY,GEOMETRY);
drop function point_inside_circle (GEOMETRY,float8,float8,float8);
drop function startpoint (GEOMETRY);
drop function endpoint (GEOMETRY);
drop function isclosed (GEOMETRY);
drop function centroid (GEOMETRY);
drop function combine_bbox (BOX3D,GEOMETRY);
drop function geometry_overleft (GEOMETRY, GEOMETRY);
drop function geometry_overright (GEOMETRY, GEOMETRY);
drop function geometry_left (GEOMETRY, GEOMETRY);
drop function geometry_right (GEOMETRY, GEOMETRY);
drop function geometry_contain (GEOMETRY, GEOMETRY);
drop function geometry_contained (GEOMETRY, GEOMETRY);
drop function geometry_overlap (GEOMETRY, GEOMETRY);
drop function geometry_same (GEOMETRY, GEOMETRY);
drop function geometry_lt (GEOMETRY, GEOMETRY);
drop function geometry_gt (GEOMETRY, GEOMETRY);
drop function geometry_eq (GEOMETRY, GEOMETRY);
drop function force_2d (GEOMETRY);
drop function force_3d (GEOMETRY);
drop function force_collection (GEOMETRY);
drop function ggeometry_consistent (opaque,GEOMETRY,int4);
drop function ggeometry_compress (opaque);
drop function ggeometry_penalty (opaque,opaque,opaque);
drop function ggeometry_picksplit (opaque, opaque);
drop function ggeometry_union (bytea, opaque);
drop function ggeometry_same (opaque, opaque, opaque);
drop function rtree_decompress (opaque);
drop function postgis_gist_sel (oid, oid, int2, opaque, int4);
drop function geometry_union (GEOMETRY,GEOMETRY);
drop function geometry_inter (GEOMETRY,GEOMETRY);
drop function geometry_size (GEOMETRY,opaque);
--- types
drop type SPHEROID ;
drop type BOX3D ;
drop type WKB ;
drop type GEOMETRY ;
----tables
drop table spatial_ref_sys;
drop table geometry_columns;

end;
