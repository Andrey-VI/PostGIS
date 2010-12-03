/**********************************************************************
 * $Id: lwgeom_geos.c 5258 2010-02-17 21:02:49Z strk $
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 *
 * Copyright (C) 2010 Sandro Santilli <strk@keybit.net>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************
 *
 * ST_Snap
 *
 * Snap a geometry to another with a given tolerance
 *
 **********************************************************************/

#include "lwgeom_geos.h"

#include <string.h>
#include <assert.h>

/* #define POSTGIS_DEBUG_LEVEL 4 */

Datum ST_Snap(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(ST_Snap);
Datum ST_Snap(PG_FUNCTION_ARGS)
{
#if POSTGIS_GEOS_VERSION < 33
	lwerror("The GEOS version this postgis binary "
	        "was compiled against (%d) doesn't support "
	        "'ST_Snap' function (3.3.0+ required)",
	        POSTGIS_GEOS_VERSION);
	PG_RETURN_NULL();
#else /* POSTGIS_GEOS_VERSION >= 33 */
	PG_LWGEOM *geom1, *geom2, *out;
	GEOSGeometry *g1, *g2, *g3;
	int is3d;
	int srid;
	double tolerance;

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	tolerance = PG_GETARG_FLOAT8(2);

	is3d = ( TYPE_HASZ(geom1->type) ) ||
	       ( TYPE_HASZ(geom2->type) );

	srid = pglwgeom_get_srid(geom1);
	error_if_srid_mismatch(srid, pglwgeom_get_srid(geom2));

	initGEOS(lwnotice, lwgeom_geos_error);

	g1 = (GEOSGeometry *)POSTGIS2GEOS(geom1);
	if ( 0 == g1 )   /* exception thrown at construction */
	{
		lwerror("First argument geometry could not be converted to GEOS: %s", lwgeom_geos_errmsg);
		PG_FREE_IF_COPY(geom1, 0);
		PG_RETURN_NULL();
	}
	PG_FREE_IF_COPY(geom1, 0);

	g2 = (GEOSGeometry *)POSTGIS2GEOS(geom2);
	if ( 0 == g2 )   /* exception thrown at construction */
	{
		lwerror("Second argument geometry could not be converted to GEOS: %s", lwgeom_geos_errmsg);
		GEOSGeom_destroy(g1);
		PG_FREE_IF_COPY(geom2, 1);
		PG_RETURN_NULL();
	}
	PG_FREE_IF_COPY(geom2, 1);

	g3 = GEOSSnap(g1, g2, tolerance);
	if (g3 == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		lwerror("GEOSIntersection: %s", lwgeom_geos_errmsg);
		PG_RETURN_NULL(); /* never get here */
	}

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	GEOSSetSRID(g3, srid);
	out = GEOS2POSTGIS(g3, is3d);
	if (out == NULL)
	{
		GEOSGeom_destroy(g3);
		lwerror("GEOSSharedPaths() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}
	GEOSGeom_destroy(g3);

	PG_RETURN_POINTER(out);

#endif /* POSTGIS_GEOS_VERSION >= 33 */

}

