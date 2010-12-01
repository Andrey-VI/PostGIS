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
 * ST_SharedPaths
 *
 * Return the set of paths shared between two linear geometries,
 * and their direction (same or opposite).
 *
 * Developed by Sandro Santilli (strk@keybit.net) for Faunalia
 * (http://www.faunalia.it) with funding from Regione Toscana - Sistema
 * Informativo per la Gestione del Territorio e dell' Ambiente
 * [RT-SIGTA]". For the project: "Sviluppo strumenti software per il
 * trattamento di dati geografici basati su QuantumGIS e Postgis (CIG
 * 0494241492)"
 *
 **********************************************************************/

#include "lwgeom_geos.h"
//#include "lwalgorithm.h"
//#include "funcapi.h"

#include <string.h>
#include <assert.h>

/* #define POSTGIS_DEBUG_LEVEL 4 */

Datum ST_SharedPaths(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(ST_SharedPaths);
Datum ST_SharedPaths(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1, *geom2, *out;
	GEOSGeometry *g1, *g2, *g3;
	int is3d;
	int srid;

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

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
		PG_FREE_IF_COPY(geom2, 0);
		PG_RETURN_NULL();
	}
	PG_FREE_IF_COPY(geom2, 0);

	g3 = GEOSSharedPaths(g1,g2);
	if (g3 == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		lwerror("GEOSIntersection: %s", lwgeom_geos_errmsg);
		PG_RETURN_NULL(); /* never get here */
	}

	/* TODO: dump instead ? */

	out = GEOS2POSTGIS(g3, is3d);
	if (out == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		GEOSGeom_destroy(g3);
		lwerror("GEOSSharedPaths() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	GEOSGeom_destroy(g3);

	PG_RETURN_POINTER(out);

}

