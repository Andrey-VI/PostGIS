#include "../postgis_config.h"

#include "postgres.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "fmgr.h"

#include "lwgeom_pg.h"
#include "liblwgeom.h"
#include "profile.h"

#include "geos_c.h"
#include "lwgeom_rtree.h"

#include <string.h>


#ifdef PROFILE
#warning PROFILE enabled!
#endif

/*
 * Define this to have have many notices printed
 * during postgis->geos and geos->postgis conversions
 *
 */

/*
 *
 * WARNING: buffer-based GeomUnion has been disabled due to
 *          limitations in the GEOS code (it would only work
 *	    against polygons)
 *
 * Fuzzy way of finding out how many points to stuff
 * in each chunk: 680 * Mb of memory
 *
 * The example below is for about 32 MB (fuzzy pragmatic check)
 *
 */
/* #define UNITE_USING_BUFFER 1 */
/* #define MAXGEOMSPOINTS 21760 */

Datum relate_full(PG_FUNCTION_ARGS);
Datum relate_pattern(PG_FUNCTION_ARGS);
Datum disjoint(PG_FUNCTION_ARGS);
Datum touches(PG_FUNCTION_ARGS);
Datum intersects(PG_FUNCTION_ARGS);
Datum crosses(PG_FUNCTION_ARGS);
Datum within(PG_FUNCTION_ARGS);
Datum contains(PG_FUNCTION_ARGS);
Datum covers(PG_FUNCTION_ARGS);
Datum overlaps(PG_FUNCTION_ARGS);
Datum isvalid(PG_FUNCTION_ARGS);
Datum buffer(PG_FUNCTION_ARGS);
Datum intersection(PG_FUNCTION_ARGS);
Datum convexhull(PG_FUNCTION_ARGS);
Datum topologypreservesimplify(PG_FUNCTION_ARGS);
Datum difference(PG_FUNCTION_ARGS);
Datum boundary(PG_FUNCTION_ARGS);
Datum symdifference(PG_FUNCTION_ARGS);
Datum geomunion(PG_FUNCTION_ARGS);
Datum unite_garray(PG_FUNCTION_ARGS);
Datum issimple(PG_FUNCTION_ARGS);
Datum isring(PG_FUNCTION_ARGS);
Datum geomequals(PG_FUNCTION_ARGS);
Datum pointonsurface(PG_FUNCTION_ARGS);
Datum GEOSnoop(PG_FUNCTION_ARGS);
Datum postgis_geos_version(PG_FUNCTION_ARGS);
Datum postgis_jts_version(PG_FUNCTION_ARGS);
Datum centroid(PG_FUNCTION_ARGS);
Datum polygonize_garray(PG_FUNCTION_ARGS);
Datum LWGEOM_buildarea(PG_FUNCTION_ARGS);
Datum linemerge(PG_FUNCTION_ARGS);
Datum coveredby(PG_FUNCTION_ARGS);


LWGEOM *GEOS2LWGEOM(GEOSGeom geom, char want3d);
PG_LWGEOM *GEOS2POSTGIS(GEOSGeom geom, char want3d);
GEOSGeom POSTGIS2GEOS(PG_LWGEOM *g);
GEOSGeom LWGEOM2GEOS(LWGEOM *g);
POINTARRAY *ptarray_from_GEOSCoordSeq(GEOSCoordSeq cs, char want3d);

void errorIfGeometryCollection(PG_LWGEOM *g1, PG_LWGEOM *g2);

PG_FUNCTION_INFO_V1(postgis_geos_version);
Datum postgis_geos_version(PG_FUNCTION_ARGS)
{
	const char *ver = GEOSversion();
	text *result;
	result = (text *) palloc(VARHDRSZ  + strlen(ver));
	SET_VARSIZE(result, VARHDRSZ + strlen(ver));
	memcpy(VARDATA(result), ver, strlen(ver));
	PG_RETURN_POINTER(result);
}



#ifndef UNITE_USING_BUFFER
/*
 * This is the final function for GeomUnion
 * aggregate. Will have as input an array of Geometries.
 * Will iteratively call GEOSUnion on the GEOS-converted
 * versions of them and return PGIS-converted version back.
 * Changing combination order *might* speed up performance.
 */
PG_FUNCTION_INFO_V1(unite_garray);
Datum unite_garray(PG_FUNCTION_ARGS)
{
	Datum datum;
	ArrayType *array;
	int is3d = 0;
	int nelems, i;
	PG_LWGEOM *result, *pgis_geom;
	GEOSGeom g1, g2, geos_result=NULL;
	int SRID=-1;
	size_t offset;
#if POSTGIS_DEBUG_LEVEL > 0
	static int call=1;
#endif

#if POSTGIS_DEBUG_LEVEL >= 2 
	call++;
	POSTGIS_DEBUGF(2, "GEOS incremental union (call %d)", call);
#endif

	datum = PG_GETARG_DATUM(0);

	/* Null array, null geometry (should be empty?) */
	if ( (Pointer *)datum == NULL ) PG_RETURN_NULL();

	array = DatumGetArrayTypeP(datum);

	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));

	POSTGIS_DEBUGF(3, "unite_garray: number of elements: %d", nelems);

	if ( nelems == 0 ) PG_RETURN_NULL();

	/* One-element union is the element itself */
	if ( nelems == 1 ) PG_RETURN_POINTER((PG_LWGEOM *)(ARR_DATA_PTR(array)));

	/* Ok, we really need geos now ;) */
	initGEOS(lwnotice, lwnotice);

	offset = 0;
	for (i=0; i<nelems; i++)
	{
		PG_LWGEOM *geom = (PG_LWGEOM *)(ARR_DATA_PTR(array)+offset);
		offset += INTALIGN(VARSIZE(geom));

		pgis_geom = geom;

		POSTGIS_DEBUGF(3, "geom %d @ %p", i, geom);

		/* Check is3d flag */
		if ( TYPE_HASZ(geom->type) ) is3d = 1;

		/* Check SRID homogeneity and initialize geos result */
		if ( ! i )
		{
			geos_result = POSTGIS2GEOS(geom);
			SRID = pglwgeom_getSRID(geom);

			POSTGIS_DEBUGF(3, "first geom is a %s", lwgeom_typename(TYPE_GETTYPE(geom->type)));

			continue;
		}
		else
		{
			errorIfSRIDMismatch(SRID, pglwgeom_getSRID(geom));
		}
		
		g1 = POSTGIS2GEOS(pgis_geom);

		POSTGIS_DEBUGF(3, "unite_garray(%d): adding geom %d to union (%s)",
				call, i, lwgeom_typename(TYPE_GETTYPE(geom->type)));

		g2 = GEOSUnion(g1,geos_result);
		if ( g2 == NULL )
		{
			GEOSGeom_destroy(g1);
			GEOSGeom_destroy(geos_result);
			elog(ERROR,"GEOS union() threw an error!");
		}
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(geos_result);
		geos_result = g2;
	}

	GEOSSetSRID(geos_result, SRID);
	result = GEOS2POSTGIS(geos_result, is3d);
	GEOSGeom_destroy(geos_result);
	if ( result == NULL )
	{
		elog(ERROR, "GEOS2POSTGIS returned an error");
		PG_RETURN_NULL(); /* never get here */
	}

	/* compressType(result); */

	PG_RETURN_POINTER(result);

}

#else /* def UNITE_USING_BUFFER */

/*
 * This is the final function for GeomUnion
 * aggregate. Will have as input an array of Geometries.
 * Builds a GEOMETRYCOLLECTION from input and call
 * GEOSBuffer(collection, 0) on the GEOS-converted
 * versions of it. Returns PGIS-converted version back.
 */
PG_FUNCTION_INFO_V1(unite_garray);
Datum unite_garray(PG_FUNCTION_ARGS)
{
	Datum datum;
	ArrayType *array;
	int is3d = 0;
	int nelems, i, ngeoms, npoints;
	PG_LWGEOM *result=NULL;
	GEOSGeom *geoms, collection;
	GEOSGeom g1, geos_result=NULL;
	int SRID=-1;
	size_t offset;
#if POSTGIS_DEBUG_LEVEL > 0
	static int call=1;
#endif

#if POSTGIS_DEBUG_LEVEL >= 2 
	call++;
	POSTGIS_DEBUGF(2, "GEOS buffer union (call %d)", call);
#endif

	datum = PG_GETARG_DATUM(0);

	/* Null array, null geometry (should be empty?) */
	if ( (Pointer *)datum == NULL ) PG_RETURN_NULL();

	array = DatumGetArrayTypeP(datum);

	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));

	POSTGIS_DEBUGF(3, "unite_garray: number of elements: %d", nelems);

	if ( nelems == 0 ) PG_RETURN_NULL();

	/* One-element union is the element itself */
	if ( nelems == 1 ) PG_RETURN_POINTER((PG_LWGEOM *)(ARR_DATA_PTR(array)));

	geoms = lwalloc(sizeof(GEOSGeom)*nelems);

	/* We need geos here */
	initGEOS(lwnotice, lwnotice);

	offset = 0; i=0;
	ngeoms = 0; npoints=0;

 	POSTGIS_DEBUGF(3, "Nelems %d, MAXGEOMSPOINST %d", nelems, MAXGEOMSPOINTS);

	while (!result) 
	{
		PG_LWGEOM *geom = (PG_LWGEOM *)(ARR_DATA_PTR(array)+offset);
		offset += INTALIGN(VARSIZE(geom));

		/* Check is3d flag */
		if ( TYPE_HASZ(geom->type) ) is3d = 1;

		/* Check SRID homogeneity  */
		if ( ! i ) SRID = pglwgeom_getSRID(geom);
		else errorIfSRIDMismatch(SRID, pglwgeom_getSRID(geom));

		geoms[ngeoms] = g1 = POSTGIS2GEOS(geom);

		npoints += GEOSGetNumCoordinate(geoms[ngeoms]);

		++ngeoms;
		++i;

		POSTGIS_DEBUGF(4, "Loop %d, npoints: %d", i, npoints);

		/*
		 * Maximum count of geometry points reached
		 * or end of them, collect and buffer(0).
		 */
		if ( (npoints>=MAXGEOMSPOINTS && ngeoms>1) || i==nelems)
		{
			POSTGIS_DEBUGF(4, " CHUNK (ngeoms:%d, npoints:%d, left:%d)",
				ngeoms, npoints, nelems-i);

			collection = GEOSMakeCollection(GEOS_GEOMETRYCOLLECTION,
				geoms, ngeoms);

			geos_result = GEOSBuffer(collection, 0, 0);
			if ( geos_result == NULL )
			{
				GEOSGeom_destroy(g1);
				lwerror("GEOS buffer() threw an error!");
			}
			GEOSGeom_destroy(collection);

			POSTGIS_DEBUG(4, " Buffer() executed");

			/*
			 * If there are no other geoms in input
			 * we've finished, otherwise we push
			 * the result back on the input stack.
			 */
			if ( i == nelems )
			{
				POSTGIS_DEBUGF(4, "  Final result points: %d",
					GEOSGetNumCoordinate(geos_result));

				GEOSSetSRID(geos_result, SRID);
				result = GEOS2POSTGIS(geos_result, is3d);
				GEOSGeom_destroy(geos_result);

				POSTGIS_DEBUG(4, " Result computed");

			}
			else
			{
				geoms[0] = geos_result;
				ngeoms=1;
				npoints = GEOSGetNumCoordinate(geoms[0]);

				POSTGIS_DEBUGF(4, "  Result pushed back on lwgeoms array (npoints:%d)", npoints);
			}
		}
	}


	/* compressType(result); */

	PG_RETURN_POINTER(result);

}

#endif /* def UNITE_USING_BUFFER */


/*
 * select geomunion(
 *      'POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))',
 *      'POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))'
 * );
 *
 */
PG_FUNCTION_INFO_V1(geomunion);
Datum geomunion(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	int is3d;
	int SRID;
	GEOSGeom g1,g2,g3;
	PG_LWGEOM *result;

	POSTGIS_DEBUG(2, "in geomunion");

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	is3d = ( TYPE_HASZ(geom1->type) ) ||
		( TYPE_HASZ(geom2->type) );

	SRID = pglwgeom_getSRID(geom1);
	errorIfSRIDMismatch(SRID, pglwgeom_getSRID(geom2));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

	POSTGIS_DEBUGF(3, "g1=%s", GEOSGeomToWKT(g1));
	POSTGIS_DEBUGF(3, "g2=%s", GEOSGeomToWKT(g2));

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSUnion(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	POSTGIS_DEBUGF(3, "g3=%s", GEOSGeomToWKT(g3));

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS union() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}


	GEOSSetSRID(g3, SRID);

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, is3d);
#ifdef PROFILE
	profstop(PROF_G2P);
#endif

	GEOSGeom_destroy(g3);

	if (result == NULL)
	{
		elog(ERROR,"GEOS union() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /*never get here */
	}

	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_POINTER(result);
}


/*
 *  select symdifference(
 *      'POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))',
 *      'POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))');
 */
PG_FUNCTION_INFO_V1(symdifference);
Datum symdifference(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2,g3;
	PG_LWGEOM *result;
	int is3d;
	int SRID;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	is3d = ( TYPE_HASZ(geom1->type) ) ||
		( TYPE_HASZ(geom2->type) );

	SRID = pglwgeom_getSRID(geom1);
	errorIfSRIDMismatch(SRID, pglwgeom_getSRID(geom2));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSSymDifference(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS symdifference() threw an error!");
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		PG_RETURN_NULL(); /*never get here */
	}

	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3));

	GEOSSetSRID(g3, SRID);

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, is3d);
#ifdef PROFILE
	profstop(PROF_G2P);
#endif

	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS symdifference() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /*never get here */
	}

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	GEOSGeom_destroy(g3);

	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_POINTER(result);
}


PG_FUNCTION_INFO_V1(boundary);
Datum boundary(PG_FUNCTION_ARGS)
{
	PG_LWGEOM	*geom1;
	GEOSGeom g1,g3;
	PG_LWGEOM *result;
	int SRID;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	SRID = pglwgeom_getSRID(geom1);

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1 );
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSBoundary(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS bounary() threw an error!");
		GEOSGeom_destroy(g1);
		PG_RETURN_NULL(); /* never get here */
	}

	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3));

	GEOSSetSRID(g3, SRID);

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, TYPE_HASZ(geom1->type));
#ifdef PROFILE
	profstart(PROF_P2G1);
#endif

	if (result == NULL)
	{
		GEOSGeom_destroy(g1);

		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS boundary() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}



	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g3);

	/* compressType(result);   */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(convexhull);
Datum convexhull(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	GEOSGeom g1, g3;
	PG_LWGEOM *result;
	LWGEOM *lwout;
	int SRID;
	BOX2DFLOAT4 bbox;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	SRID = pglwgeom_getSRID(geom1);

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSConvexHull(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS convexhull() threw an error!");
		GEOSGeom_destroy(g1);
		PG_RETURN_NULL(); /* never get here */
	}


	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3));

	GEOSSetSRID(g3, SRID);

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	lwout = GEOS2LWGEOM(g3, TYPE_HASZ(geom1->type));
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (lwout == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g3);
		elog(ERROR,"convexhull() failed to convert GEOS geometry to LWGEOM");
		PG_RETURN_NULL(); /* never get here */
	}

	/* Copy input bbox if any */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &bbox) )
	{
		lwout->bbox = box2d_clone(&bbox);
	}

	result = pglwgeom_serialize(lwout);
	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS convexhull() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}
	lwgeom_release(lwout);
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g3);


	/* compressType(result);   */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);

}

#if POSTGIS_GEOS_VERSION >= 30 

PG_FUNCTION_INFO_V1(topologypreservesimplify);
Datum topologypreservesimplify(PG_FUNCTION_ARGS)
{
	PG_LWGEOM	*geom1;
	double	tolerance;
	GEOSGeom g1,g3;
	PG_LWGEOM *result;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	tolerance = PG_GETARG_FLOAT8(1);

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSTopologyPreserveSimplify(g1,tolerance);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS topologypreservesimplify() threw an error!");
		GEOSGeom_destroy(g1);
		PG_RETURN_NULL(); /* never get here */
	}


	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3));

	GEOSSetSRID(g3, pglwgeom_getSRID(geom1));

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, TYPE_HASZ(geom1->type));
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS topologypreservesimplify() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g3);


	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}
#endif

PG_FUNCTION_INFO_V1(buffer);
Datum buffer(PG_FUNCTION_ARGS)
{
	PG_LWGEOM	*geom1;
	double	size;
	GEOSGeom g1,g3;
	PG_LWGEOM *result;
	int quadsegs = 8; /* the default */

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	size = PG_GETARG_FLOAT8(1);
	if ( PG_NARGS() > 2 ) quadsegs = PG_GETARG_INT32(2);

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSBuffer(g1,size,quadsegs);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS buffer() threw an error!");
		GEOSGeom_destroy(g1);
		PG_RETURN_NULL(); /* never get here */
	}


	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3));

	GEOSSetSRID(g3, pglwgeom_getSRID(geom1));

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, TYPE_HASZ(geom1->type));
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS buffer() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g3);


	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(intersection);
Datum intersection(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2,g3;
	PG_LWGEOM *result;
	int is3d;
	int SRID;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	is3d = ( TYPE_HASZ(geom1->type) ) ||
		( TYPE_HASZ(geom2->type) );

	SRID = pglwgeom_getSRID(geom1);
	errorIfSRIDMismatch(SRID, pglwgeom_getSRID(geom2));

	initGEOS(lwnotice, lwnotice);

	POSTGIS_DEBUG(3, "intersection() START");

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

	POSTGIS_DEBUG(3, " constructed geometrys - calling geos");
	POSTGIS_DEBUGF(3, " g1 = %s", GEOSGeomToWKT(g1));
	POSTGIS_DEBUGF(3, " g2 = %s", GEOSGeomToWKT(g2));
	/*POSTGIS_DEBUGF(3, "g2 is valid = %i",GEOSisvalid(g2)); */
	/*POSTGIS_DEBUGF(3, "g1 is valid = %i",GEOSisvalid(g1)); */




#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSIntersection(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	POSTGIS_DEBUG(3, " intersection finished");

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS Intersection() threw an error!");
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		PG_RETURN_NULL(); /* never get here */
	}


	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3) ) ;

	GEOSSetSRID(g3, SRID);

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, is3d);
#ifdef PROFILE
	profstop(PROF_G2P);
#endif

	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS Intersection() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}



	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	GEOSGeom_destroy(g3);

	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_POINTER(result);
}

/*
 * select difference(
 *      'POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))',
 *	'POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))');
 */
PG_FUNCTION_INFO_V1(difference);
Datum difference(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2,g3;
	PG_LWGEOM *result;
	int is3d;
	int SRID;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	is3d = ( TYPE_HASZ(geom1->type) ) ||
		( TYPE_HASZ(geom2->type) );

	SRID = pglwgeom_getSRID(geom1);
	errorIfSRIDMismatch(SRID, pglwgeom_getSRID(geom2));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSDifference(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS difference() threw an error!");
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		PG_RETURN_NULL(); /* never get here */
	}

  	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3) ) ;

	GEOSSetSRID(g3, SRID);

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, is3d);
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g2);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS difference() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	GEOSGeom_destroy(g3);

	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_POINTER(result);
}


/* select pointonsurface('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'); */
PG_FUNCTION_INFO_V1(pointonsurface);
Datum pointonsurface(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	GEOSGeom g1,g3;
	PG_LWGEOM *result;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSPointOnSurface(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS pointonsurface() threw an error!");
		GEOSGeom_destroy(g1);
		PG_RETURN_NULL(); /* never get here */
	}

	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3) ) ;

	GEOSSetSRID(g3, pglwgeom_getSRID(geom1));
#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, TYPE_HASZ(geom1->type));
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS pointonsurface() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /* never get here */
	}

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g3);

	/* compressType(result);  */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(centroid);
Datum centroid(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom, *result;
	GEOSGeom geosgeom, geosresult;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	geosgeom = POSTGIS2GEOS(geom);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	geosresult = GEOSGetCentroid(geosgeom);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if ( geosresult == NULL )
	{
		GEOSGeom_destroy(geosgeom);
		elog(ERROR,"GEOS getCentroid() threw an error!");
		PG_RETURN_NULL(); 
	}

	GEOSSetSRID(geosresult, pglwgeom_getSRID(geom));

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(geosresult, TYPE_HASZ(geom->type));
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (result == NULL)
	{
		GEOSGeom_destroy(geosgeom);
		GEOSGeom_destroy(geosresult);
		elog(ERROR,"Error in GEOS-PGIS conversion");
		PG_RETURN_NULL(); 
	}
	GEOSGeom_destroy(geosgeom);
	GEOSGeom_destroy(geosresult);

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom, NULL, result);
#endif

	PG_FREE_IF_COPY(geom, 0);

	PG_RETURN_POINTER(result);
}



/*---------------------------------------------*/



void errorIfGeometryCollection(PG_LWGEOM *g1, PG_LWGEOM *g2)
{
	int t1 = lwgeom_getType(g1->type);
	int t2 = lwgeom_getType(g2->type);

	if (  (t1 == COLLECTIONTYPE) || (t2 == COLLECTIONTYPE) )
		elog(ERROR,"Relate Operation called with a LWGEOMCOLLECTION type.  This is unsupported");
}

PG_FUNCTION_INFO_V1(isvalid);
Datum isvalid(PG_FUNCTION_ARGS)
{
	PG_LWGEOM	*geom1;
	LWGEOM *lwgeom;
	bool result;
	GEOSGeom g1;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	lwgeom = lwgeom_deserialize(SERIALIZED_FORM(geom1));
	if ( ! lwgeom )
	{
		lwerror("unable to deserialize input");
	}
	g1 = LWGEOM2GEOS(lwgeom);
	if ( ! g1 )
	{
		lwgeom_release(lwgeom);
		PG_RETURN_BOOL(FALSE);
	}
	lwgeom_release(lwgeom);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSisValid(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	if (result == 2)
	{
		elog(ERROR,"GEOS isvalid() threw an error!");
		PG_RETURN_NULL(); /*never get here */
	}


#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_BOOL(result);
}


/*
 * overlaps(PG_LWGEOM g1,PG_LWGEOM g2)
 * returns  if GEOS::g1->overlaps(g2) returns true
 * throws an error (elog(ERROR,...)) if GEOS throws an error
 */
PG_FUNCTION_INFO_V1(overlaps);
Datum overlaps(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not overlap
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmin > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin > box2.ymax ) PG_RETURN_BOOL(FALSE);
	}

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSOverlaps(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	if (result == 2)
	{
		elog(ERROR,"GEOS overlaps() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}

int point_in_polygon(RTREE_NODE **root, int ringCount, LWPOINT *point);
int point_in_polygon_deprecated(LWPOLY *polygon, LWPOINT *point);
int point_outside_polygon(RTREE_NODE **root, int ringCount, LWPOINT *point);
int point_outside_polygon_deprecated(LWPOLY *polygon, LWPOINT *point);
int point_in_multipolygon(LWMPOLY *mpolygon, LWPOINT *point);


PG_FUNCTION_INFO_V1(contains);
Datum contains(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;
        int type1, type2;
        LWPOLY *poly;
        /* LWMPOLY *mpoly; */
        LWPOINT *point;
        RTREE_POLY_CACHE *poly_cache;
        MemoryContext old_context;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box is not completely inside
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmin < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmax > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax > box1.ymax ) PG_RETURN_BOOL(FALSE);
	}
        /*
         * short-circuit 2: if geom2 is a point and geom1 is a polygon
         * call the point-in-polygon function.
         */
        type1 = lwgeom_getType((uchar)SERIALIZED_FORM(geom1)[0]);
        type2 = lwgeom_getType((uchar)SERIALIZED_FORM(geom2)[0]);
        if(type1 == POLYGONTYPE && type2 == POINTTYPE)
        {
                POSTGIS_DEBUG(3, "Point in Polygon test requested...short-circuiting.");

                poly = lwpoly_deserialize(SERIALIZED_FORM(geom1));
                point = lwpoint_deserialize(SERIALIZED_FORM(geom2));

                POSTGIS_DEBUGF(3, "Precall point_in_polygon %p, %p", poly, point);

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom1), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

                if(point_in_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0)
                {
	                PG_FREE_IF_COPY(geom1, 0);
	                PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(FALSE);
                }
                else
                {
                        PG_FREE_IF_COPY(geom1, 0);
                        PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(TRUE);
                }
        } 
        /* Not yet functional 
        else if(type1 == MULTIPOLYGONTYPE && type2 == POINTTYPE)
        {
                POSTGIS_DEBUG(3, "Point in MultiPolygon test requested...short-circuiting.");

                mpoly = lwmpoly_deserialize(SERIALIZED_FORM(geom1));
                point = lwpoint_deserialize(SERIALIZED_FORM(geom2));
                if(point_in_multipolygon(mpoly, point) == 0)
                {
                        PG_FREE_IF_COPY(geom1, 0);
                        PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)mpoly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(FALSE);
                }
                else
                {
                        PG_FREE_IF_COPY(geom1, 0);
                        PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)mpoly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(TRUE);
                }
        }
        */
        else 
        {
                POSTGIS_DEBUGF(3, "Contains: type1: %d, type2: %d", type1, type2);
        }
        
	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSContains(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS contains() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("cont",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}


/*
 * Described at 
 * http://lin-ear-th-inking.blogspot.com/2007/06/subtleties-of-ogc-covers-spatial.html
 */
PG_FUNCTION_INFO_V1(covers);
Datum covers(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;
        int type1, type2;
        LWPOLY *poly;
        /* LWMPOLY *mpoly; */
        LWPOINT *point;
        RTREE_POLY_CACHE *poly_cache;
        MemoryContext old_context;
        char *patt = "******FF*";

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box is not completely inside
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmin < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmax > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax > box1.ymax ) PG_RETURN_BOOL(FALSE);
	}
        /*
         * short-circuit 2: if geom2 is a point and geom1 is a polygon
         * call the point-in-polygon function.
         */
        type1 = lwgeom_getType((uchar)SERIALIZED_FORM(geom1)[0]);
        type2 = lwgeom_getType((uchar)SERIALIZED_FORM(geom2)[0]);
        if(type1 == POLYGONTYPE && type2 == POINTTYPE)
        {
                POSTGIS_DEBUG(3, "Point in Polygon test requested...short-circuiting.");

                poly = lwpoly_deserialize(SERIALIZED_FORM(geom1));
                point = lwpoint_deserialize(SERIALIZED_FORM(geom2));

                POSTGIS_DEBUGF(3, "Precall point_in_polygon %p, %p", poly, point);

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom1), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

                if(point_outside_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0)
                {
	                PG_FREE_IF_COPY(geom1, 0);
	                PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(TRUE);
                }
                else
                {
                        PG_FREE_IF_COPY(geom1, 0);
                        PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(FALSE);
                }
        } 
        else 
        {
                POSTGIS_DEBUGF(3, "Covers: type1: %d, type2: %d", type1, type2);
        }
        
	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSRelatePattern(g1,g2,patt);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS covers() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(within);
Datum within(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;
        LWPOLY *poly;
        LWPOINT *point;
        int type1, type2;
        MemoryContext old_context;
        RTREE_POLY_CACHE *poly_cache;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom1 bounding box is not completely inside
	 * geom2 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box1.xmin < box2.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box1.xmax > box2.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box1.ymin < box2.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box1.ymax > box2.ymax ) PG_RETURN_BOOL(FALSE);
	}
        /*
         * short-circuit 2: if geom1 is a point and geom2 is a polygon
         * call the point-in-polygon function.
         */
        type1 = lwgeom_getType((uchar)SERIALIZED_FORM(geom1)[0]);
        type2 = lwgeom_getType((uchar)SERIALIZED_FORM(geom2)[0]);
        if(type1 == POINTTYPE && type2 == POLYGONTYPE)
        {
                POSTGIS_DEBUG(3, "Point in Polygon test requested...short-circuiting.");

                point = lwpoint_deserialize(SERIALIZED_FORM(geom1));
                poly = lwpoly_deserialize(SERIALIZED_FORM(geom2));

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom2), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

                if(point_in_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0)
                {
	                PG_FREE_IF_COPY(geom1, 0);
	                PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(FALSE);
                }
                else
                {
                        PG_FREE_IF_COPY(geom1, 0);
                        PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(TRUE);
                }
        }

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSWithin(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS within() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}


/*
 * Described at:
 * http://lin-ear-th-inking.blogspot.com/2007/06/subtleties-of-ogc-covers-spatial.html
 */
PG_FUNCTION_INFO_V1(coveredby);
Datum coveredby(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;
        LWPOLY *poly;
        LWPOINT *point;
        int type1, type2;
        MemoryContext old_context;
        RTREE_POLY_CACHE *poly_cache;
        char *patt = "**F**F***";

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom1 bounding box is not completely inside
	 * geom2 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box1.xmin < box2.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box1.xmax > box2.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box1.ymin < box2.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box1.ymax > box2.ymax ) PG_RETURN_BOOL(FALSE);

                POSTGIS_DEBUG(3, "bounding box short-circuit missed.");
	}
        /*
         * short-circuit 2: if geom1 is a point and geom2 is a polygon
         * call the point-in-polygon function.
         */
        type1 = lwgeom_getType((uchar)SERIALIZED_FORM(geom1)[0]);
        type2 = lwgeom_getType((uchar)SERIALIZED_FORM(geom2)[0]);
        if(type1 == POINTTYPE && type2 == POLYGONTYPE)
        {
                POSTGIS_DEBUG(3, "Point in Polygon test requested...short-circuiting.");

                point = lwpoint_deserialize(SERIALIZED_FORM(geom1));
                poly = lwpoly_deserialize(SERIALIZED_FORM(geom2));

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom2), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

                if(point_outside_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0)
                {
	                PG_FREE_IF_COPY(geom1, 0);
	                PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(TRUE);
                }
                else
                {
                        PG_FREE_IF_COPY(geom1, 0);
                        PG_FREE_IF_COPY(geom2, 1);
                        lwgeom_release((LWGEOM *)poly);
                        lwgeom_release((LWGEOM *)point);
                        PG_RETURN_BOOL(FALSE);
                }
        }

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSRelatePattern(g1,g2,patt);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS coveredby() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}



PG_FUNCTION_INFO_V1(crosses);
Datum crosses(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not overlap
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmin > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin > box2.ymax ) PG_RETURN_BOOL(FALSE);
	}

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSCrosses(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS crosses() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}



PG_FUNCTION_INFO_V1(intersects);
Datum intersects(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;
	int type1, type2;
	LWPOINT *point;
	LWPOLY *poly;
        MemoryContext old_context;
        RTREE_POLY_CACHE *poly_cache;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not overlap
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmin > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin > box2.ymax ) PG_RETURN_BOOL(FALSE);
	}

	/*
	 * short-circuit 2: if the geoms are a point and a polygon,
	 * call the point_outside_polygon function.
	 */
	type1 = lwgeom_getType((uchar)SERIALIZED_FORM(geom1)[0]);
	type2 = lwgeom_getType((uchar)SERIALIZED_FORM(geom2)[0]);
	if(type1 == POINTTYPE && type2 == POLYGONTYPE)
	{
		POSTGIS_DEBUG(3, "Point in Polygon test requested...short-circuiting.");

		point = lwpoint_deserialize(SERIALIZED_FORM(geom1));
		poly = lwpoly_deserialize(SERIALIZED_FORM(geom2));

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom2), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

		if(point_outside_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0)
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 1);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(TRUE);
		}
		else
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 1);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(FALSE);
		}
	}
	else if(type1 == POLYGONTYPE && type2 == POINTTYPE)
	{
		POSTGIS_DEBUG(3, "Point in Polygon test requested...short-circuiting.");

		point = lwpoint_deserialize(SERIALIZED_FORM(geom2));
		poly = lwpoly_deserialize(SERIALIZED_FORM(geom1));

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom1), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

		if(point_outside_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0)
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 1);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(TRUE);
		}
		else
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 1);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(FALSE);
		}

	}


	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1 );
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2 );
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSIntersects(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	if (result == 2)
	{
		elog(ERROR,"GEOS intersects() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("intr",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(touches);
Datum touches(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not overlap
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmin > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin > box2.ymax ) PG_RETURN_BOOL(FALSE);
	}

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1 );
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2 );
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSTouches(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS touches() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(disjoint);
Datum disjoint(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;
	int type1, type2;
	LWPOLY *poly;
	LWPOINT *point;
        MemoryContext old_context;
        RTREE_POLY_CACHE *poly_cache;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not overlap
	 * geom1 bounding box we can prematurely return TRUE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax < box1.xmin ) PG_RETURN_BOOL(TRUE);
		if ( box2.xmin > box1.xmax ) PG_RETURN_BOOL(TRUE);
		if ( box2.ymax < box1.ymin ) PG_RETURN_BOOL(TRUE);
		if ( box2.ymin > box1.ymax ) PG_RETURN_BOOL(TRUE);
	}

	/*
	 * short-circuit 2: if the geoms are a point and a polygon, 
	 * call the point_outside_polygon function.
	 */
	type1 = lwgeom_getType((uchar)SERIALIZED_FORM(geom1)[0]);
	type2 = lwgeom_getType((uchar)SERIALIZED_FORM(geom2)[0]);
	if(type1 == POINTTYPE && type2 == POLYGONTYPE)
	{
		POSTGIS_DEBUG(3, "Point outside Polygon test requested...short-circuiting.");

		point = lwpoint_deserialize(SERIALIZED_FORM(geom1));
		poly = lwpoly_deserialize(SERIALIZED_FORM(geom2));

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom2), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

		if(point_outside_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0) 
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 0);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(FALSE);
		}
		else
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 0);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(TRUE);
		}
	}
	else if(type1 == POLYGONTYPE && type2 == POINTTYPE)
	{
		POSTGIS_DEBUG(3, "Point outside Polygon test requested...short-circuiting.");

		point = lwpoint_deserialize(SERIALIZED_FORM(geom2));
		poly = lwpoly_deserialize(SERIALIZED_FORM(geom1));

                /*
                 * Switch the context to the function-scope context,
                 * retrieve the appropriate cache object, cache it for 
                 * future use, then switch back to the local context.
                 */                 
                old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
                poly_cache = retrieveCache(poly, SERIALIZED_FORM(geom2), fcinfo->flinfo->fn_extra);
                fcinfo->flinfo->fn_extra = poly_cache;
                MemoryContextSwitchTo(old_context);

		if(point_outside_polygon(poly_cache->ringIndices, poly_cache->ringCount, point) == 0) 
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 0);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(FALSE);
		}
		else
		{
			PG_FREE_IF_COPY(geom1, 0);
			PG_FREE_IF_COPY(geom2, 0);
			lwgeom_release((LWGEOM *)poly);
			lwgeom_release((LWGEOM *)point);
			PG_RETURN_BOOL(TRUE);
		}
	}

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSDisjoint(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS disjoin() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(relate_pattern);
Datum relate_pattern(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	char *patt;
	bool result;
	GEOSGeom g1,g2;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

	patt =  DatumGetCString(DirectFunctionCall1(textout,
                        PointerGetDatum(PG_GETARG_DATUM(2))));

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSRelatePattern(g1,g2,patt);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);
	pfree(patt);

	if (result == 2)
	{
		elog(ERROR,"GEOS relate_pattern() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}



PG_FUNCTION_INFO_V1(relate_full);
Datum relate_full(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	char *relate_str;
	int len;
	text *result;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	POSTGIS_DEBUG(2, "in relate_full()");

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));


	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1 );
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2 );
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

 	POSTGIS_DEBUG(3, "constructed geometries ");

	if ((g1==NULL) || (g2 == NULL))
		elog(NOTICE,"g1 or g2 are null");

	POSTGIS_DEBUGF(3, "%s", GEOSGeomToWKT(g1));
	POSTGIS_DEBUGF(3, "%s", GEOSGeomToWKT(g2));

	/*POSTGIS_DEBUGF(3, "valid g1 = %i", GEOSisvalid(g1));*/
	/*POSTGIS_DEBUGF(3, "valid g2 = %i",GEOSisvalid(g2));*/

	POSTGIS_DEBUG(3, "about to relate()");


#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	relate_str = GEOSRelate(g1, g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

 	POSTGIS_DEBUG(3, "finished relate()");

	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (relate_str == NULL)
	{
		elog(ERROR,"GEOS relate() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

	len = strlen(relate_str) + VARHDRSZ;

	result= palloc(len);
	SET_VARSIZE(result, len);

	memcpy(VARDATA(result), relate_str, len-VARHDRSZ);

	free(relate_str);

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_POINTER(result);
}

/*============================== */

PG_FUNCTION_INFO_V1(geomequals);
Datum geomequals(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom1;
	PG_LWGEOM *geom2;
	GEOSGeom g1,g2;
	bool result;
	BOX2DFLOAT4 box1, box2;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not equal
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax != box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmin != box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax != box1.ymax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin != box2.ymin ) PG_RETURN_BOOL(FALSE);
	}

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif
#ifdef PROFILE
	profstart(PROF_P2G2);
#endif
	g2 = POSTGIS2GEOS(geom2);
#ifdef PROFILE
	profstop(PROF_P2G2);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSEquals(g1,g2);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS equals() threw an error!");
		PG_RETURN_NULL(); /*never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, geom2, NULL);
#endif

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(issimple);
Datum issimple(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom;
	GEOSGeom g1;
	int result;

	POSTGIS_DEBUG(2, "issimple called");

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	if (lwgeom_getnumgeometries(SERIALIZED_FORM(geom)) == 0)
		PG_RETURN_BOOL(true);

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSisSimple(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif
	GEOSGeom_destroy(g1);

	if (result == 2)
	{
		elog(ERROR,"GEOS issimple() threw an error!");
		PG_RETURN_NULL(); /*never get here */
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom, NULL, NULL);
#endif

	PG_FREE_IF_COPY(geom, 0);

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(isring);
Datum isring(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom;
	GEOSGeom g1;
	int result;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	if (lwgeom_getType(geom->type) != LINETYPE)
	{
		elog(ERROR,"isring() should only be called on a LINE");
	}

	if (lwgeom_getnumgeometries(SERIALIZED_FORM(geom)) == 0)
		PG_RETURN_BOOL(false);

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom );
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	result = GEOSisRing(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif
	GEOSGeom_destroy(g1);

	if (result == 2)
	{
		elog(ERROR,"GEOS isring() threw an error!");
		PG_RETURN_NULL(); 
	}

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom, NULL, NULL);
#endif

	PG_FREE_IF_COPY(geom, 0);

	PG_RETURN_BOOL(result);
}



/*= GEOS <=> POSTGIS CONVERSION ========================= */

/*-----=GEOS2POSTGIS= */

#ifdef WKB_CONVERSION

/* Return an LWGEOM from a GEOSGeom */
LWGEOM *
GEOS2LWGEOM(GEOSGeom geom, char want3d)
{
	size_t size;
	char *wkb;
	LWGEOM *lwgeom;

	if ( want3d ) GEOS_setWKBOutputDims(3);
	else GEOS_setWKBOutputDims(2);

	wkb = GEOSGeomToWKB_buf(geom, &size);
	lwgeom = lwgeom_from_ewkb(wkb, size);

	return lwgeom;
}

PG_LWGEOM *
GEOS2POSTGIS(GEOSGeom geom, char want3d)
{
	size_t size;
	char *wkb;
	PG_LWGEOM *pglwgeom, *ret;

	if ( want3d ) GEOS_setWKBOutputDims(3);
	else GEOS_setWKBOutputDims(2);

	wkb = GEOSGeomToWKB_buf(geom, &size);
	if ( ! wkb )
	{
		lwerror("GEOS failed to export WKB");
	}
	pglwgeom = pglwgeom_from_ewkb(wkb, size);
	if ( ! pglwgeom )
	{
		lwerror("GEOS2POSTGIS: lwgeom_from_ewkb returned NULL");
	}

	if ( is_worth_caching_pglwgeom_bbox(pglwgeom) )
	{
		ret = (PG_LWGEOM *)DatumGetPointer(DirectFunctionCall1(
			LWGEOM_addBBOX, PointerGetDatum(pglwgeom)));
		lwfree(pglwgeom);
	}
	else
	{
		ret = pglwgeom;
	}

	return ret;
}

#else /* !ndef WKB_CONVERSION */

/* Return a POINTARRAY from a GEOSCoordSeq */
POINTARRAY *
ptarray_from_GEOSCoordSeq(GEOSCoordSeq cs, char want3d)
{
	unsigned int dims=2;
	unsigned int size, i, ptsize;
	uchar *points, *ptr;
	POINTARRAY *ret;

	LWDEBUG(2, "ptarray_fromGEOSCoordSeq called");

	if ( ! GEOSCoordSeq_getSize(cs, &size) )
			lwerror("Exception thrown");

	LWDEBUGF(4, " GEOSCoordSeq size: %d", size);

	if ( want3d )
	{
		if ( ! GEOSCoordSeq_getDimensions(cs, &dims) )
			lwerror("Exception thrown");

		LWDEBUGF(4, " GEOSCoordSeq dimensions: %d", dims);

		/* forget higher dimensions (if any) */
		if ( dims > 3 ) dims = 3;
	}

	LWDEBUGF(4, " output dimensions: %d", dims);

	ptsize = sizeof(double)*dims;

	ret = ptarray_construct((dims==3), 0, size);

	points = ret->serialized_pointlist;
	ptr = points;
	for (i=0; i<size; i++)
	{
		POINT3DZ point;
		GEOSCoordSeq_getX(cs, i, &(point.x));
		GEOSCoordSeq_getY(cs, i, &(point.y));
		if ( dims >= 3 ) GEOSCoordSeq_getZ(cs, i, &(point.z));
		memcpy(ptr, &point, ptsize);
		ptr += ptsize;
	}

	return ret;
}

/* Return an LWGEOM from a Geometry */
LWGEOM *
GEOS2LWGEOM(GEOSGeom geom, char want3d)
{
	int type = GEOSGeomTypeId(geom) ;
	bool hasZ = GEOSHasZ(geom);
	int SRID = GEOSGetSRID(geom);

	/* GEOS's 0 is equivalent to our -1 as for SRID values */
	if ( SRID == 0 ) SRID = -1;

	if ( ! hasZ )
	{
		if ( want3d )
		{
			LWDEBUG(3, "Geometry has no Z, won't provide one");

			want3d = 0;
		}
	}

	switch (type)
	{
		GEOSCoordSeq cs;
		POINTARRAY *pa, **ppaa;
		GEOSGeom g;
		LWGEOM **geoms;
		unsigned int i, ngeoms;

		case GEOS_POINT:
			LWDEBUG(4, "lwgeom_from_geometry: it's a Point");

			cs = GEOSGeom_getCoordSeq(geom);
			pa = ptarray_from_GEOSCoordSeq(cs, want3d);
			return (LWGEOM *)lwpoint_construct(SRID, NULL, pa);
			
		case GEOS_LINESTRING:
		case GEOS_LINEARRING:
			LWDEBUG(4, "lwgeom_from_geometry: it's a LineString or LinearRing");

			cs = GEOSGeom_getCoordSeq(geom);
			pa = ptarray_from_GEOSCoordSeq(cs, want3d);
			return (LWGEOM *)lwline_construct(SRID, NULL, pa);

		case GEOS_POLYGON:
			LWDEBUG(4, "lwgeom_from_geometry: it's a Polygon");

			ngeoms = GEOSGetNumInteriorRings(geom);
			ppaa = lwalloc(sizeof(POINTARRAY *)*(ngeoms+1));
			g = GEOSGetExteriorRing(geom);
			cs = GEOSGeom_getCoordSeq(g);
			ppaa[0] = ptarray_from_GEOSCoordSeq(cs, want3d);
			for (i=0; i<ngeoms; i++)
			{
				g = GEOSGetInteriorRingN(geom, i);
				cs = GEOSGeom_getCoordSeq(g);
				ppaa[i+1] = ptarray_from_GEOSCoordSeq(cs,
					want3d);
			}
			return (LWGEOM *)lwpoly_construct(SRID, NULL,
				ngeoms+1, ppaa);

		case GEOS_MULTIPOINT:
		case GEOS_MULTILINESTRING:
		case GEOS_MULTIPOLYGON:
		case GEOS_GEOMETRYCOLLECTION:
			LWDEBUG(4, "lwgeom_from_geometry: it's a Collection or Multi");

			ngeoms = GEOSGetNumGeometries(geom);
			geoms = NULL;
			if ( ngeoms )
			{
				geoms = lwalloc(sizeof(LWGEOM *)*ngeoms);
				for (i=0; i<ngeoms; i++)
				{
					g = GEOSGetGeometryN(geom, i);
					geoms[i] = GEOS2LWGEOM(g, want3d);
				}
			}
			return (LWGEOM *)lwcollection_construct(type,
				SRID, NULL, ngeoms, geoms);

		default:
			lwerror("GEOS2LWGEOM: unknown geometry type: %d", type);
			return NULL;

	}

}


PG_LWGEOM *
GEOS2POSTGIS(GEOSGeom geom, char want3d)
{
	LWGEOM *lwgeom;
	PG_LWGEOM *result;

	lwgeom = GEOS2LWGEOM(geom, want3d);
	if ( ! lwgeom )
	{
		lwerror("GEOS2POSTGIS: GEOS2LWGEOM returned NULL");
		return NULL;
	}

	LWDEBUGF(4, "GEOS2POSTGIS: GEOS2LWGEOM returned a %s", lwgeom_summary(lwgeom, 0)); 

	if ( is_worth_caching_lwgeom_bbox(lwgeom) )
	{
		lwgeom_addBBOX(lwgeom);
	}

	result = pglwgeom_serialize(lwgeom);

	return result;
}

#endif /* def WKB_CONVERSION */

/*-----=POSTGIS2GEOS= */


#ifdef WKB_CONVERSION

GEOSGeom LWGEOM2GEOS(LWGEOM *);

GEOSGeom 
LWGEOM2GEOS(LWGEOM *lwgeom)
{
	size_t size;
	char *wkb;
	GEOSGeom geom;

	wkb = lwgeom_to_ewkb(lwgeom, getMachineEndian(), &size);
	geom = GEOSGeomFromWKB_buf(wkb, size);
	return geom;
}

GEOSGeom 
POSTGIS2GEOS(PG_LWGEOM *pglwgeom)
{
	size_t size;
	char *wkb;
	GEOSGeom geom;

	wkb = pglwgeom_to_ewkb(pglwgeom, getMachineEndian(), &size);
	if ( ! wkb )
	{
		lwerror("Postgis failed to export EWKB %s:%d", __FILE__, __LINE__);
	}
	geom = GEOSGeomFromWKB_buf(wkb, size);
	lwfree(wkb);
	if ( ! geom ) {
		lwerror("POSTGIS2GEOS conversion failed");
	}

#if POSTGIS_DEBUG_LEVEL >= 4
	wkb = GEOSGeomToWKT(geom);
	LWDEBUGF(4, "GEOS geom: %s", wkb);
#endif

	return geom;
}

#else /* ndef WKB_CONVERSION */

GEOSCoordSeq ptarray_to_GEOSCoordSeq(POINTARRAY *);
GEOSGeom LWGEOM2GEOS(LWGEOM *lwgeom);

GEOSCoordSeq
ptarray_to_GEOSCoordSeq(POINTARRAY *pa)
{
	unsigned int dims = 2;
	unsigned int size, i;
	POINT3DZ p;
	GEOSCoordSeq sq;

	if ( TYPE_HASZ(pa->dims) ) dims = 3;
	size = pa->npoints;

	sq = GEOSCoordSeq_create(size, dims);
	if ( ! sq ) lwerror("Error creating GEOS Coordinate Sequence");

	for (i=0; i<size; i++)
	{
		getPoint3dz_p(pa, i, &p);

		LWDEBUGF(4, "Point: %g,%g,%g", p.x, p.y, p.z);

		GEOSCoordSeq_setX(sq, i, p.x);
		GEOSCoordSeq_setY(sq, i, p.y);
		if ( dims == 3 ) GEOSCoordSeq_setZ(sq, i, p.z);
	}
	return sq;
}

GEOSGeom
LWGEOM2GEOS(LWGEOM *lwgeom)
{
	GEOSCoordSeq sq;
	GEOSGeom g, shell, *geoms;
	/*
        LWGEOM *tmp;
	*/
	unsigned int ngeoms, i;
	int type = 0;
	int geostype;
#if POSTGIS_DEBUG_LEVEL >= 4 
	char *wkt;
#endif

	LWDEBUGF(4, "LWGEOM2GEOS got a %s", lwgeom_typename(type));

        if(has_arc(lwgeom))
        {
                LWDEBUG(3, "LWGEOM2GEOS_c: arced geometry found.");

		lwerror("Exception in LWGEOM2GEOS: curved geometry not supported.");
		/*
                tmp = lwgeom;
                lwgeom = lwgeom_segmentize(tmp, 32);
                LWDEBUGF(3, "LWGEOM2GEOM_c: was %p, is %p", tmp, lwgeom);
		*/
        }
        type = TYPE_GETTYPE(lwgeom->type);
	switch (type)
	{
		LWPOINT *lwp;
		LWPOLY *lwpoly;
		LWLINE *lwl;
		LWCOLLECTION *lwc;

		case POINTTYPE:
			lwp = (LWPOINT *)lwgeom;
			sq = ptarray_to_GEOSCoordSeq(lwp->point);
			g = GEOSGeom_createPoint(sq);
			if ( ! g ) lwerror("Exception in LWGEOM2GEOS");
			break;
		case LINETYPE:
			lwl = (LWLINE *)lwgeom;
			sq = ptarray_to_GEOSCoordSeq(lwl->points);
			g = GEOSGeom_createLineString(sq);
			if ( ! g ) lwerror("Exception in LWGEOM2GEOS");
			break;

		case POLYGONTYPE:
			lwpoly = (LWPOLY *)lwgeom;
			sq = ptarray_to_GEOSCoordSeq(lwpoly->rings[0]);
			shell = GEOSGeom_createLinearRing(sq);
	if ( ! shell ) return NULL;
	/*lwerror("LWGEOM2GEOS: exception during polygon shell conversion"); */
			ngeoms = lwpoly->nrings-1;
			geoms = malloc(sizeof(GEOSGeom)*ngeoms);
			for (i=1; i<lwpoly->nrings; ++i)
			{
				sq = ptarray_to_GEOSCoordSeq(lwpoly->rings[i]);
				geoms[i-1] = GEOSGeom_createLinearRing(sq);
	if ( ! geoms[i-1] ) return NULL;
	/*lwerror("LWGEOM2GEOS: exception during polygon hole conversion"); */
			}
			g = GEOSGeom_createPolygon(shell, geoms, ngeoms);
			if ( ! g ) return NULL;
			free(geoms);
			break;
		case MULTIPOINTTYPE:
		case MULTILINETYPE:
		case MULTIPOLYGONTYPE:
		case COLLECTIONTYPE:
			if ( type == MULTIPOINTTYPE )
				geostype = GEOS_MULTIPOINT;
			else if ( type == MULTILINETYPE )
				geostype = GEOS_MULTILINESTRING;
			else if ( type == MULTIPOLYGONTYPE )
				geostype = GEOS_MULTIPOLYGON;
			else
				geostype = GEOS_GEOMETRYCOLLECTION;

			lwc = (LWCOLLECTION *)lwgeom;
			ngeoms = lwc->ngeoms;
			geoms = malloc(sizeof(GEOSGeom)*ngeoms);

			for (i=0; i<ngeoms; ++i)
			{
				geoms[i] = LWGEOM2GEOS(lwc->geoms[i]);
				if ( ! geoms[i] ) return NULL;
			}
			g = GEOSGeom_createCollection(geostype, geoms, ngeoms);
			if ( ! g ) return NULL;
			free(geoms);
			break;

		default:
			lwerror("Unknown geometry type: %d", type);

			return NULL;
	}

	GEOSSetSRID(g, lwgeom->SRID);

#if POSTGIS_DEBUG_LEVEL >= 4 
	wkt = GEOSGeomToWKT(g);
	LWDEBUGF(4, "LWGEOM2GEOS: GEOSGeom: %s", wkt);
	/*
        if(tmp != NULL) lwgeom_release(tmp);
	*/
	free(wkt);
#endif

	return g;
}

GEOSGeom 
POSTGIS2GEOS(PG_LWGEOM *pglwgeom)
{
	GEOSGeom ret;
	LWGEOM *lwgeom = lwgeom_deserialize(SERIALIZED_FORM(pglwgeom));
	if ( ! lwgeom )
	{
		lwerror("POSTGIS2GEOS: unable to deserialize input");
		return NULL;
	}
	ret = LWGEOM2GEOS(lwgeom);
	lwgeom_release(lwgeom);
	if ( ! ret )  {
		lwerror("POSTGIS2GEOS conversion failed");
		return NULL;
	}
	return ret;
}

#endif /* WKB_CONVERSION */

PG_FUNCTION_INFO_V1(GEOSnoop);
Datum GEOSnoop(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom;
	GEOSGeom geosgeom;
	PG_LWGEOM *result;

	initGEOS(lwnotice, lwnotice);

	geom = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	POSTGIS_DEBUGF(2, "GEOSnoop: IN: %s", serialized_lwgeom_to_ewkt(SERIALIZED_FORM(geom)));

	geosgeom = POSTGIS2GEOS(geom);
	if ( ! geosgeom ) PG_RETURN_NULL();

#ifdef PROFILE
	profstart(PROF_GRUN);
	profstop(PROF_GRUN);
#endif

	result = GEOS2POSTGIS(geosgeom, TYPE_HASZ(geom->type));
	GEOSGeom_destroy(geosgeom);

	POSTGIS_DEBUGF(4, "GEOSnoop: OUT: %s", serialized_lwgeom_to_ewkt(SERIALIZED_FORM(result)));

	PG_FREE_IF_COPY(geom, 0);

	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(polygonize_garray);
Datum polygonize_garray(PG_FUNCTION_ARGS)
{
	Datum datum;
	ArrayType *array;
	int is3d = 0;
	unsigned int nelems, i;
	PG_LWGEOM *result;
	GEOSGeom geos_result;
	GEOSGeom *vgeoms;
	int SRID=-1;
	size_t offset;
#if POSTGIS_DEBUG_LEVEL > 0 
	static int call=1;
#endif

#if POSTGIS_DEBUG_LEVEL >= 3 
	call++;
#endif

	datum = PG_GETARG_DATUM(0);

	/* Null array, null geometry (should be empty?) */
	if ( (Pointer *)datum == NULL ) PG_RETURN_NULL();

	array = DatumGetArrayTypeP(datum);

	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));

	POSTGIS_DEBUGF(3, "polygonize_garray: number of elements: %d", nelems);

	if ( nelems == 0 ) PG_RETURN_NULL();

	/* Ok, we really need geos now ;) */
	initGEOS(lwnotice, lwnotice);

	vgeoms = palloc(sizeof(GEOSGeom)*nelems);
	offset = 0;
	for (i=0; i<nelems; i++)
	{
		PG_LWGEOM *geom = (PG_LWGEOM *)(ARR_DATA_PTR(array)+offset);
		offset += INTALIGN(VARSIZE(geom));

		vgeoms[i] = POSTGIS2GEOS(geom);
		if ( ! i )
		{
			SRID = pglwgeom_getSRID(geom);
		}
		else
		{
			if ( SRID != pglwgeom_getSRID(geom) )
			{
	elog(ERROR, "polygonize: operation on mixed SRID geometries");
	PG_RETURN_NULL();
			}
		}
	}

	POSTGIS_DEBUG(3, "polygonize_garray: invoking GEOSpolygonize");

	geos_result = GEOSPolygonize(vgeoms, nelems);

	POSTGIS_DEBUG(3, "polygonize_garray: GEOSpolygonize returned");

	for (i=0; i<nelems; ++i) GEOSGeom_destroy(vgeoms[i]);
	pfree(vgeoms);

	if ( ! geos_result ) PG_RETURN_NULL();

	GEOSSetSRID(geos_result, SRID);
	result = GEOS2POSTGIS(geos_result, is3d);
	GEOSGeom_destroy(geos_result);
	if ( result == NULL )
	{
		elog(ERROR, "GEOS2POSTGIS returned an error");
		PG_RETURN_NULL(); /*never get here */
	}

	/*compressType(result); */

	PG_RETURN_POINTER(result);

}

PG_FUNCTION_INFO_V1(linemerge);
Datum linemerge(PG_FUNCTION_ARGS)
{
	PG_LWGEOM	*geom1;
	GEOSGeom g1,g3;
	PG_LWGEOM *result;

#ifdef PROFILE
	profstart(PROF_QRUN);
#endif

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	initGEOS(lwnotice, lwnotice);

#ifdef PROFILE
	profstart(PROF_P2G1);
#endif
	g1 = POSTGIS2GEOS(geom1);
#ifdef PROFILE
	profstop(PROF_P2G1);
#endif

#ifdef PROFILE
	profstart(PROF_GRUN);
#endif
	g3 = GEOSLineMerge(g1);
#ifdef PROFILE
	profstop(PROF_GRUN);
#endif

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS LineMerge() threw an error!");
		GEOSGeom_destroy(g1);
		PG_RETURN_NULL(); /*never get here */
	}


  	POSTGIS_DEBUGF(3, "result: %s", GEOSGeomToWKT(g3) ) ;

	GEOSSetSRID(g3, pglwgeom_getSRID(geom1));

#ifdef PROFILE
	profstart(PROF_G2P);
#endif
	result = GEOS2POSTGIS(g3, TYPE_HASZ(geom1->type));
#ifdef PROFILE
	profstop(PROF_G2P);
#endif
	if (result == NULL)
	{
		GEOSGeom_destroy(g1);
		GEOSGeom_destroy(g3);
		elog(ERROR,"GEOS LineMerge() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); /*never get here */
	}
	GEOSGeom_destroy(g1);
	GEOSGeom_destroy(g3);


	/* compressType(result); */

#ifdef PROFILE
	profstop(PROF_QRUN);
	profreport("geos",geom1, NULL, result);
#endif

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

Datum JTSnoop(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(JTSnoop);
Datum JTSnoop(PG_FUNCTION_ARGS)
{
	elog(ERROR, "JTS support is disabled");
	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(postgis_jts_version);
Datum postgis_jts_version(PG_FUNCTION_ARGS)
{
	PG_RETURN_NULL();
}

/*
 * Take a geometry and return an areal geometry
 * (Polygon or MultiPolygon).
 * Actually a wrapper around GEOSpolygonize, 
 * transforming the resulting collection into
 * a valid polygonzl Geometry.
 */
PG_FUNCTION_INFO_V1(LWGEOM_buildarea);
Datum LWGEOM_buildarea(PG_FUNCTION_ARGS)
{
	int is3d = 0;
	unsigned int i, ngeoms;
	PG_LWGEOM *result;
	LWGEOM *lwg;
	GEOSGeom geos_result, shp;
	GEOSGeom vgeoms[1];
	int SRID=-1;
#if POSTGIS_DEBUG_LEVEL > 0
	static int call=1;
#endif

	PG_LWGEOM *geom = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

#if POSTGIS_DEBUG_LEVEL >= 3
	call++;
	lwnotice("buildarea called (call %d)", call);
#endif

	SRID = pglwgeom_getSRID(geom);
	is3d = TYPE_HASZ(geom->type);

	POSTGIS_DEBUGF(3, "LWGEOM_buildarea got geom @ %p", geom);

	initGEOS(lwnotice, lwnotice);

	vgeoms[0] = POSTGIS2GEOS(geom);
	geos_result = GEOSPolygonize(vgeoms, 1);
	GEOSGeom_destroy(vgeoms[0]);

	POSTGIS_DEBUGF(3, "GEOSpolygonize returned @ %p", geos_result);

	/* Null return from GEOSpolygonize */
	if ( ! geos_result ) PG_RETURN_NULL();

	/*
	 * We should now have a collection
	 */
#if PARANOIA_LEVEL > 0
	if ( GEOSGeometryTypeId(geos_result) != COLLECTIONTYPE )
	{
		GEOSGeom_destroy(geos_result);
		lwerror("Unexpected return from GEOSpolygonize");
		PG_RETURN_NULL();
	}
#endif

	ngeoms = GEOSGetNumGeometries(geos_result);

	POSTGIS_DEBUGF(3, "GEOSpolygonize: ngeoms in polygonize output: %d", ngeoms);

	/*
	 * No geometries in collection, return NULL
	 */
	if ( ngeoms == 0 ) 
	{
		GEOSGeom_destroy(geos_result);
		PG_RETURN_NULL();
	}

	/*
	 * Return first geometry if we only have one in collection,
	 * to avoid the unnecessary Geometry clone below.
	 */
	if ( ngeoms == 1 )
	{
		shp = GEOSGetGeometryN(geos_result, 0);
		lwg = GEOS2LWGEOM(shp, is3d);
		lwg->SRID = SRID;
		result = pglwgeom_serialize(lwg);
		lwgeom_release(lwg);
		GEOSGeom_destroy(geos_result);
		PG_RETURN_POINTER(result);
	}

	/* 
	 * Iteratively invoke symdifference on outer rings
	 * as suggested by Carl Anderson:
	 * postgis-devel/2005-December/001805.html
	 */
	shp = NULL;
	for (i=0; i<ngeoms; ++i)
	{
		GEOSGeom extring, tmp;
		GEOSCoordSeq sq;

		/*
		 * Construct a Polygon from geometry i exterior ring
		 * We don't use GEOSGeom_clone on the ExteriorRing
		 * due to a bug in CAPI contained in GEOS 2.2 branch
		 * failing to properly return a LinearRing from
		 * a LinearRing clone.
		 */
		sq=GEOSCoordSeq_clone(GEOSGeom_getCoordSeq(
			GEOSGetExteriorRing(GEOSGetGeometryN( geos_result, i))
			));
		extring = GEOSGeom_createPolygon(
				GEOSGeom_createLinearRing(sq),
				NULL, 0
				);

		if ( extring == NULL ) /* exception */
		{
			lwerror("GEOSCreatePolygon threw an exception");
			PG_RETURN_NULL();
		}

		if ( shp == NULL )
		{
			shp = extring;
		}
		else
		{
			tmp = GEOSSymDifference(shp, extring);
			GEOSGeom_destroy(shp);
			GEOSGeom_destroy(extring);
			shp = tmp;
		}
	}

	GEOSGeom_destroy(geos_result);

	GEOSSetSRID(shp, SRID);
	result = GEOS2POSTGIS(shp, is3d);
	GEOSGeom_destroy(shp);

#if PARANOIA_LEVEL > 0
	if ( result == NULL )
	{
		lwerror("serialization error");
		PG_RETURN_NULL(); /*never get here */
	}

#endif

	PG_RETURN_POINTER(result);

}


Datum containsPrepared(PG_FUNCTION_ARGS);
Datum containsProperlyPrepared(PG_FUNCTION_ARGS);
Datum coversPrepared(PG_FUNCTION_ARGS);
Datum intersectsPrepared(PG_FUNCTION_ARGS);

/*
 * GEOS prepared geometry is only available from GEOS 3.1 onwards
 */
#if GEOS_VERNUM >= 31
#define PREPARED_GEOM 1
#endif


#ifdef PREPARED_GEOM

typedef struct
{
	int32					key;
	GEOSPreparedGeometry * 	prepared_geom;
} PREPARED_GEOM_CACHE;


/* 
 * get cached prepared geometry for geom1
 * only geom1 is potentially prepared as only
 * the first arg of the prepared predicates CAN be prepared
 * briefly, the following does:
 *
 * get cache
 * if cache not exist
 *   create cache 
 *   key into cache
 *
 * else if key matches cached key
 *    if cached prepared not exist
 *        geom1 prepared into cache
 *    
 * else
 *   key into cache
 *   clear prepared cache
 */
PREPARED_GEOM_CACHE *
get_prepared_geometry_cache( 
	PREPARED_GEOM_CACHE * 	cache, 
	uchar * 				serialized_geom,
	int32					key )
{
	GEOSGeom g;

	if ( !cache )
	{
		LWDEBUGF(3, "get_prepared_geometry_cache: creating cache: %x", cache);
		
		cache = lwalloc( sizeof( PREPARED_GEOM_CACHE ));
		
		cache->prepared_geom = 0;
		cache->key = key;
	}
	else if ( cache->key == key )
	{
		if ( !cache->prepared_geom )
		{
			LWDEBUGF(3, "get_prepared_geometry_cache: preparing obj");
			
			g = POSTGIS2GEOS( serialized_geom);
			cache->prepared_geom = GEOSPrepare( g);
		}
		else
		{
			LWDEBUGF(3, "get_prepared_geometry_cache: prepared obj in cache");
		}
	}
	else
	{
		LWDEBUG(3, "get_prepared_geometry_cache: obj NOT in cache");

		GEOSPreparedGeom_destroy( cache->prepared_geom);

		cache->prepared_geom = 0;
		cache->key = key;
	}
	
	return cache;
}

#endif /* PREPARED_GEOM */


PG_FUNCTION_INFO_V1(containsPrepared);
Datum containsPrepared(PG_FUNCTION_ARGS)
{
#ifndef PREPARED_GEOM
	elog(ERROR,"Not implemented in this version!");
	PG_RETURN_NULL(); /* never get here */

#else
	PG_LWGEOM *				geom1;
	PG_LWGEOM *				geom2;
	GEOSGeom 				g1, g2;
	GEOSPreparedGeometry * 	pg;
	bool 					result;
	BOX2DFLOAT4 			box1, box2;
	PREPARED_GEOM_CACHE *	prep_cache;
	MemoryContext 			old_context;
	int32					surrogate_key;

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	
	surrogate_key = PG_GETARG_INT32(2);
	
	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit: if geom2 bounding box is not completely inside
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmin < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmax > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax > box1.ymax ) PG_RETURN_BOOL(FALSE);
	}
	
	old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
	prep_cache =  fcinfo->flinfo->fn_extra;

	prep_cache = get_prepared_geometry_cache( prep_cache, geom1, surrogate_key);

	fcinfo->flinfo->fn_extra = prep_cache;
	MemoryContextSwitchTo(old_context);

	initGEOS(lwnotice, lwnotice);

	g2 = POSTGIS2GEOS(geom2);

	if ( !prep_cache || !prep_cache->prepared_geom )
	{
		g1 = POSTGIS2GEOS(geom1);

		result = GEOSContains( g1, g2);
		
		GEOSGeom_destroy(g1);
	}
	else
	{
		pg = prep_cache->prepared_geom;

		result = GEOSPreparedContains( pg, g2);	
	}
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS contains() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
#endif /* PREPARED_GEOM */
}

PG_FUNCTION_INFO_V1(containsProperlyPrepared);
Datum containsProperlyPrepared(PG_FUNCTION_ARGS)
{
#ifndef PREPARED_GEOM
	elog(ERROR,"Not implemented in this version!");
	PG_RETURN_NULL(); /* never get here */

#else
	PG_LWGEOM *				geom1;
	PG_LWGEOM *				geom2;
	GEOSGeom 				g1, g2;
	GEOSPreparedGeometry * 	pg;
	bool 					result;
	BOX2DFLOAT4 			box1, box2;
	PREPARED_GEOM_CACHE *	prep_cache;
	MemoryContext 			old_context;
	int32					surrogate_key;

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	
	surrogate_key = PG_GETARG_INT32(2);
	
	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit: if geom2 bounding box is not completely inside
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmin < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmax > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax > box1.ymax ) PG_RETURN_BOOL(FALSE);
	}
	
	old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
	prep_cache =  fcinfo->flinfo->fn_extra;
	
	prep_cache = get_prepared_geometry_cache( prep_cache, geom1, surrogate_key);
	
	fcinfo->flinfo->fn_extra = prep_cache;
	MemoryContextSwitchTo( old_context);

	initGEOS(lwnotice, lwnotice);
		
	g2 = POSTGIS2GEOS(geom2);

	if ( !prep_cache || !prep_cache->prepared_geom )
	{
		g1 = POSTGIS2GEOS(geom1);
		
		result = GEOSRelatePattern( g1, g2, "T**FF*FF*");
		
		GEOSGeom_destroy(g1);
	}
	else
	{
		pg = prep_cache->prepared_geom;

		result = GEOSPreparedContainsProperly( pg, g2);	
	}
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS contains() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
#endif /* PREPARED_GEOM */
}

PG_FUNCTION_INFO_V1(coversPrepared);
Datum coversPrepared(PG_FUNCTION_ARGS)
{
#ifndef PREPARED_GEOM
	elog(ERROR,"Not implemented in this version!");
	PG_RETURN_NULL(); /* never get here */

#else
	PG_LWGEOM *				geom1;
	PG_LWGEOM *				geom2;
	GEOSGeom 				g1, g2;
	GEOSPreparedGeometry * 	pg;
	bool 					result;
	BOX2DFLOAT4 			box1, box2;
	PREPARED_GEOM_CACHE *	prep_cache;
	MemoryContext 			old_context;
	int32					surrogate_key;

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	
	surrogate_key = PG_GETARG_INT32(2);
	
	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit: if geom2 bounding box is not completely inside
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmin < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmax > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax > box1.ymax ) PG_RETURN_BOOL(FALSE);
	}
	
	old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
	prep_cache =  fcinfo->flinfo->fn_extra;

	prep_cache = get_prepared_geometry_cache( prep_cache, geom1, surrogate_key);

	fcinfo->flinfo->fn_extra = prep_cache;
	MemoryContextSwitchTo(old_context);

	initGEOS(lwnotice, lwnotice);

	g2 = POSTGIS2GEOS(geom2);

	if ( !prep_cache || !prep_cache->prepared_geom )
	{
		g1 = POSTGIS2GEOS(geom1);

		result = GEOSRelatePattern( g1, g2, "******FF*");
		
		GEOSGeom_destroy(g1);
	}
	else
	{
		pg = prep_cache->prepared_geom;

		result = GEOSPreparedCovers( pg, g2);	
	}
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS contains() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
#endif /* PREPARED_GEOM */
}

PG_FUNCTION_INFO_V1(intersectsPrepared);
Datum intersectsPrepared(PG_FUNCTION_ARGS)
{
#ifndef PREPARED_GEOM
	elog(ERROR,"Not implemented in this version!");
	PG_RETURN_NULL(); /* never get here */

#else
	PG_LWGEOM *				geom1;
	PG_LWGEOM *				geom2;
	GEOSGeom 				g1, g2;
	GEOSPreparedGeometry * 	pg;
	bool 					result;
	BOX2DFLOAT4 			box1, box2;
	PREPARED_GEOM_CACHE *	prep_cache;
	MemoryContext 			old_context;
	int32					surrogate_key;

	geom1 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (PG_LWGEOM *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	
	surrogate_key = PG_GETARG_INT32(2);
	
	errorIfGeometryCollection(geom1,geom2);
	errorIfSRIDMismatch(pglwgeom_getSRID(geom1), pglwgeom_getSRID(geom2));

	/*
	 * short-circuit 1: if geom2 bounding box does not overlap
	 * geom1 bounding box we can prematurely return FALSE.
	 * Do the test IFF BOUNDING BOX AVAILABLE.
	 */
	if ( getbox2d_p(SERIALIZED_FORM(geom1), &box1) &&
		getbox2d_p(SERIALIZED_FORM(geom2), &box2) )
	{
		if ( box2.xmax < box1.xmin ) PG_RETURN_BOOL(FALSE);
		if ( box2.xmin > box1.xmax ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymax < box1.ymin ) PG_RETURN_BOOL(FALSE);
		if ( box2.ymin > box2.ymax ) PG_RETURN_BOOL(FALSE);
	}
	
	old_context = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
	prep_cache =  fcinfo->flinfo->fn_extra;

	prep_cache = get_prepared_geometry_cache( prep_cache, geom1, surrogate_key);

	fcinfo->flinfo->fn_extra = prep_cache;
	MemoryContextSwitchTo(old_context);

	initGEOS(lwnotice, lwnotice);

	g2 = POSTGIS2GEOS(geom2);

	if ( !prep_cache || !prep_cache->prepared_geom )
	{
		g1 = POSTGIS2GEOS(geom1);

		result = GEOSIntersects( g1, g2);
		
		GEOSGeom_destroy(g1);
	}
	else
	{
		pg = prep_cache->prepared_geom;

		result = GEOSPreparedIntersects( pg, g2);	
	}
	GEOSGeom_destroy(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS contains() threw an error!");
		PG_RETURN_NULL(); /* never get here */
	}

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
#endif /* PREPARED_GEOM */
}

