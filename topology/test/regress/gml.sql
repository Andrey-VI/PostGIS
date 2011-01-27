set client_min_messages to WARNING;

INSERT INTO spatial_ref_sys ( auth_name, auth_srid, srid, proj4text ) VALUES ( 'EPSG', 4326, 4326, '+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs' );

\i load_topology-4326.sql
\i load_features.sql

--- Puntual single element {

-- Output simple puntual features (composed by single topo-element)
SELECT feature_name||'-vanilla', topology.AsGML(feature)
 FROM features.traffic_signs
 WHERE feature_name IN ('S1', 'S2', 'S3', 'S4' )
 ORDER BY feature_name;

-- Output again but with no prefix
SELECT feature_name||'-noprefix', topology.AsGML(feature, '')
 FROM features.traffic_signs 
 WHERE feature_name IN ('S1', 'S2', 'S3', 'S4' )
 ORDER BY feature_name;

-- Output again with custom prefix
SELECT feature_name||'-customprefix', topology.AsGML(feature, 'cstm')
 FROM features.traffic_signs 
 WHERE feature_name IN ('S1', 'S2', 'S3', 'S4' )
 ORDER BY feature_name;

-- Again with no prefix, no srsDimension (opt+=2)
-- and swapped lat/lon (opt+=16) and short CRS
SELECT feature_name||'-latlon', topology.AsGML(feature, '', 15, 18)
 FROM features.traffic_signs 
 WHERE feature_name IN ('S4');

--- } Puntual single-element 

--- Puntual multi element (TODO) {
--- } Puntual multi-element 

--- Lineal single element {

-- Output simple lineal features (composed by single topo element)
SELECT feature_name||'-vanilla', topology.AsGML(feature)
 FROM features.city_streets
 WHERE feature_name IN ('R3', 'R4' )
 ORDER BY feature_name;

-- Output again but with no prefix
SELECT feature_name||'-noprefix', topology.AsGML(feature, '')
 FROM features.city_streets 
 WHERE feature_name IN ('R3', 'R4' )
 ORDER BY feature_name;

-- Output again with custom prefix
SELECT feature_name||'-customprefix', topology.AsGML(feature, 'cstm')
 FROM features.city_streets 
 WHERE feature_name IN ('R3', 'R4' )
 ORDER BY feature_name;

--- } Lineal single-element

--- Lineal multi-element  {

-- Output simple lineal features (composed by single topo element)
SELECT feature_name||'-vanilla', topology.AsGML(feature)
 FROM features.city_streets
 WHERE feature_name IN ('R1', 'R2' )
 ORDER BY feature_name;

-- Output again but with no prefix
SELECT feature_name||'-noprefix', topology.AsGML(feature, '')
 FROM features.city_streets 
 WHERE feature_name IN ('R1', 'R2' )
 ORDER BY feature_name;

-- Output again with custom prefix
SELECT feature_name||'-customprefix', topology.AsGML(feature, 'cstm')
 FROM features.city_streets 
 WHERE feature_name IN ('R1', 'R2' )
 ORDER BY feature_name;

--- } Lineal multi-element

--- Areal single-element {

-- Output simple lineal features (composed by single topo element)
SELECT feature_name||'-vanilla', topology.AsGML(feature)
 FROM features.land_parcels
 WHERE feature_name IN ('P4', 'P5' )
 ORDER BY feature_name;

-- Output again but with no prefix
SELECT feature_name||'-noprefix', topology.AsGML(feature, '')
 FROM features.land_parcels WHERE feature_name IN ('P4', 'P5')
 ORDER BY feature_name;

-- Output again with custom prefix
SELECT feature_name||'-customprefix', topology.AsGML(feature, 'cstm')
 FROM features.land_parcels WHERE feature_name IN ('P4', 'P5')
 ORDER BY feature_name;

--- } Areal single-element 

--- Areal multi-element {

-- Output simple lineal features (composed by single topo element)
SELECT feature_name||'-vanilla', topology.AsGML(feature)
 FROM features.land_parcels
 WHERE feature_name IN ('P1', 'P2', 'P3' )
 ORDER BY feature_name;

-- Output again but with no prefix
SELECT feature_name||'-noprefix', topology.AsGML(feature, '')
 FROM features.land_parcels 
 WHERE feature_name IN ('P1', 'P2', 'P3' )
 ORDER BY feature_name;

-- Output again with custom prefix
SELECT feature_name||'-customprefix', topology.AsGML(feature, 'cstm')
 FROM features.land_parcels 
 WHERE feature_name IN ('P1', 'P2', 'P3' )
 ORDER BY feature_name;

--- } Areal multi-element 

--- { Visited table bookkeeping

CREATE TABLE visited (element_type int, element_id int);

-- R2 visits E4,E5
--           N5,N6,N7
SELECT feature_name||'-visited', topology.AsGML(feature,
       '', 15, 2, 'visited'::regclass) FROM features.city_streets
       WHERE feature_name IN ('R2');

-- S1 visits N14
-- S3 visits (N6)
SELECT feature_name||'-visited', topology.AsGML(feature,
       '', 15, 2, 'visited'::regclass) FROM features.traffic_signs
       WHERE feature_name IN ('S1', 'S3')
       ORDER BY feature_name;

-- R1 visits E9,E10,
--           N13,(N14),N15
SELECT feature_name||'-visited', topology.AsGML(feature,
       '', 15, 2, 'visited'::regclass) FROM features.city_streets
       WHERE feature_name IN ('R1');

-- P2 visits E7,E17,E18,E13,E20,E19 
--           N17,N18,(N13),N10,N9,(N14),N17
-- P1 visits E22,E21,E6,(E19),(E20),E12
--           N8,(N15),N16,(N17),(N14),(N9)
SELECT feature_name||'-visited', topology.AsGML(feature,
       '', 15, 2, 'visited'::regclass) FROM features.land_parcels
       WHERE feature_name IN ('P1', 'P2')
       ORDER BY feature_name DESC;

-- TODO: we'd need to query a street taking E14 and E13, to find E14 visited ..

-- Test custom identifier prefix
-- P3 visits (E18),(E17),E8,E15,E16,E14
--           (N10),(N13),(N18),N19,N12,N11
SELECT feature_name||'-visited-idprefix', topology.AsGML(feature,
       '', 15, 2, 'visited'::regclass, 'cd-') FROM features.land_parcels
       WHERE feature_name IN ('P3');

--- } Visited table bookkeeping

SELECT topology.DropTopology('city_data');
DROP SCHEMA features CASCADE;
DELETE FROM spatial_ref_sys where srid = 4326;
