--$Id$
 /*** 
 * 
 * Copyright (C) 2011 Regina Obe and Leo Hsu (Paragon Corporation)
 **/
-- This function given a point try to determine the approximate street address (norm_addy form)
-- and array of cross streets, as well as interpolated points along the streets
-- Use case example an address at the intersection of 3 streets: SELECT pprint_addy(r.addy[1]) As st1, pprint_addy(r.addy[2]) As st2, pprint_addy(r.addy[3]) As st3, array_to_string(r.street, ',') FROM reverse_geocode(ST_GeomFromText('POINT(-71.057811 42.358274)',4269)) As r;
--set search_path=tiger,public;
CREATE OR REPLACE FUNCTION reverse_geocode(IN pt geometry, IN include_strnum_range boolean, OUT intpt geometry[], OUT addy norm_addy[], OUT street character varying[])
  RETURNS record AS
$$
DECLARE
  var_redge RECORD;
  var_state text := NULL;
  var_countyfp text := NULL;
  var_addy NORM_ADDY;
  var_strnum varchar;
  var_nstrnum numeric(10);
  var_primary_line geometry := NULL;
  var_primary_dist numeric(10,2) ;
  var_pt geometry;
  var_stmt text;
  var_debug boolean = false;
BEGIN
	IF pt IS NULL THEN
		RETURN;
	ELSE
		IF ST_SRID(pt) = 4269 THEN
			var_pt := pt;
		ELSIF ST_SRID(pt) > 0 THEN
			var_pt := ST_Transform(pt, 4269); 
		ELSE --If srid is unknown, assume its 4269
			var_pt := ST_SetSRID(pt, 4269);
		END IF;
		var_pt := ST_SnapToGrid(var_pt, 0.00005); /** Get rid of floating point junk that would prevent intersections **/
	END IF;
	-- Determine state tables to check 
	-- this is needed to take advantage of constraint exclusion
	IF var_debug THEN
		RAISE NOTICE 'Get matching states start: %', clock_timestamp();
	END IF;
	SELECT statefp INTO var_state FROM state WHERE ST_Intersects(the_geom, var_pt) LIMIT 1;
	IF var_debug THEN
		RAISE NOTICE 'Get matching states end: % -  %', var_state, clock_timestamp();
	END IF;
	IF var_state IS NULL THEN
		-- We don't have any data for this state
		RETURN;
	END IF;
	IF var_debug THEN
		RAISE NOTICE 'Get matching counties start: %', clock_timestamp();
	END IF;
	SELECT countyfp INTO var_countyfp FROM county WHERE statefp = var_state AND ST_Intersects(the_geom, var_pt)  LIMIT 1;

	IF var_debug THEN
		RAISE NOTICE 'Get matching counties end: % - %',var_countyfp,  clock_timestamp();
	END IF;
	IF var_countyfp IS NULL THEN
		-- We don't have any data for this county
		RETURN;
	END IF;
	
	-- Find the street edges that this point is closest to with tolerance of 0.005 but only consider the edge if the point is contained in the right or left face
	-- Then order addresses by proximity to road
	IF var_debug THEN
		RAISE NOTICE 'Get matching edges start: %', clock_timestamp();
	END IF;

	var_stmt := '
		WITH 
			f AS 
			( SELECT * FROM faces 
			WHERE statefp = ' || quote_literal(var_state) || ' AND countyfp = ' || quote_literal(var_countyfp) || ' 
				AND ST_DWithin(faces.the_geom, ' || quote_literal(var_pt::text) || '::geometry, 0.001) ),
			e AS 
			( SELECT * FROM edges 
			WHERE statefp = ' || quote_literal(var_state) || ' AND countyfp = ' || quote_literal(var_countyfp) || ' 
				AND ST_DWithin(edges.the_geom, ' || quote_literal(var_pt::text) || '::geometry, 0.05)
				  )
				
		SELECT * 
		FROM (SELECT DISTINCT ON(fullname)  foo.fullname, foo.stusps, foo.zip, 
			   (SELECT z.place FROM zip_state_loc AS z WHERE z.zip = foo.zip and z.statefp = ' || quote_literal(var_state) || ' LIMIT 1) As place, foo.center_pt,
			  side, to_number(fromhn, ''999999'') As fromhn, to_number(tohn, ''999999'') As tohn, ST_GeometryN(ST_Multi(line),1) As line, foo.dist
		FROM 
		  (SELECT e.the_geom As line, e.fullname, a.zip, s.abbrev As stusps, ST_ClosestPoint(e.the_geom,' || quote_literal(var_pt::text) || '::geometry) As center_pt, e.statefp, a.side, a.fromhn, a.tohn, 
			ST_Distance_Sphere(e.the_geom, ' || quote_literal(var_pt::text) || '::geometry) As dist
				FROM e 
					INNER JOIN (SELECT * FROM state_lookup WHERE statefp = ' || quote_literal(var_state) || ' ) As s ON (e.statefp = s.statefp )
					INNER JOIN f As fl ON (e.tfidl = fl.tfid)
					INNER JOIN f As fr ON (e.tfidr = fr.tfid)
					INNER JOIN (SELECT * FROM addr WHERE statefp = ' || quote_literal(var_state) || ' ) As a ON ( e.tlid = a.tlid AND e.statefp = a.statefp AND  
					   ( ( ST_Covers(fl.the_geom, ' || quote_literal(var_pt::text) || '::geometry) AND a.side = ''L'') OR ( ST_Covers(fr.the_geom, ' || quote_literal(var_pt::text) || '::geometry) AND a.side = ''R'' ) ) )
					-- INNER JOIN zip_state_loc As z ON (a.statefp =  z.statefp AND a.zip = z.zip) /** really slow with this join **/
				WHERE ST_DWithin(e.the_geom, ' || quote_literal(var_pt::text) || '::geometry, 0.005)
				ORDER BY ST_Distance(e.the_geom, ' || quote_literal(var_pt::text) || '::geometry) LIMIT 4) As foo 
				WHERE dist < 150 --less than 150 m
				ORDER BY foo.fullname, foo.dist) As f ORDER BY f.dist ';
	RAISE NOTICE 'Statement 1: %', var_stmt;
	/** FOR var_redge IN
		SELECT * 
		FROM (SELECT DISTINCT ON(fullname)  foo.fullname, foo.stusps, foo.zip, 
			   (SELECT z.place FROM zip_state_loc AS z WHERE z.zip = foo.zip and z.statefp = foo.statefp LIMIT 1) As place, foo.center_pt,
			  side, to_number(fromhn, '999999') As fromhn, to_number(tohn, '999999') As tohn, ST_GeometryN(ST_Multi(line),1) As line, foo.dist
		FROM 
		  (SELECT e.the_geom As line, e.fullname, a.zip, s.abbrev As stusps, ST_ClosestPoint(e.the_geom, var_pt) As center_pt, e.statefp, a.side, a.fromhn, a.tohn, ST_Distance_Sphere(e.the_geom, var_pt) As dist
				FROM (SELECT * FROM edges WHERE statefp = var_state AND countyfp = var_countyfp ) AS e INNER JOIN (SELECT * FROM state_lookup WHERE statefp = var_state ) As s ON (e.statefp = s.statefp )
					INNER JOIN (SELECT * FROM faces WHERE statefp = var_state AND countyfp = var_countyfp ) As fl ON (e.tfidl = fl.tfid AND e.statefp = fl.statefp)
					INNER JOIN (SELECT * FROM faces WHERE statefp = var_state AND countyfp = var_countyfp ) As fr ON (e.tfidr = fr.tfid AND e.statefp = fr.statefp)
					INNER JOIN (SELECT * FROM addr WHERE statefp = var_state ) As a ON ( e.tlid = a.tlid AND e.statefp = a.statefp AND  
					   ( ( ST_Covers(fl.the_geom, var_pt) AND a.side = 'L') OR ( ST_Covers(fr.the_geom, var_pt) AND a.side = 'R' ) ) )
					-- INNER JOIN zip_state_loc As z ON (a.statefp =  z.statefp AND a.zip = z.zip) 
				WHERE ST_DWithin(e.the_geom, var_pt, 0.005)
				ORDER BY ST_Distance(e.the_geom, var_pt) LIMIT 4) As foo 
				WHERE dist < 150 --less than 150 m
				ORDER BY foo.fullname, foo.dist) As f ORDER BY f.dist LOOP **/
	FOR var_redge IN EXECUTE var_stmt LOOP
		IF var_debug THEN
			RAISE NOTICE 'Get matching edges loop: %,%', var_primary_line, clock_timestamp();
		END IF;
	   IF var_primary_line IS NULL THEN --this is the first time in the loop and our primary guess
			var_primary_line := var_redge.line;
			var_primary_dist := var_redge.dist;
	   END IF;
	   -- We only consider other edges as matches if they intersect our primary edge -- that would mean we are at a corner place
	   IF ST_Intersects(var_redge.line, var_primary_line) THEN
		   intpt := array_append(intpt,var_redge.center_pt); 
		   IF var_redge.fullname IS NOT NULL THEN
				street := array_append(street, (CASE WHEN include_strnum_range THEN COALESCE(var_redge.fromhn::varchar, '')::varchar || ' - ' || COALESCE(var_redge.tohn::varchar,'')::varchar || ' '::varchar  ELSE '' END::varchar ||  var_redge.fullname::varchar)::varchar);
				--interploate the number -- note that if fromhn > tohn we will be subtracting which is what we want
				-- We only consider differential distances are reeally close from our primary pt
				IF var_redge.dist < var_primary_dist*1.1 THEN 
					var_nstrnum := (var_redge.fromhn + ST_Line_Locate_Point(var_redge.line, var_pt)*(var_redge.tohn - var_redge.fromhn))::numeric(10);
					-- The odd even street number side of street rule
					IF (var_nstrnum  % 2)  != (var_redge.tohn % 2) THEN
						var_nstrnum := CASE WHEN var_nstrnum + 1 NOT BETWEEN var_redge.fromhn AND var_redge.tohn THEN var_nstrnum - 1 ELSE var_nstrnum + 1 END;
					END IF;
					var_strnum := var_nstrnum::varchar;
					var_addy := normalize_address( COALESCE(var_strnum::varchar || ' ', '') || var_redge.fullname || ', ' || var_redge.place || ', ' || var_redge.stusps || ' ' || var_redge.zip);
					addy := array_append(addy, var_addy);
				END IF;
		   END IF;
		END IF;
	END LOOP;
  
	IF var_debug THEN
		RAISE NOTICE 'Get matching edges loop: %', clock_timestamp();
	END IF;
          		
	RETURN;   
END;
$$
  LANGUAGE plpgsql STABLE COST 1000;