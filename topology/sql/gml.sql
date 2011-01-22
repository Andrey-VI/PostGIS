-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
-- 
-- PostGIS - Spatial Types for PostgreSQL
-- http://postgis.refractions.net
--
-- Copyright (C) 2010 Sandro Santilli <strk@keybit.net>
--
-- This is free software; you can redistribute and/or modify it under
-- the terms of the GNU General Public Licence. See the COPYING file.
--
-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
--
-- Functions used for topology GML output
--
-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
--
-- Developed by Sandro Santilli <strk@keybit.net>
-- for Faunalia (http://www.faunalia.it) with funding from
-- Regione Toscana - Sistema Informativo per la Gestione del Territorio
-- e dell' Ambiente [RT-SIGTA].
-- For the project: "Sviluppo strumenti software per il trattamento di dati
-- geografici basati su QuantumGIS e Postgis (CIG 0494241492)"
--
-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

--{
--
-- text AsGMLNode(id, point, nsprefix)
--
-- 
CREATE OR REPLACE FUNCTION topology.AsGMLNode(int, geometry, text)
  RETURNS text
AS
$$
DECLARE
  id ALIAS FOR $1;
  point ALIAS FOR $2;
  nsprefix_in ALIAS FOR $3;
  nsprefix text;
  gml text;
BEGIN

  nsprefix := 'gml:';
  IF NOT nsprefix_in IS NULL THEN
    IF nsprefix_in = '' THEN
      nsprefix = nsprefix_in;
    ELSE
      nsprefix = nsprefix_in || ':';
    END IF;
  END IF;

  gml := '<' || nsprefix || 'Node ' || nsprefix || 'id="N' || id || '"';
  IF point IS NOT NULL THEN
    gml = gml || '>'
              || '<' || nsprefix || 'pointProperty>'
              || ST_AsGML(3, point, 15, 1, nsprefix_in)
              || '</' || nsprefix || 'pointProperty>'
              || '</' || nsprefix || 'Node>';
  ELSE
    gml = gml || '/>';
  END IF;
  RETURN gml;
END
$$
LANGUAGE 'plpgsql';
--} AsGMLNode(id, point, nsprefix)

--{ AsGMLNode(id, point)
CREATE OR REPLACE FUNCTION topology.AsGMLNode(int, geometry) RETURNS text
AS $$ SELECT topology.AsGMLNode($1, $2, 'gml'); $$
LANGUAGE 'sql';
--} AsGMLNode(id, point)

--{ AsGMLNode(id)
CREATE OR REPLACE FUNCTION topology.AsGMLNode(int) RETURNS text
AS $$ SELECT topology.AsGMLNode($1, NULL, 'gml'); $$
LANGUAGE 'sql';
--} AsGMLNode(id)

--{
--
-- text AsGMLEdge(edge_id, start_node, end_node, line, nsprefix)
--
-- 
CREATE OR REPLACE FUNCTION topology.AsGMLEdge(int, int, int, geometry, text)
  RETURNS text
AS
$$
DECLARE
  edge_id ALIAS FOR $1;
  start_node ALIAS FOR $2;
  end_node ALIAS for $3;
  line ALIAS FOR $4;
  nsprefix_in ALIAS FOR $5;
  nsprefix text;
  gml text;
BEGIN

  nsprefix := 'gml:';
  IF nsprefix_in IS NOT NULL THEN
    IF nsprefix_in = '' THEN
      nsprefix = nsprefix_in;
    ELSE
      nsprefix = nsprefix_in || ':';
    END IF;
  END IF;

  gml := '<' || nsprefix || 'Edge ' || nsprefix || 'id="E' || edge_id || '">';

  -- Start node
  -- TODO: optionally output the directedNode as xlink, using a visited map
  gml = gml || '<' || nsprefix || 'directedNode orientation="-">';
  gml = gml || topology.AsGMLNode(start_node, NULL, nsprefix_in);
  gml = gml || '</' || nsprefix || 'directedNode>';

  -- End node
  -- TODO: optionally output the directedNode as xlink, using a visited map
  gml = gml || '<' || nsprefix || 'directedNode>';
  --gml = gml || topology.AsGMLNode(end_node, NULL, nsprefix_in);
  gml = gml || '</' || nsprefix || 'directedNode>';

  IF line IS NOT NULL THEN
    gml = gml || '<' || nsprefix || 'curveProperty>'
              || ST_AsGML(3, line, 15, 1, nsprefix_in)
              || '</' || nsprefix || 'curveProperty>';
  END IF;

  gml = gml || '</' || nsprefix || 'Edge>';

  RETURN gml;
END
$$
LANGUAGE 'plpgsql';
--} AsGMLEdge(id, start_node, end_node, line, nsprefix)

--{ AsGMLEdge(id, start_node, end_node, line)
CREATE OR REPLACE FUNCTION topology.AsGMLEdge(int, int, int, geometry)
RETURNS text AS $$
  SELECT topology.AsGMLEdge($1, $2, $3, $4, 'gml');
$$ LANGUAGE 'sql';
--} AsGMLEdge(id, start_node, end_node, line)



--{
--
-- text AsGML(TopoGeometry, nsprefix)
--
-- 
CREATE OR REPLACE FUNCTION topology.AsGML(topology.TopoGeometry, text)
  RETURNS text
AS
$$
DECLARE
  tg ALIAS FOR $1;
  nsprefix_in ALIAS FOR $2;
  nsprefix text;
  toponame text;
  gml text;
  sql text;
  rec RECORD;
  rec2 RECORD;
  bounds geometry;
BEGIN

  nsprefix := 'gml:';
  IF nsprefix_in IS NOT NULL THEN
    IF nsprefix_in = '' THEN
      nsprefix = nsprefix_in;
    ELSE
      nsprefix = nsprefix_in || ':';
    END IF;
  END IF;

  -- Get topology name (for subsequent queries)
  SELECT name FROM topology.topology into toponame
              WHERE id = tg.topology_id;

  -- Puntual TopoGeometry
  IF tg.type = 1 THEN
    gml = '<' || nsprefix || 'TopoPoint>';
    -- For each defining node, print a directedNode
    FOR rec IN  EXECUTE 'SELECT r.element_id, n.geom from '
      || quote_ident(toponame) || '.relation r LEFT JOIN '
      || quote_ident(toponame) || '.node n ON (r.element_id = n.node_id)'
      || ' WHERE r.layer_id = ' || tg.layer_id
      || ' AND r.topogeo_id = ' || tg.id
    LOOP
      gml = gml || '<' || nsprefix || 'directedNode>';
      gml = gml || topology.AsGMLNode(rec.element_id, rec.geom, nsprefix_in);
      gml = gml || '</' || nsprefix || 'directedNode>';
    END LOOP;
    gml = gml || '</' || nsprefix || 'TopoPoint>';
    RETURN gml;

  ELSIF tg.type = 2 THEN -- lineal
    gml = '<' || nsprefix || 'TopoCurve>';
    -- For each defining edge, print a directedEdge
    FOR rec IN  EXECUTE
      'SELECT r.element_id as rid, e.edge_id, e.geom, '
      || 'e.start_node, e.end_node from '
      || quote_ident(toponame) || '.relation r LEFT JOIN '
      || quote_ident(toponame) || '.edge e ON (abs(r.element_id) = e.edge_id)'
      || ' WHERE r.layer_id = ' || tg.layer_id
      || ' AND r.topogeo_id = ' || tg.id
    LOOP
      IF rec.rid < 0 THEN
        gml = gml || '<' || nsprefix || 'directedEdge orientation="-">';
      ELSE
        gml = gml || '<' || nsprefix || 'directedEdge>';
      END IF;
      gml = gml || topology.AsGMLEdge(rec.edge_id, rec.start_node,
                                      rec.end_node, rec.geom, nsprefix_in);
      gml = gml || '</' || nsprefix || 'directedEdge>';
    END LOOP;
    gml = gml || '</' || nsprefix || 'TopoCurve>';
    return gml;

  ELSIF tg.type = 3 THEN -- areal
    gml = '<' || nsprefix || 'TopoSurface>';

    -- Construct the geometry, then for each polygon:
    FOR rec IN SELECT (ST_DumpRings((ST_Dump(topology.Geometry(tg))).geom)).*
    LOOP
      -- print a directedFace for each exterior ring
      -- and a negative directedFace for
      -- each interior ring.
      IF rec.path[1] = 0 THEN
        gml = gml || '<' || nsprefix || 'directedFace>';
      ELSE
        gml = gml || '<' || nsprefix || 'directedFace orientation="-">';
      END IF;

      -- Contents of a directed face are the list of edges
      -- that cover the specific ring
      bounds = ST_Boundary(rec.geom);

      -- TODO: figure out a way to express an id for a face
      --       and use a reference for an already-seen face ?
      gml = gml || '<' || nsprefix || 'Face>';
      FOR rec2 IN EXECUTE 'SELECT e.* FROM ' || quote_ident(toponame)
        || '.edge e WHERE ST_Covers(' || quote_literal(bounds::text)
        || ', e.geom)'
        -- TODO: add left_face/right_face to the conditional, to reduce load ?
      LOOP
        -- TODO: pass the 'visited' table over
        gml = gml || topology.AsGMLEdge(rec2.edge_id,
                                        rec2.start_node,
                                        rec2.end_node, rec2.geom,
                                        nsprefix_in);
      END LOOP;
      gml = gml || '</' || nsprefix || 'Face>';
      gml = gml || '</' || nsprefix || 'directedFace>';
    END LOOP;
  
    gml = gml || '</' || nsprefix || 'TopoSurface>';
    RETURN gml;

  ELSIF tg.type = 4 THEN -- collection
    RAISE EXCEPTION 'Collection TopoGeometries are not supported by AsGML';

  END IF;
	

  RETURN gml;
	
END
$$
LANGUAGE 'plpgsql';
--} AsGML(TopoGeometry, nsprefix)

--{
--
-- text AsGML(TopoGeometry)
--
-- 
CREATE OR REPLACE FUNCTION topology.AsGML(topology.TopoGeometry)
  RETURNS text
AS
$$
  SELECT topology.AsGML($1, 'gml');
$$ LANGUAGE 'sql';
-- } AsGML(TopoGeometry)
