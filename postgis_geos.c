/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2001-2003 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************
 * $Log$
 * Revision 1.38  2004/07/28 16:10:59  strk
 * Changed all version functions to return text.
 * Renamed postgis_scripts_version() to postgis_scripts_installed()
 * Added postgis_scripts_released().
 * Added postgis_full_version().
 *
 * Revision 1.37  2004/07/28 13:37:43  strk
 * Added postgis_uses_stats and postgis_scripts_version.
 * Experimented with PIP short-circuit in within/contains functions.
 * Documented new version functions.
 *
 * Revision 1.36  2004/07/27 17:51:50  strk
 * short-circuit test for 'contains'
 *
 * Revision 1.35  2004/07/27 17:49:59  strk
 * Added short-circuit test for the within function.
 *
 * Revision 1.34  2004/07/22 16:58:08  strk
 * Updated to reflect geos version string split.
 *
 * Revision 1.33  2004/07/22 16:20:10  strk
 * Added postgis_lib_version() and postgis_geos_version()
 *
 * Revision 1.32  2004/07/01 17:02:05  strk
 * Updated to support latest GEOS (actually removed all geos-version related
 * switches).
 * Fixed an access to unallocated memory.
 *
 * Revision 1.31  2004/06/16 19:59:36  strk
 * Changed GEOS_VERSION to POSTGIS_GEOS_VERSION to avoid future clashes
 *
 * Revision 1.30  2004/06/16 19:37:54  strk
 * Added cleanup needed for GEOS > 1.0
 *
 * Revision 1.29  2004/06/16 18:47:59  strk
 * Added code to detect geos version.
 * Added appropriate includes in geos connectors.
 *
 * Revision 1.28  2004/04/28 22:26:02  pramsey
 * Fixed spelling mistake in header text.
 *
 * Revision 1.27  2004/02/12 10:34:49  strk
 * changed USE_GEOS check from ifdef / ifndef to if / if !
 *
 * Revision 1.26  2003/12/12 12:03:29  strk
 * More debugging output, some code cleanup.
 *
 * Revision 1.25  2003/12/12 10:28:50  strk
 * added GEOSnoop OUTPUT debugging info
 *
 * Revision 1.24  2003/12/12 10:08:24  strk
 * Added GEOSnoop function and some optional debugging output for
 * geos<->postgis converter (define DEBUG_CONVERTER at top postgis_geos.c)
 *
 * Revision 1.23  2003/11/05 18:25:08  strk
 * moved #ifdef USE_GEOS below prototypes, added NULL implementation of unite_garray
 *
 * Revision 1.22  2003/11/05 18:02:41  strk
 * renamed unite_finalfunc to unite_garray
 *
 * Revision 1.21  2003/11/04 19:06:08  strk
 * added missing first geom pfree in unite_finalfunc
 *
 * Revision 1.20  2003/10/29 15:53:10  strk
 * geoscentroid() removed. both geos and pgis versions are called 'centroid'.
 * only one version will be compiled based on USE_GEOS flag.
 *
 * Revision 1.19  2003/10/29 13:59:40  strk
 * Added geoscentroid function.
 *
 * Revision 1.18  2003/10/28 15:16:17  strk
 * unite_sfunc() from postgis_geos.c renamed to geom_accum() and moved in postgis_fn.c
 *
 * Revision 1.17  2003/10/28 10:59:55  strk
 * handled NULL state array in unite_finalfunc, cleaned up some spurios code
 *
 * Revision 1.16  2003/10/27 23:44:54  strk
 * unite_sfunc made always copy input array in long lived memory context.
 * It should now work with safer memory.
 *
 * Revision 1.15  2003/10/27 20:13:05  strk
 * Made GeomUnion release memory soon, Added fastunion support functions
 *
 * Revision 1.14  2003/10/24 21:52:35  strk
 * Modified strcmp-based if-else with switch-case in GEOS2POSTGIS()
 * using new GEOSGeometryTypeId() interface.
 *
 * Revision 1.13  2003/10/24 08:28:49  strk
 * Fixed memory leak in GEOSGetCoordinates(), made sure that GEOS2POSTGIS
 * free type string even in case of collapsed geoms. Made sure that geomunion
 * release memory in case of exception thrown by GEOSUnion. Sooner release
 * of palloced memory in PolyFromGeometry (pts_per_ring).
 *
 * Revision 1.12  2003/10/16 20:16:18  dblasby
 * Added NOTICE_HANDLER function.  For some reason this didnt get properly
 * committed last time.
 *
 * Revision 1.11  2003/10/14 23:19:19  dblasby
 * GEOS2POSTGIS - added protection to pfree(NULL) for multi* geoms
 *
 * Revision 1.10  2003/10/03 16:45:37  dblasby
 * added pointonsurface() as a sub.  Some memory management fixes/tests.
 * removed a few NOTICEs.
 *
 * Revision 1.9  2003/09/16 20:27:12  dblasby
 * added ability to delete geometries.
 *
 * Revision 1.8  2003/08/08 18:19:20  dblasby
 * Conformance changes.
 * Removed junk from postgis_debug.c and added the first run of the long
 * transaction locking support.  (this will change - dont use it)
 * conformance tests were corrected
 * some dos cr/lf removed
 * empty geometries i.e. GEOMETRYCOLLECT(EMPTY) added (with indexing support)
 * pointN(<linestring>,1) now returns the first point (used to return 2nd)
 *
 * Revision 1.7  2003/08/06 19:31:18  dblasby
 * Added the WKB parser.  Added all the functions like
 * PolyFromWKB(<WKB>,[<SRID>]).
 *
 * Added all the functions like PolyFromText(<WKT>,[<SRID>])
 *
 * Minor problem in GEOS library fixed.
 *
 * Revision 1.6  2003/08/05 18:27:21  dblasby
 * Added null implementations of new GEOS-returning-geometry functions (ie.
 * buffer).
 *
 * Revision 1.5  2003/08/01 23:58:08  dblasby
 * added the functionality to convert GEOS->PostGIS geometries.  Added those geos
 * functions to postgis.
 *
 * Revision 1.4  2003/07/01 18:30:55  pramsey
 * Added CVS revision headers.
 *
 *
 **********************************************************************/


//--------------------------------------------------------------------------
//
//#define DEBUG

#include "postgres.h"


#include <math.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "fmgr.h"

#include "postgis.h"
#include "utils/elog.h"
#include "utils/array.h"

#include "access/gist.h"
#include "access/itup.h"
#include "access/rtree.h"

#include "utils/builtins.h"

//#include "postgis_geos_version.h"

 /*
  * Define this to have have many notices printed
  * during postgis->geos and geos->postgis conversions
  */
#undef DEBUG_CONVERTER
#undef DEBUG_POSTGIS2GEOS
#undef DEBUG_GEOS2POSTGIS


typedef  struct Geometry Geometry;


extern const char * createGEOSPoint(POINT3D *pt);
extern void initGEOS(int maxalign);
extern char *GEOSrelate(Geometry *g1, Geometry*g2);
extern char GEOSrelatePattern(Geometry *g1, Geometry*g2,char *pat);
extern  char GEOSrelateDisjoint(Geometry *g1, Geometry*g2);
extern  char GEOSrelateTouches(Geometry *g1, Geometry*g2);
extern  char GEOSrelateIntersects(Geometry *g1, Geometry*g2);
extern  char GEOSrelateCrosses(Geometry *g1, Geometry*g2);
extern  char GEOSrelateWithin(Geometry *g1, Geometry*g2);
extern  char GEOSrelateContains(Geometry *g1, Geometry*g2);
extern  char GEOSrelateOverlaps(Geometry *g1, Geometry*g2);

extern char *GEOSasText(Geometry *g1);
extern char GEOSisEmpty(Geometry *g1);
extern char *GEOSGeometryType(Geometry *g1);
extern int GEOSGeometryTypeId(Geometry *g1);
extern char *GEOSversion();
extern char *GEOSjtsport();


extern void GEOSdeleteChar(char *a);
extern void GEOSdeleteGeometry(Geometry *a);

extern  Geometry *PostGIS2GEOS_point(POINT3D *point,int SRID, bool is3d);
extern  Geometry *PostGIS2GEOS_linestring(LINE3D *line,int SRID, bool is3d);
extern  Geometry *PostGIS2GEOS_polygon(POLYGON3D *polygon,int SRID, bool is3d);
extern  Geometry *PostGIS2GEOS_multipolygon(POLYGON3D **polygons,int npolys, int SRID, bool is3d);
extern Geometry *PostGIS2GEOS_multilinestring(LINE3D **lines,int nlines, int SRID, bool is3d);
extern Geometry *PostGIS2GEOS_multipoint(POINT3D **points,int npoints, int SRID, bool is3d);
extern Geometry *PostGIS2GEOS_box3d(BOX3D *box, int SRID);
extern Geometry *PostGIS2GEOS_collection(Geometry **geoms, int ngeoms,int SRID, bool is3d);

extern char GEOSisvalid(Geometry *g1);



extern char *throw_exception(Geometry *g);
extern Geometry *GEOSIntersection(Geometry *g1, Geometry *g2);


extern POINT3D  *GEOSGetCoordinate(Geometry *g1);
extern POINT3D  *GEOSGetCoordinates(Geometry *g1);
extern int      GEOSGetNumCoordinate(Geometry *g1);
extern Geometry	*GEOSGetGeometryN(Geometry *g1, int n);
extern Geometry	*GEOSGetExteriorRing(Geometry *g1);
extern Geometry	*GEOSGetInteriorRingN(Geometry *g1,int n);
extern int	GEOSGetNumInteriorRings(Geometry *g1);
extern int      GEOSGetSRID(Geometry *g1);
extern int      GEOSGetNumGeometries(Geometry *g1);

extern  Geometry *GEOSBuffer(Geometry *g1,double width);
extern  Geometry *GEOSConvexHull(Geometry *g1);
extern  Geometry *GEOSDifference(Geometry *g1,Geometry *g2);
extern  Geometry *GEOSBoundary(Geometry *g1);
extern  Geometry *GEOSSymDifference(Geometry *g1,Geometry *g2);
extern  Geometry *GEOSUnion(Geometry *g1,Geometry *g2);
extern   char GEOSequals(Geometry *g1, Geometry*g2);

extern  char GEOSisSimple(Geometry *g1);
extern char GEOSisRing(Geometry *g1);

extern Geometry *GEOSpointonSurface(Geometry *g1);
extern Geometry *GEOSGetCentroid(Geometry *g);


Datum relate_full(PG_FUNCTION_ARGS);
Datum relate_pattern(PG_FUNCTION_ARGS);
Datum disjoint(PG_FUNCTION_ARGS);
Datum touches(PG_FUNCTION_ARGS);
Datum intersects(PG_FUNCTION_ARGS);
Datum crosses(PG_FUNCTION_ARGS);
Datum within(PG_FUNCTION_ARGS);
Datum contains(PG_FUNCTION_ARGS);
Datum overlaps(PG_FUNCTION_ARGS);
Datum isvalid(PG_FUNCTION_ARGS);


Datum buffer(PG_FUNCTION_ARGS);
Datum intersection(PG_FUNCTION_ARGS);
Datum convexhull(PG_FUNCTION_ARGS);
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


Geometry *POSTGIS2GEOS(GEOMETRY *g);
void errorIfGeometryCollection(GEOMETRY *g1, GEOMETRY *g2);
GEOMETRY *GEOS2POSTGIS(Geometry *g, char want3d );

POLYGON3D *PolyFromGeometry(Geometry *g, int *size);
LINE3D *LineFromGeometry(Geometry *g, int *size);
void NOTICE_MESSAGE(char *msg);

#if USE_GEOS

//-----------------------------------------------
// return a GEOS Geometry from a POSTGIS GEOMETRY
//----------------------------------------------




void NOTICE_MESSAGE(char *msg)
{
	elog(NOTICE,msg);
}


/*
 * Resize an ArrayType of 1 dimension to contain num (Pointer *) elements
 * array will be repalloc'ed
 */
static ArrayType *
resize_ptrArrayType(ArrayType *a, int num)
{
	int nelems = ArrayGetNItems(ARR_NDIM(a), ARR_DIMS(a));
	int nbytes = ARR_OVERHEAD(1) + sizeof(Pointer *) * num;

	if (num == nelems) return a;

	a = (ArrayType *) repalloc(a, nbytes);

	a->size = nbytes;
	a->ndim = 1;

	*((int *) ARR_DIMS(a)) = num;
	return a;
}


/*
 * This is the final function for union/fastunite/geomunion
 * aggregate (still discussing the name). Will have
 * as input an array of Geometry pointers.
 * Will iteratively call GEOSUnion on the GEOS-converted
 * versions of them and return PGIS-converted version back.
 * Changing combination order *might* speed up performance.
 *
 * Geometries in the array are pfree'd as soon as possible.
 *
 */
PG_FUNCTION_INFO_V1(unite_garray);
Datum unite_garray(PG_FUNCTION_ARGS)
{
	Datum datum;
	ArrayType *array;
	int is3d = 0;
	int nelems, i;
	GEOMETRY **geoms, *result, *pgis_geom;
	Geometry *g1, *g2, *geos_result;
#ifdef DEBUG
	static int call=1;
#endif

#ifdef DEBUG
	call++;
#endif

	datum = PG_GETARG_DATUM(0);

	/* Null array, null geometry (should be empty?) */
	if ( (Pointer *)datum == NULL ) PG_RETURN_NULL();

	array = (ArrayType *) PG_DETOAST_DATUM(datum);

	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));

#ifdef DEBUG
	elog(NOTICE, "unite_garray: number of elements: %d", nelems);
#endif

	if ( nelems == 0 ) PG_RETURN_NULL();

	geoms = (GEOMETRY **)ARR_DATA_PTR(array);

	/* One-element union is the element itself */
	if ( nelems == 1 ) PG_RETURN_POINTER(geoms[0]);

	/* Ok, we really need geos now ;) */
	initGEOS(MAXIMUM_ALIGNOF);

	if ( geoms[0]->is3d ) is3d = 1;
	geos_result = POSTGIS2GEOS(geoms[0]);
	pfree(geoms[0]);
	for (i=1; i<nelems; i++)
	{
		pgis_geom = geoms[i];
		
		g1 = POSTGIS2GEOS(pgis_geom);
		/*
		 * If we free this memory now we'll have
		 * more space for the growing result geometry.
		 * We don't need it anyway.
		 */
		pfree(pgis_geom);

#ifdef DEBUG
		elog(NOTICE, "unite_garray(%d): adding geom %d to union",
				call, i);
#endif

		g2 = GEOSUnion(g1,geos_result);
		if ( g2 == NULL )
		{
			GEOSdeleteGeometry(g1);
			GEOSdeleteGeometry(geos_result);
			elog(ERROR,"GEOS union() threw an error!");
		}
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(geos_result);
		geos_result = g2;
	}

	result = GEOS2POSTGIS(geos_result, is3d);
	GEOSdeleteGeometry(geos_result);
	if ( result == NULL )
	{
		elog(ERROR, "GEOS2POSTGIS returned an error");
		PG_RETURN_NULL(); //never get here
	}

	compressType(result);

	PG_RETURN_POINTER(result);

}


//select geomunion('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))');
PG_FUNCTION_INFO_V1(geomunion);
Datum geomunion(PG_FUNCTION_ARGS)
{
	GEOMETRY *geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY *geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	int is3d = geom1->is3d || geom2->is3d;

	Geometry *g1,*g2,*g3;
	GEOMETRY *result;

	initGEOS(MAXIMUM_ALIGNOF);
//elog(NOTICE,"in geomunion");

	g1 = POSTGIS2GEOS(geom1);
	g2 = POSTGIS2GEOS(geom2);

//elog(NOTICE,"g1=%s",GEOSasText(g1));
//elog(NOTICE,"g2=%s",GEOSasText(g2));
	g3 = GEOSUnion(g1,g2);

//elog(NOTICE,"g3=%s",GEOSasText(g3));

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS union() threw an error!");
		PG_RETURN_NULL(); //never get here
	}

//elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, is3d);

	GEOSdeleteGeometry(g3);

	if (result == NULL)
	{
		elog(ERROR,"GEOS union() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}

	compressType(result);  // convert multi* to single item if appropriate
	PG_RETURN_POINTER(result);
}


// select symdifference('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))');
PG_FUNCTION_INFO_V1(symdifference);
Datum symdifference(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
		GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

		Geometry *g1,*g2,*g3;
		GEOMETRY *result;

		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );
		g2 = 	POSTGIS2GEOS(geom2 );
		g3 =    GEOSSymDifference(g1,g2);

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS symdifference() threw an error!");
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		PG_RETURN_NULL(); //never get here
	}

//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d || geom2->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS symdifference() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}



		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		GEOSdeleteGeometry(g3);

		compressType(result);  // convert multi* to single item if appropriate

		PG_RETURN_POINTER(result);
}


PG_FUNCTION_INFO_V1(boundary);
Datum boundary(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

		Geometry *g1,*g3;
		GEOMETRY *result;
		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );
		g3 =    GEOSBoundary(g1);

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS bounary() threw an error!");
		GEOSdeleteGeometry(g1);
		PG_RETURN_NULL(); //never get here
	}

//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);

		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS bounary() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}



		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);

		compressType(result);  // convert multi* to single item if appropriate

		PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(convexhull);
Datum convexhull(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
		Geometry *g1,*g3;
		GEOMETRY *result;

		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );
		g3 =    GEOSConvexHull(g1);


	if (g3 == NULL)
	{
		elog(ERROR,"GEOS convexhull() threw an error!");
		GEOSdeleteGeometry(g1);
		PG_RETURN_NULL(); //never get here
	}


//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS convexhull() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);


		compressType(result);  // convert multi* to single item if appropriate

		PG_RETURN_POINTER(result);

}

PG_FUNCTION_INFO_V1(buffer);
Datum buffer(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
		double			size   = PG_GETARG_FLOAT8(1);
		Geometry *g1,*g3;
		GEOMETRY *result;

		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );
		g3 =    GEOSBuffer(g1,size);


	if (g3 == NULL)
	{
		elog(ERROR,"GEOS buffer() threw an error!");
		GEOSdeleteGeometry(g1);
		PG_RETURN_NULL(); //never get here
	}


//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS buffer() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);


		compressType(result);  // convert multi* to single item if appropriate
		PG_RETURN_POINTER(result);

}


//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((25 5, 35 5, 35 7, 25 7, 25 5))');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POINT(5 5)');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','LINESTRING(5 5, 10 10)');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','MULTIPOINT(5 5, 7 7, 9 9, 10 10, 11 11)');
//select intersection('POLYGON(( 0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((5 5, 15 5, 15 7, 5 7, 5 5 ),(6 6,6.5 6, 6.5 6.5,6 6.5,6 6))');


////select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','MULTIPOINT(5 5, 7 7, 9 9, 10 10, 11 11)');
// select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','MULTILINESTRING((5 5, 10 10),(1 1, 2 2) )');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','MULTILINESTRING((5 5, 10 10),(1 1, 2 2) )');
//select intersection('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','MULTIPOLYGON(((5 5, 15 5, 15 7, 5 7, 5 5)),((1 1,1 2,2 2,1 2, 1 1)))');
PG_FUNCTION_INFO_V1(intersection);
Datum intersection(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
		GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

		Geometry *g1,*g2,*g3;
		GEOMETRY *result;

		initGEOS(MAXIMUM_ALIGNOF);

//elog(NOTICE,"intersection() START");

		g1 = 	POSTGIS2GEOS(geom1 );
		g2 = 	POSTGIS2GEOS(geom2 );

//elog(NOTICE,"               constructed geometrys - calling geos");

//elog(NOTICE,"g1 = %s",GEOSasText(g1));
//elog(NOTICE,"g2 = %s",GEOSasText(g2));


//if (g1==NULL)
//	elog(NOTICE,"g1 is null");
//if (g2==NULL)
//	elog(NOTICE,"g2 is null");
//elog(NOTICE,"g2 is valid = %i",GEOSisvalid(g2));
//elog(NOTICE,"g1 is valid = %i",GEOSisvalid(g1));


		g3 =    GEOSIntersection(g1,g2);

//elog(NOTICE,"               intersection finished");

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS Intersection() threw an error!");
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		PG_RETURN_NULL(); //never get here
	}




//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d || geom2->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS Intersection() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}



		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		GEOSdeleteGeometry(g3);

		compressType(result);  // convert multi* to single item if appropriate

		PG_RETURN_POINTER(result);
}

//select difference('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))','POLYGON((5 5, 15 5, 15 7, 5 7, 5 5))');
PG_FUNCTION_INFO_V1(difference);
Datum difference(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
		GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

		Geometry *g1,*g2,*g3;
		GEOMETRY *result;

		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );
		g2 = 	POSTGIS2GEOS(geom2 );
		g3 =    GEOSDifference(g1,g2);

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS difference() threw an error!");
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		PG_RETURN_NULL(); //never get here
	}




//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d || geom2->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS difference() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}



		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g2);
		GEOSdeleteGeometry(g3);


		compressType(result);  // convert multi* to single item if appropriate

		PG_RETURN_POINTER(result);
}


//select pointonsurface('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))');
PG_FUNCTION_INFO_V1(pointonsurface);
Datum pointonsurface(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

		Geometry *g1,*g3;
		GEOMETRY *result;

		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );
		g3 =    GEOSpointonSurface(g1);

	if (g3 == NULL)
	{
		elog(ERROR,"GEOS pointonsurface() threw an error!");
		GEOSdeleteGeometry(g1);
		PG_RETURN_NULL(); //never get here
	}




//	elog(NOTICE,"result: %s", GEOSasText(g3) ) ;

	result = GEOS2POSTGIS(g3, geom1->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);
		elog(ERROR,"GEOS pointonsurface() threw an error (result postgis geometry formation)!");
		PG_RETURN_NULL(); //never get here
	}



		GEOSdeleteGeometry(g1);
		GEOSdeleteGeometry(g3);

		compressType(result);  // convert multi* to single item if appropriate

		PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(centroid);
Datum centroid(PG_FUNCTION_ARGS)
{
	GEOMETRY *geom, *result;
	Geometry *geosgeom, *geosresult;

	geom = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	initGEOS(MAXIMUM_ALIGNOF);

	geosgeom = POSTGIS2GEOS(geom);

	geosresult = GEOSGetCentroid(geosgeom);
	if ( geosresult == NULL )
	{
		GEOSdeleteGeometry(geosgeom);
		elog(ERROR,"GEOS getCentroid() threw an error!");
		PG_RETURN_NULL(); 
	}

	result = GEOS2POSTGIS(geosresult, geom->is3d);
	if (result == NULL)
	{
		GEOSdeleteGeometry(geosgeom);
		GEOSdeleteGeometry(geosresult);
		elog(ERROR,"Error in GEOS-PGIS conversion");
		PG_RETURN_NULL(); 
	}
	GEOSdeleteGeometry(geosgeom);
	GEOSdeleteGeometry(geosresult);

	PG_RETURN_POINTER(result);
}



//----------------------------------------------



void errorIfGeometryCollection(GEOMETRY *g1, GEOMETRY *g2)
{
	if (  (g1->type == COLLECTIONTYPE) || (g2->type == COLLECTIONTYPE) )
		elog(ERROR,"Relate Operation called with a GEOMETRYCOLLECTION type.  This is unsupported");
}

PG_FUNCTION_INFO_V1(isvalid);
Datum isvalid(PG_FUNCTION_ARGS)
{
		GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
		bool result;
		Geometry *g1;

		initGEOS(MAXIMUM_ALIGNOF);

		g1 = 	POSTGIS2GEOS(geom1 );

		result = GEOSisvalid(g1);
		GEOSdeleteGeometry(g1);
	if (result == 2)
	{
		elog(ERROR,"GEOS isvalid() threw an error!");
		PG_RETURN_NULL(); //never get here
	}


	PG_RETURN_BOOL(result);
}


// overlaps(GEOMETRY g1,GEOMETRY g2)
// returns  if GEOS::g1->overlaps(g2) returns true
// throws an error (elog(ERROR,...)) if GEOS throws an error
PG_FUNCTION_INFO_V1(overlaps);
Datum overlaps(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));


	Geometry *g1,*g2;
	bool result;

	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );



	result = GEOSrelateOverlaps(g1,g2);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);
	if (result == 2)
	{
		elog(ERROR,"GEOS overlaps() threw an error!");
		PG_RETURN_NULL(); //never get here
	}

	PG_RETURN_BOOL(result);
}



PG_FUNCTION_INFO_V1(contains);
Datum contains(PG_FUNCTION_ARGS)
{
	GEOMETRY *geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY *geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	Geometry *g1,*g2;
	bool result;
	//POINT3D *testpoint;
	//POLYGON3D *poly;

	errorIfGeometryCollection(geom1,geom2);

	/*
	 * short-circuit 1: if geom2 bounding box is not completely inside
	 * geom1 bounding box we can prematurely return FALSE
	 */
	if ( geom2->bvol.LLB.x < geom1->bvol.LLB.x ) PG_RETURN_BOOL(FALSE);
	if ( geom2->bvol.URT.x > geom1->bvol.URT.x ) PG_RETURN_BOOL(FALSE);
	if ( geom2->bvol.LLB.y < geom1->bvol.LLB.y ) PG_RETURN_BOOL(FALSE);
	if ( geom2->bvol.URT.y > geom1->bvol.URT.y ) PG_RETURN_BOOL(FALSE);

	/*
	 * short-circuit 2: if geom1 is a polygon and any corner of
	 * geom2 bounding box is not 'within' geom1 we can prematurely
	 * return FALSE
	 */
	//if ( geom1->type == POLYGONTYPE )
	//{
	//	poly = (POLYGON3D *)geom1->objData;
	//	testpoint.x = geom2->bvol.LLB.x;
	//	testpoint.y = geom2->bvol.LLB.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//	testpoint.x = geom2->bvol.LLB.x;
	//	testpoint.y = geom2->bvol.URT.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//	testpoint.x = geom2->bvol.URT.x;
	//	testpoint.y = geom2->bvol.URT.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//	testpoint.x = geom2->bvol.URT.x;
	//	testpoint.y = geom2->bvol.LLB.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//}

	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );



	result = GEOSrelateContains(g1,g2);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS contains() threw an error!");
		PG_RETURN_NULL(); //never get here
	}



	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(within);
Datum within(PG_FUNCTION_ARGS)
{
	GEOMETRY *geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY *geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	Geometry *g1,*g2;
	bool result;
	//POINT3D testpoint;
	//POLYGON3D *poly;

	errorIfGeometryCollection(geom1,geom2);


	/*
	 * short-circuit 1: if geom1 bounding box is not completely inside
	 * geom2 bounding box we can prematurely return FALSE
	 */
	if ( geom1->bvol.LLB.x < geom2->bvol.LLB.x ) PG_RETURN_BOOL(FALSE);
	if ( geom1->bvol.URT.x > geom2->bvol.URT.x ) PG_RETURN_BOOL(FALSE);
	if ( geom1->bvol.LLB.y < geom2->bvol.LLB.y ) PG_RETURN_BOOL(FALSE);
	if ( geom1->bvol.URT.y > geom2->bvol.URT.y ) PG_RETURN_BOOL(FALSE);

#if 1
	/*
	 * short-circuit 2: if geom2 is a polygon and any corner of
	 * geom1 bounding box is not 'within' geom2 we can prematurely
	 * return FALSE
	 */
	//if ( geom2->type == POLYGONTYPE )
	//{
	//	poly = (POLYGON3D *)geom2->objData;
	//	testpoint.x = geom1->bvol.LLB.x;
	//	testpoint.y = geom1->bvol.LLB.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//	testpoint.x = geom1->bvol.LLB.x;
	//	testpoint.y = geom1->bvol.URT.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//	testpoint.x = geom1->bvol.URT.x;
	//	testpoint.y = geom1->bvol.URT.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//	testpoint.x = geom1->bvol.URT.x;
	//	testpoint.y = geom1->bvol.LLB.y;
	//	if ( !point_within_polygon(&testpoint, poly) )
	//		PG_RETURN_BOOL(FALSE);
	//}
#endif

	initGEOS(MAXIMUM_ALIGNOF);
	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );

	result = GEOSrelateWithin(g1,g2);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS within() threw an error!");
		PG_RETURN_NULL(); //never get here
	}



	PG_RETURN_BOOL(result);
}



PG_FUNCTION_INFO_V1(crosses);
Datum crosses(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));


	Geometry *g1,*g2;
	bool result;

	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );



	result = GEOSrelateCrosses(g1,g2);

	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS crosses() threw an error!");
		PG_RETURN_NULL(); //never get here
	}



	PG_RETURN_BOOL(result);
}



PG_FUNCTION_INFO_V1(intersects);
Datum intersects(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	Geometry *g1,*g2;
	bool result;




	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );



	result = GEOSrelateIntersects(g1,g2);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);
	if (result == 2)
	{
		elog(ERROR,"GEOS intersects() threw an error!");
		PG_RETURN_NULL(); //never get here
	}



	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(touches);
Datum touches(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));


	Geometry *g1,*g2;
	bool result;

	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );



	result = GEOSrelateTouches(g1,g2);

	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS touches() threw an error!");
		PG_RETURN_NULL(); //never get here
	}



	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(disjoint);
Datum disjoint(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));


	Geometry *g1,*g2;
	bool result;

	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );


	result = GEOSrelateDisjoint(g1,g2);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS disjoin() threw an error!");
		PG_RETURN_NULL(); //never get here
	}

	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(relate_pattern);
Datum relate_pattern(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	char *patt;
	bool result;

	Geometry *g1,*g2;


	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );


	patt =  DatumGetCString(DirectFunctionCall1(textout,
                        PointerGetDatum(PG_GETARG_DATUM(2))));

	result = GEOSrelatePattern(g1,g2,patt);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);
	pfree(patt);

	if (result == 2)
	{
		elog(ERROR,"GEOS relate_pattern() threw an error!");
		PG_RETURN_NULL(); //never get here
	}

	PG_RETURN_BOOL(result);


}



PG_FUNCTION_INFO_V1(relate_full);
Datum relate_full(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	Geometry *g1,*g2;
	char	*relate_str;
	int len;
	char *result;

//elog(NOTICE,"in relate_full()");

	errorIfGeometryCollection(geom1,geom2);


	initGEOS(MAXIMUM_ALIGNOF);

//elog(NOTICE,"GEOS init()");

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );

//elog(NOTICE,"constructed geometries ");





if ((g1==NULL) || (g2 == NULL))
	elog(NOTICE,"g1 or g2 are null");

//elog(NOTICE,GEOSasText(g1));
//elog(NOTICE,GEOSasText(g2));

//elog(NOTICE,"valid g1 = %i", GEOSisvalid(g1));
//elog(NOTICE,"valid g2 = %i",GEOSisvalid(g2));

//elog(NOTICE,"about to relate()");


		relate_str = GEOSrelate(g1, g2);

//elog(NOTICE,"finished relate()");

	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);



	if (relate_str == NULL)
	{
		//free(relate_str);
		elog(ERROR,"GEOS relate() threw an error!");
		PG_RETURN_NULL(); //never get here
	}


	len = strlen(relate_str) + 4;

	result= palloc(len);
	*((int *) result) = len;

	memcpy(result +4, relate_str, len-4);

	free(relate_str);


	PG_RETURN_POINTER(result);
}

//==============================

PG_FUNCTION_INFO_V1(geomequals);
Datum geomequals(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom1 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	GEOMETRY		*geom2 = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(1));


	Geometry *g1,*g2;
	bool result;

	errorIfGeometryCollection(geom1,geom2);
	initGEOS(MAXIMUM_ALIGNOF);

	g1 = 	POSTGIS2GEOS(geom1 );
	g2 = 	POSTGIS2GEOS(geom2 );


	result = GEOSequals(g1,g2);
	GEOSdeleteGeometry(g1);
	GEOSdeleteGeometry(g2);

	if (result == 2)
	{
		elog(ERROR,"GEOS equals() threw an error!");
		PG_RETURN_NULL(); //never get here
	}

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(issimple);
Datum issimple(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	Geometry *g1;
	int result;

	if (geom->nobjs == 0)
		PG_RETURN_BOOL(true);

		initGEOS(MAXIMUM_ALIGNOF);

	//elog(NOTICE,"GEOS init()");

		g1 = 	POSTGIS2GEOS(geom );

		result = GEOSisSimple(g1);
		GEOSdeleteGeometry(g1);


			if (result == 2)
			{
				elog(ERROR,"GEOS issimple() threw an error!");
				PG_RETURN_NULL(); //never get here
			}

	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(isring);
Datum isring(PG_FUNCTION_ARGS)
{
	GEOMETRY		*geom = (GEOMETRY *)  PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	Geometry *g1;
	int result;

	if (geom->type != LINETYPE)
	{
		elog(ERROR,"isring() should only be called on a LINE");
	}

	if (geom->nobjs == 0)
		PG_RETURN_BOOL(false);

		initGEOS(MAXIMUM_ALIGNOF);

	//elog(NOTICE,"GEOS init()");

		g1 = 	POSTGIS2GEOS(geom );

		result = GEOSisRing(g1);
		GEOSdeleteGeometry(g1);


			if (result == 2)
			{
				elog(ERROR,"GEOS isring() threw an error!");
				PG_RETURN_NULL(); //never get here
			}

	PG_RETURN_BOOL(result);
}



//===================================================

POLYGON3D *PolyFromGeometry(Geometry *g, int *size)
{

		int ninteriorrings;
		POINT3D *pts;
		int		*pts_per_ring;
		int t;
		POLYGON3D *poly;
		int npoints;

		ninteriorrings = GEOSGetNumInteriorRings(g);

		npoints = 	GEOSGetNumCoordinate(g);
		pts = GEOSGetCoordinates(g);
		if (npoints <3)
		{
			GEOSdeleteChar( (char*) pts);
			return NULL;
		}

		pts_per_ring = palloc(sizeof(int) *  (ninteriorrings+1));
		pts_per_ring[0] = GEOSGetNumCoordinate(GEOSGetExteriorRing(g));

		for (t=0;t<ninteriorrings;t++)
		{

			pts_per_ring[t+1] = GEOSGetNumCoordinate(GEOSGetInteriorRingN(g,t));
		}



		poly = make_polygon( ninteriorrings+1, pts_per_ring,
							pts, GEOSGetNumCoordinate(g), size);



		GEOSdeleteChar( (char*) pts);
		pfree(pts_per_ring);
		return poly;
}



LINE3D *LineFromGeometry(Geometry *g,int *size)
{
//	int t;
		POINT3D *pts = GEOSGetCoordinates(g);

		LINE3D	*line;
		int npoints = GEOSGetNumCoordinate(g);


		if (npoints <2)
		{
			GEOSdeleteChar( (char*) pts);
			return NULL;
		}

		line = make_line(npoints, pts, size);
//elog(NOTICE,"line::");
//for (t=0;t<npoints;t++)
//{
//	elog(NOTICE,"point (in): %g,%g,%g",  pts[t].x,pts[t].y,pts[t].z);
//	elog(NOTICE,"point (line): %g,%g,%g",line->points[t].x,line->points[t].y,line->points[t].z);
//}
		GEOSdeleteChar( (char*) pts);
		return line;
}




GEOMETRY *
GEOS2POSTGIS(Geometry *g,char want3d)
{
	int type = GEOSGeometryTypeId(g) ;
	GEOMETRY *result = NULL;
	POINT3D *pts;
	LINE3D *line;
	BOX3D *bbox ;
	GEOMETRY *geom, *g2, *r;
	POLYGON3D *poly;
	GEOMETRY *g_new,*g_old;
	int ngeoms,t,size;

	switch (type)
	{
		/* From slower to faster.. compensation rule :) */

		case COLLECTIONTYPE:
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a COLLECTION");
#endif
			//this is more difficult because GEOS allows GCs of GCs
			ngeoms = GEOSGetNumGeometries(g);
			if (ngeoms ==0)
			{
				result =  makeNullGeometry(GEOSGetSRID(g));
				return result;
			}
			if (ngeoms == 1)
			{
				return GEOS2POSTGIS(GEOSGetGeometryN(g,0),
					want3d);  // short cut!
			}
			geom = GEOS2POSTGIS(GEOSGetGeometryN(g,0) , want3d);
			for (t=1;t<ngeoms;t++)
			{
				g2 = GEOS2POSTGIS(GEOSGetGeometryN(g,t),
					want3d);
				r = geom;
#if 1
				geom = (GEOMETRY *) DatumGetPointer(
					DirectFunctionCall2(collector,
					PointerGetDatum(r),
					PointerGetDatum(g2)));
#else
				geom = collector_raw(r, g2);
#endif
				pfree(r);
				pfree(g2);
			}
			return geom;

		case MULTIPOLYGONTYPE:
			ngeoms = 	GEOSGetNumGeometries(g);
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a MULTIPOLYGON");
#endif // DEBUG_GEOS2POSTGIS
			if (ngeoms ==0)
			{
				result =  makeNullGeometry(GEOSGetSRID(g));
				result->type = MULTIPOLYGONTYPE;
				return result;
			}
			for (t=0;t<ngeoms;t++)
			{
				poly = PolyFromGeometry(GEOSGetGeometryN(g,t) ,&size);
				if (t==0)
				{
					result = make_oneobj_geometry(size,
								(char *) poly,
								   MULTIPOLYGONTYPE,  want3d, GEOSGetSRID(g),1.0, 0.0, 0.0
							);
				}
				else
				{
					g_old = result;
					result = 	add_to_geometry(g_old,size, (char*) poly, POLYGONTYPE);
					pfree(g_old);
				}
				pfree(poly);
			}
			// make bounding box
			bbox = bbox_of_geometry( result );
			// copy bounding box
		    	memcpy( &result->bvol, bbox, sizeof(BOX3D) );
			// free bounding box
			pfree( bbox );
			return result;

		case MULTILINETYPE:
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a MULTILINE");
#endif
			ngeoms = GEOSGetNumGeometries(g);
			if (ngeoms ==0)
			{
				result =  makeNullGeometry(GEOSGetSRID(g));
				result->type = MULTILINETYPE;
				return result;
			}

			line = LineFromGeometry(GEOSGetGeometryN(g,0), &size);
			result = make_oneobj_geometry(size, (char *)line,
				MULTILINETYPE,  want3d,
				GEOSGetSRID(g),1.0, 0.0, 0.0);
#ifdef DEBUG_GEOS2POSTGIS
	elog(NOTICE,"GEOS2POSTGIS: t0: %s",geometry_to_text(result));
	elog(NOTICE,"    size = %i", result->size);
#endif

			for (t=1;t<ngeoms;t++)
			{
				line = LineFromGeometry(GEOSGetGeometryN(g,t),
						&size);
				g_old = result;
				result = add_to_geometry(g_old,size,
						(char*) line, LINETYPE);
#ifdef DEBUG_GEOS2POSTGIS
	elog(NOTICE,"GEOS2POSTGIS: t%d: %s", t, geometry_to_text(result));
	elog(NOTICE,"    size = %i", result->size);
#endif
				pfree(g_old);
			}
			bbox = bbox_of_geometry( result ); // make bounding box
			memcpy( &result->bvol, bbox, sizeof(BOX3D) ); // copy bounding box
			pfree( bbox ); // free bounding box
#ifdef DEBUG_GEOS2POSTGIS
	elog(NOTICE,"GEOS2POSTGIS: end: %s",geometry_to_text(result));
	elog(NOTICE,"    size = %i", result->size);
#endif

			return result;

		case MULTIPOINTTYPE:
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a MULTIPOINT");
#endif
			g_new = NULL;
			ngeoms = GEOSGetNumGeometries(g);
			if (ngeoms ==0)
			{
				result =  makeNullGeometry(GEOSGetSRID(g));
				result->type = MULTIPOINTTYPE;
				return result;
			}
			pts = GEOSGetCoordinates(g);
			g_old = make_oneobj_geometry(sizeof(POINT3D),
				(char *) pts, MULTIPOINTTYPE, want3d,
				GEOSGetSRID(g),1.0, 0.0, 0.0);
			for (t=1;t<ngeoms;t++)
			{
				g_new = add_to_geometry(g_old, sizeof(POINT3D),
					(char*)&pts[t], POINTTYPE);
				pfree(g_old);
				g_old =g_new;
			}
			GEOSdeleteChar( (char*) pts);
			// make bounding box
			bbox = bbox_of_geometry( g_new );
			// copy bounding box
			memcpy( &g_new->bvol, bbox, sizeof(BOX3D) );
			// free bounding box
			pfree( bbox );
			return g_new;

		case POLYGONTYPE:
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a POLYGON");
#endif
			poly = PolyFromGeometry(g,&size);
			if (poly == NULL) return NULL;
			result = make_oneobj_geometry(size,
				(char *) poly, POLYGONTYPE, want3d,
				GEOSGetSRID(g),1.0, 0.0, 0.0);
			return result;

		case LINETYPE:
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a LINE");
#endif
			line = LineFromGeometry(g,&size);
			if (line == NULL) return NULL;
			result = make_oneobj_geometry(size,
				(char *) line, LINETYPE, want3d,
				GEOSGetSRID(g),1.0, 0.0, 0.0);
			return result;

		case POINTTYPE: 
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's a POINT");
#endif
			pts = GEOSGetCoordinate(g);
			result = make_oneobj_geometry(sizeof(POINT3D),
				(char *) pts, POINTTYPE, want3d,
				GEOSGetSRID(g), 1.0, 0.0, 0.0);
			GEOSdeleteChar( (char*) pts);
			return result;

		default:
#ifdef DEBUG_GEOS2POSTGIS
			elog(NOTICE, "GEOS2POSTGIS: It's UNKNOWN!");
#endif
			return NULL;

	}
}



//BBOXONLYTYPE -> returns as a 2d polygon
Geometry *
POSTGIS2GEOS(GEOMETRY *g)
{
	POINT3D *pt;
	LINE3D *line;
	POLYGON3D *poly;
	POLYGON3D **polys;
	LINE3D **lines;
	POINT3D **points;
	Geometry **geoms;
	Geometry *geos;
	char     *obj;
	int      obj_type;
	int t;
	Geometry	*result;

	int32 *offsets1 = (int32 *) ( ((char *) &(g->objType[0] ))+ sizeof(int32) * g->nobjs ) ;

	switch(g->type)
	{
		case POINTTYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a POINT");
#endif
			pt = (POINT3D*) ((char *) g +offsets1[0]) ;
			result =  PostGIS2GEOS_point(pt,g->SRID,g->is3d);
			if (result == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return result;
			break;

		case LINETYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a LINE");
#endif
			line = (LINE3D*) ((char *) g +offsets1[0]) ;
			result =  PostGIS2GEOS_linestring(line,g->SRID,g->is3d);
			if (result == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return result;
			break;

		case POLYGONTYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a POLYGON");
#endif
			poly = (POLYGON3D*) ((char *) g +offsets1[0]) ;
			result =  PostGIS2GEOS_polygon(poly,g->SRID,g->is3d);
			if (result == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return result;
			break;

		case MULTIPOLYGONTYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a MULTIPOLYGON");
#endif
			//make an array of POLYGON3Ds
			polys = NULL;
			if (g->nobjs >0)
				polys = (POLYGON3D**) palloc(sizeof (POLYGON3D*) * g->nobjs);
			for (t=0;t<g->nobjs;t++)
			{
				polys[t] = 	(POLYGON3D*) ((char *) g +offsets1[t]) ;
			}
			geos= PostGIS2GEOS_multipolygon(polys, g->nobjs, g->SRID,g->is3d);
			if (polys != NULL) pfree(polys);
			if (geos == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return geos;
			break;

		case MULTILINETYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a MULTILINE");
#endif
			//make an array of LINES3D
			lines = NULL;
			if (g->nobjs >0)
				lines = (LINE3D**) palloc(sizeof (LINE3D*) * g->nobjs);
			for (t=0;t<g->nobjs;t++)
			{
#ifdef DEBUG_POSTGIS2GEOS
		elog(NOTICE, "Line %d has offset %d", t, offsets1[t]);
#endif
				lines[t] = (LINE3D*) ((char *)g+offsets1[t]) ;
			}
			geos= PostGIS2GEOS_multilinestring(lines, g->nobjs, g->SRID,g->is3d);
			if (lines != NULL) pfree(lines);
			if (geos == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return geos;
			break;

		case MULTIPOINTTYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a MULTIPOINT");
#endif
			//make an array of POINT3Ds
			points = NULL;
			if (g->nobjs >0)
				points = (POINT3D**) palloc(sizeof (POINT3D*) * g->nobjs);
			for (t=0;t<g->nobjs;t++)
			{
				points[t] = 	(POINT3D*) ((char *) g +offsets1[t]) ;
			}
			geos= PostGIS2GEOS_multipoint(points, g->nobjs,g->SRID,g->is3d);
			if (points != NULL) pfree(points);
			if (geos == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return geos;
			break;

		case BBOXONLYTYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a BBOXONLY");
#endif
			result =   PostGIS2GEOS_box3d(&g->bvol, g->SRID);
			if (result == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return result;
			break;

		case COLLECTIONTYPE:
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: it's a COLLECTION");
#endif
			//make an array of GEOS Geometries
			geoms = NULL;
			if (g->nobjs >0)
				geoms = (Geometry**) palloc(sizeof (Geometry*) * g->nobjs);
			for (t=0;t<g->nobjs;t++)
			{
				obj = ((char *) g +offsets1[t]);
				obj_type =  g->objType[t];
				switch (obj_type)
				{
					case POINTTYPE:
						pt = (POINT3D*) obj ;
						geoms[t] = PostGIS2GEOS_point(pt,g->SRID,g->is3d);
						if (geoms[t] == NULL)
						{
							pfree(geoms);
							elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
							return NULL;
						}
						break;
					case LINETYPE:
						line = (LINE3D*) obj ;
						geoms[t] = PostGIS2GEOS_linestring(line,g->SRID,g->is3d);
						if (geoms[t] == NULL)
						{
							pfree(geoms);
							elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
							return NULL;
						}
						break;
					case POLYGONTYPE:
						poly = (POLYGON3D*) obj ;
						geoms[t] = PostGIS2GEOS_polygon(poly,g->SRID,g->is3d);
						if (geoms[t] == NULL)
						{
							pfree(geoms);
							elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
							return NULL;
						}
						break;
				}
			}
#ifdef DEBUG_POSTGIS2GEOS
			elog(NOTICE, "POSTGIS2GEOS: COLLECTION has %d objs, srid %d and is %s 3d", g->nobjs, g->SRID, g->is3d ? "" : "not");
#endif
			geos = PostGIS2GEOS_collection(geoms,g->nobjs,g->SRID,g->is3d);
			if (geoms != NULL) pfree(geoms);
			if (geos == NULL)
			{
				elog(ERROR,"Couldnt convert the postgis geometry to GEOS!");
			}
			return geos;
			break;

	}
	return NULL;
}

PG_FUNCTION_INFO_V1(GEOSnoop);
Datum GEOSnoop(PG_FUNCTION_ARGS)
{
	GEOMETRY *geom;
	Geometry *geosgeom;
	GEOMETRY *result;
	
	initGEOS(MAXIMUM_ALIGNOF);

	geom = (GEOMETRY *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
#ifdef DEBUG_CONVERTER
	elog(NOTICE, "GEOSnoop: IN: %s", geometry_to_text(geom));
#endif

	geosgeom = POSTGIS2GEOS(geom);
	if ( (Pointer *)geom != (Pointer *)PG_GETARG_DATUM(0) ) pfree(geom);

	result = GEOS2POSTGIS(geosgeom, geom->is3d);
	GEOSdeleteGeometry(geosgeom);

#ifdef DEBUG_CONVERTER
	elog(NOTICE, "GEOSnoop: OUT: %s", geometry_to_text(result));
#endif

	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(postgis_geos_version);
Datum postgis_geos_version(PG_FUNCTION_ARGS)
{
	char *ver = GEOSversion();
	text *result;
	result = (text *) palloc(VARHDRSZ  + strlen(ver));
	VARATT_SIZEP(result) = VARHDRSZ + strlen(ver) ;
	memcpy(VARDATA(result), ver, strlen(ver));
	free(ver);
	PG_RETURN_POINTER(result);
}

//----------------------------------------------------------------------------
// NULL implementation here
// ---------------------------------------------------------------------------
#else // ndef USE_GEOS


#include "postgres.h"


#include <math.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "fmgr.h"


Datum relate_full(PG_FUNCTION_ARGS);
Datum relate_pattern(PG_FUNCTION_ARGS);
Datum disjoint(PG_FUNCTION_ARGS);
Datum touches(PG_FUNCTION_ARGS);
Datum intersects(PG_FUNCTION_ARGS);
Datum crosses(PG_FUNCTION_ARGS);
Datum within(PG_FUNCTION_ARGS);
Datum contains(PG_FUNCTION_ARGS);
Datum overlaps(PG_FUNCTION_ARGS);
Datum isvalid(PG_FUNCTION_ARGS);


Datum buffer(PG_FUNCTION_ARGS);
Datum intersection(PG_FUNCTION_ARGS);
Datum convexhull(PG_FUNCTION_ARGS);
Datum difference(PG_FUNCTION_ARGS);
Datum boundary(PG_FUNCTION_ARGS);
Datum symdifference(PG_FUNCTION_ARGS);
Datum geomunion(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(intersection);
Datum intersection(PG_FUNCTION_ARGS)
{
	elog(ERROR,"intersection:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(convexhull);
Datum convexhull(PG_FUNCTION_ARGS)
{
	elog(ERROR,"convexhull:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(difference);
Datum difference(PG_FUNCTION_ARGS)
{
	elog(ERROR,"difference:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(boundary);
Datum boundary(PG_FUNCTION_ARGS)
{
	elog(ERROR,"boundary:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(symdifference);
Datum symdifference(PG_FUNCTION_ARGS)
{
	elog(ERROR,"symdifference:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(geomunion);
Datum geomunion(PG_FUNCTION_ARGS)
{
	elog(ERROR,"geomunion:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}

PG_FUNCTION_INFO_V1(buffer);
Datum buffer(PG_FUNCTION_ARGS)
{
	elog(ERROR,"buffer:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}



PG_FUNCTION_INFO_V1(relate_full);
Datum relate_full(PG_FUNCTION_ARGS)
{
	elog(ERROR,"relate_full:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(relate_pattern);
Datum relate_pattern(PG_FUNCTION_ARGS)
{
	elog(ERROR,"relate_pattern:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(disjoint);
Datum disjoint(PG_FUNCTION_ARGS)
{
	elog(ERROR,"disjoint:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(intersects);
Datum intersects(PG_FUNCTION_ARGS)
{
	elog(ERROR,"intersects:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(touches);
Datum touches(PG_FUNCTION_ARGS)
{
	elog(ERROR,"touches:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(crosses);
Datum crosses(PG_FUNCTION_ARGS)
{
	elog(ERROR,"crosses:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(within);
Datum within(PG_FUNCTION_ARGS)
{
	elog(ERROR,"within:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(contains);
Datum contains(PG_FUNCTION_ARGS)
{
	elog(ERROR,"contains:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(overlaps);
Datum overlaps(PG_FUNCTION_ARGS)
{
	elog(ERROR,"overlaps:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(isvalid);
Datum isvalid(PG_FUNCTION_ARGS)
{
	elog(ERROR,"isvalid:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}

PG_FUNCTION_INFO_V1(issimple);
Datum issimple(PG_FUNCTION_ARGS)
{
	elog(ERROR,"issimple:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(geomequals);
Datum geomequals(PG_FUNCTION_ARGS)
{
	elog(ERROR,"geomequals:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}
PG_FUNCTION_INFO_V1(isring);
Datum isring(PG_FUNCTION_ARGS)
{
	elog(ERROR,"isring:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}

PG_FUNCTION_INFO_V1(pointonsurface);
Datum pointonsurface(PG_FUNCTION_ARGS)
{
	elog(ERROR,"pointonsurface:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}

PG_FUNCTION_INFO_V1(unite_garray);
Datum unite_garray(PG_FUNCTION_ARGS)
{
	elog(ERROR,"unite_garray:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL(); // never get here
}

PG_FUNCTION_INFO_V1(GEOSnoop);
Datum GEOSnoop(PG_FUNCTION_ARGS)
{
	elog(ERROR,"GEOSnoop:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(postgis_geos_version);
Datum postgis_geos_version(PG_FUNCTION_ARGS)
{
	//elog(ERROR,"GEOSversion:: operation not implemented - compile PostGIS with GEOS support");
	PG_RETURN_NULL();
}

#endif

