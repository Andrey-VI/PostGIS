//  g++ postgis_GEOSwrapper.cpp -c -I/usr/local/include  -I/usr/local/include/geos -I/usr/local/src/postgresql-7.2.3//src/include

/*
* $Log$
* Revision 1.22  2004/07/01 17:02:26  strk
* Updated to support latest GEOS API.
*
* Revision 1.21  2004/06/16 19:59:36  strk
* Changed GEOS_VERSION to POSTGIS_GEOS_VERSION to avoid future clashes
*
* Revision 1.20  2004/06/16 19:37:54  strk
* Added cleanup needed for GEOS > 1.0
*
* Revision 1.19  2004/06/16 18:47:59  strk
* Added code to detect geos version.
* Added appropriate includes in geos connectors.
*
* Revision 1.18  2004/04/27 07:44:26  strk
* Removed use of geometryFactory->toGeometry(), indicated by Martin Davis
* as being intended for internal use only. Created a linear ring instead
* (the function converts a box3d to a geos geometry).
*
* Revision 1.17  2003/12/12 13:34:20  strk
* added missing 'const' in prototypes
*
* Revision 1.16  2003/12/12 12:03:30  strk
* More debugging output, some code cleanup.
*
* Revision 1.15  2003/11/12 16:36:04  strk
* delete all caught exceptions after use
*
* Revision 1.14  2003/10/29 13:58:28  strk
* Added GEOSGetCentroid() function
*
* Revision 1.13  2003/10/24 21:33:21  strk
* Added GEOSGeometryTypeId(Geometry *) wrapper function.
* Added GEOSGetCoordinates_Polygon(Polygon *) as an experimental optimized
* version of GEOSGetCoordinates(Geometry *); More to add ...
*
* Revision 1.12  2003/10/24 14:29:53  strk
* GEOSGetCoordinates() reverted to getCoordinates() call so to be compatible
* to all type of geometries (not only LineStrings)
*
* Revision 1.11  2003/10/24 08:28:50  strk
* Fixed memory leak in GEOSGetCoordinates(), made sure that GEOS2POSTGIS
* free type string even in case of collapsed geoms. Made sure that geomunion
* release memory in case of exception thrown by GEOSUnion. Sooner release
* of palloced memory in PolyFromGeometry (pts_per_ring).
*
* Revision 1.10  2003/10/20 19:50:49  strk
* Removed some memory leaks in PostGIS2* converters.
*
*/


#include <stdio.h>

#include <string>
#include <iostream>
#include <fstream>

#include "geom.h"
#include "util.h"
#include "postgis_geos_version.h"

using namespace geos;



//WARNING THIS *MUST* BE SET CORRECTLY.
int MAXIMUM_ALIGNOF = -999999;    // to be set during initialization - this will be either 4 (intel) or 8 (sparc)

//for getting things to align properly  double are on 8byte align on solaris machines, and 4bytes on intel

#define TYPEALIGN(ALIGNVAL,LEN) (((long)(LEN) + (ALIGNVAL-1)) & ~(ALIGNVAL-1))
#define MAXALIGN(LEN)           TYPEALIGN(MAXIMUM_ALIGNOF, (LEN))

typedef  int int32;

typedef struct
{
	double		x,y,z;  //for lat/long   x=long, y=lat
} POINT3D;

typedef struct
{
	POINT3D		LLB,URT; /* corner POINT3Ds on long diagonal */
} BOX3D;

typedef struct
{
	int32 	npoints; // how many points in the line
	int32 	junk;	   // double-word alignment
	POINT3D  	points[1]; // array of actual points
} LINE3D;


typedef struct
{
	int32 	nrings;	 // how many rings in this polygon
	int32		npoints[1]; //how many points in each ring
	/* could be 4 byes of filler here to make sure points[] is
         double-word aligned*/
	POINT3D  	points[1]; // array of actual points
} POLYGON3D;


#define	POINTTYPE	1
#define	LINETYPE	2
#define	POLYGONTYPE	3
#define	MULTIPOINTTYPE	4
#define	MULTILINETYPE	5
#define	MULTIPOLYGONTYPE	6
#define	COLLECTIONTYPE	7

//###########################################################

extern "C" char *GEOSrelate(Geometry *g1, Geometry*g2);
extern "C" void initGEOS(int maxalign);


extern "C" void GEOSdeleteChar(char *a);
extern "C" void GEOSdeleteGeometry(Geometry *a);
extern "C" char GEOSrelatePattern(Geometry *g1, Geometry*g2,char *pat);
extern "C" char GEOSrelateDisjoint(Geometry *g1, Geometry*g2);
extern "C" char GEOSrelateTouches(Geometry *g1, Geometry*g2);
extern "C" char GEOSrelateIntersects(Geometry *g1, Geometry*g2);
extern "C" char GEOSrelateCrosses(Geometry *g1, Geometry*g2);
extern "C" char GEOSrelateWithin(Geometry *g1, Geometry*g2);
extern "C" char GEOSrelateContains(Geometry *g1, Geometry*g2);
extern "C" char GEOSrelateOverlaps(Geometry *g1, Geometry*g2);


extern "C" Geometry *PostGIS2GEOS_point(POINT3D *point,int SRID, bool is3d);
extern "C" Geometry *PostGIS2GEOS_linestring(const LINE3D *line,int SRID, bool is3d);
extern "C" Geometry *PostGIS2GEOS_polygon(POLYGON3D *polygon,int SRID, bool is3d);
extern "C" Geometry *PostGIS2GEOS_multipolygon(POLYGON3D **polygons,int npolys, int SRID, bool is3d);
extern "C" Geometry *PostGIS2GEOS_multilinestring(const LINE3D **lines,int nlines, int SRID, bool is3d);
extern "C" Geometry *PostGIS2GEOS_multipoint(POINT3D **points,int npoints, int SRID, bool is3d);

extern "C" Geometry *PostGIS2GEOS_box3d(BOX3D *box, int SRID);
extern "C" Geometry *PostGIS2GEOS_collection(Geometry **geoms, int ngeoms,int SRID, bool is3d);

extern "C" char GEOSisvalid(Geometry *g1);


extern "C" char *GEOSasText(Geometry *g1);
extern "C" char GEOSisEmpty(Geometry *g1);
extern "C" char *GEOSGeometryType(Geometry *g1);
extern "C" int GEOSGeometryTypeId(Geometry *g1);


extern "C" char *throw_exception(Geometry *g);

extern "C" Geometry *GEOSIntersection(Geometry *g1,Geometry *g1);
extern "C" Geometry *GEOSBuffer(Geometry *g1,double width);
extern "C" Geometry *GEOSConvexHull(Geometry *g1);
extern "C" Geometry *GEOSDifference(Geometry *g1,Geometry *g2);
extern "C" Geometry *GEOSBoundary(Geometry *g1);
extern "C" Geometry *GEOSSymDifference(Geometry *g1,Geometry *g2);
extern "C" Geometry *GEOSUnion(Geometry *g1,Geometry *g2);


extern "C" POINT3D  *GEOSGetCoordinate(Geometry *g1);
extern "C" POINT3D  *GEOSGetCoordinates_Polygon(Polygon *g1);
extern "C" POINT3D  *GEOSGetCoordinates(Geometry *g1);
extern "C" int      GEOSGetNumCoordinate(Geometry *g1);
extern "C" const Geometry *GEOSGetGeometryN(Geometry *g1, int n);
extern "C" const Geometry *GEOSGetExteriorRing(Geometry *g1);
extern "C" const Geometry *GEOSGetInteriorRingN(Geometry *g1, int n);
extern "C" int      GEOSGetNumInteriorRings(Geometry *g1);
extern "C" int      GEOSGetSRID(Geometry *g1);
extern "C" int      GEOSGetNumGeometries(Geometry *g1);

extern "C" char GEOSisSimple(Geometry *g1);
extern "C" char GEOSequals(Geometry *g1, Geometry*g2);

extern "C" char GEOSisRing(Geometry *g1);

extern "C" Geometry *GEOSpointonSurface(Geometry *g1);

extern "C" Geometry *GEOSGetCentroid(Geometry *g1);

extern "C" void NOTICE_MESSAGE(char *msg);



//###########################################################

GeometryFactory *geomFactory = NULL;


void initGEOS (int maxalign)
{
	if (geomFactory == NULL)
	{
		geomFactory = new GeometryFactory( new PrecisionModel(), -1); // NOTE: SRID will have to be changed after geometry creation
		MAXIMUM_ALIGNOF = maxalign;
	}
}

// ------------------------------------------------------------------------------
// geometry constuctors - return NULL if there was an error
//-------------------------------------------------------------------------------



		//note: you lose the 3d from this!
Geometry *PostGIS2GEOS_box3d(BOX3D *box, int SRID)
{
	BasicCoordinateList  *cl = new BasicCoordinateList(5);
	try {
		Geometry *g;
		Coordinate c;
		c.x = box->LLB.x; c.y = box->LLB.y;
		cl->setAt(c, 0);
		c.x = box->LLB.x; c.y = box->URT.y;
		cl->setAt(c, 1);
		c.x = box->URT.x; c.y = box->URT.y;
		cl->setAt(c, 2);
		c.x = box->URT.x; c.y = box->LLB.y;
		cl->setAt(c, 3);
		c.x = box->LLB.x; c.y = box->LLB.y;
		cl->setAt(c, 4);

		g = geomFactory->createLinearRing(cl);
#if POSTGIS_GEOS_VERSION == 100
		delete cl;
#endif
		if (g==NULL) return NULL;
		g->setSRID(SRID);
		return g;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete cl;
		return NULL;
	}
	catch (...)
	{
		delete cl;
		return NULL;
	}
}

Geometry *PostGIS2GEOS_collection(Geometry **geoms, int ngeoms,int SRID, bool is3d)
{
	try
	{
		Geometry *g;
		int t;
		vector<Geometry *> *subGeos=new vector<Geometry *>;

		for (t =0; t< ngeoms; t++)
		{
			subGeos->push_back(geoms[t]);
		}
		g = geomFactory->buildGeometry(subGeos);
#if POSTGIS_GEOS_VERSION == 100
		delete subGeos;
#endif
		if (g==NULL)
			return NULL;
		g->setSRID(SRID);
		return g;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *PostGIS2GEOS_point(POINT3D *point,int SRID, bool is3d)
{
	try
	{
			Coordinate *c;

			if (is3d)
				c = new Coordinate(point->x, point->y);
			else
				c = new Coordinate(point->x, point->y, point->z);
			Geometry *g = geomFactory->createPoint(*c);
			delete c;
			if (g==NULL)
				return NULL;
			g->setSRID(SRID);
			return g;
		}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}


/*
 * This function must return an all-new allocated object
 */
Geometry *
PostGIS2GEOS_linestring(const LINE3D *line,int SRID, bool is3d)
{
	try{
		int t;
		Coordinate c;

		//build coordinatelist & pre-allocate space
		BasicCoordinateList  *coords = new BasicCoordinateList(line->npoints);
		if (is3d)
		{
			for (t=0;t<line->npoints;t++)
			{
				c.x = line->points[t].x;
				c.y = line->points[t].y;
				c.z = line->points[t].z;
				coords->setAt( c ,t);
			}
		}
		else  //make 2d points
		{
			for (t=0;t<line->npoints;t++)
			{
				c.x = line->points[t].x;
				c.y = line->points[t].y;
				c.z = DoubleNotANumber;
				coords->setAt( c ,t);
			}
		}
		Geometry *g = geomFactory->createLineString(coords);
#if POSTGIS_GEOS_VERSION == 100
		delete coords;
#endif
		if (g==NULL) return NULL;
		g->setSRID(SRID);
		return g;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

//polygons is an array of pointers to polygons
Geometry *PostGIS2GEOS_multipolygon(POLYGON3D **polygons,int npolys, int SRID, bool is3d)
{
	try
	{
			int t;
			vector<Geometry *> *subPolys=NULL;
			Geometry *g;

			subPolys=new vector<Geometry *>;

			for (t =0; t< npolys; t++)
			{
				subPolys->push_back(PostGIS2GEOS_polygon(polygons[t], SRID,is3d ));
			}
			g = geomFactory->createMultiPolygon(subPolys);
#if POSTGIS_GEOS_VERSION == 100
			delete subPolys;
#endif

			if (g== NULL)
				return NULL;
			g->setSRID(SRID);
			return g;
		}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

//lines is an array of pointers to line3d
Geometry *
PostGIS2GEOS_multilinestring(const LINE3D **lines, int nlines, int SRID, bool is3d)
{
	try
	{
		int t;
		vector<Geometry *> *subLines = new vector<Geometry *>;
		Geometry *g;

		for (t =0; t< nlines; t++)
		{
			subLines->push_back(PostGIS2GEOS_linestring(lines[t],
						SRID,is3d ));
		}
		g = geomFactory->createMultiLineString(subLines);
#if POSTGIS_GEOS_VERSION == 100
		delete subLines;
#endif
		if (g==NULL) return NULL;
		g->setSRID(SRID);
		return g;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}
	catch (...)
	{
		return NULL;
	}
}

Geometry *PostGIS2GEOS_multipoint(POINT3D **points,int npoints, int SRID, bool is3d)
{
	try
	{
			int t;
			vector<Geometry *> *subPoints =new vector<Geometry *>;
			Geometry *g;

			for (t =0; t< npoints; t++)
			{
				subPoints->push_back(PostGIS2GEOS_point(points[t], SRID,is3d ));
			}
			g = geomFactory->createMultiPoint(subPoints);
#if POSTGIS_GEOS_VERSION == 100
			delete subPoints;
#endif
			if (g==NULL)
				return NULL;
			g->setSRID(SRID);
			return g;
		}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}

}


Geometry *PostGIS2GEOS_polygon(POLYGON3D *polygon,int SRID, bool is3d)
{
	try
	{
		POINT3D *pts;
		Coordinate c;
		int  ring,t;
		Geometry *g;
		LinearRing *outerRing;
		LinearRing *innerRing;
		BasicCoordinateList *cl;
		int pointOffset =0; // first point that we're looking at.  a POLYGON3D has all its points smooshed together
		vector<Geometry *> *innerRings=new vector<Geometry *>;


		pts = (POINT3D *) ( (char *)&(polygon->npoints[polygon->nrings] )  );
		pts = (POINT3D *) MAXALIGN(pts);

			// make outerRing
			cl = new BasicCoordinateList(polygon->npoints[0]);
			if (is3d)
			{
				for(t=0;t<polygon->npoints[0];t++)
				{
						c.x = pts[t].x;
						c.y = pts[t].y;
						c.z = pts[t].z;
						cl->setAt( c ,t);
				}
			}
			else
			{
				for(t=0;t<polygon->npoints[0];t++)
				{
						c.x = pts[t].x;
						c.y = pts[t].y;
						c.z = DoubleNotANumber;
						cl->setAt( c ,t);
				}
			}
			outerRing = (LinearRing*) geomFactory->createLinearRing(cl);
#if POSTGIS_GEOS_VERSION == 100
			delete cl;
#endif
			if (outerRing == NULL)
				return NULL;
			outerRing->setSRID(SRID);
			pointOffset = polygon->npoints[0];

		for(ring =1; ring< polygon->nrings; ring++)
		{
			cl = new BasicCoordinateList(polygon->npoints[ring]);
			if (is3d)
			{
				for(t=0;t<polygon->npoints[ring];t++)
				{
						c.x = pts[t+pointOffset].x;
						c.y = pts[t+pointOffset].y;
						c.z = pts[t+pointOffset].z;
						cl->setAt( c ,t);
				}
			}
			else
			{
				for(t=0;t<polygon->npoints[ring];t++)
				{
						c.x = pts[t+pointOffset].x;
						c.y = pts[t+pointOffset].y;
						c.z = DoubleNotANumber;
						cl->setAt( c ,t);
				}
			}
			innerRing = (LinearRing *) geomFactory->createLinearRing(cl);
#if POSTGIS_GEOS_VERSION == 100
			delete cl;
#endif
			if (innerRing == NULL)
			{
				delete outerRing;
				return NULL;
			}
			innerRing->setSRID(SRID);
			innerRings->push_back(innerRing);
			pointOffset += polygon->npoints[ring];
		}

		g = geomFactory->createPolygon(outerRing, innerRings);
		if (g==NULL)
			return NULL;
		g->setSRID(SRID);
		return g;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

//-----------------------------------------------------------
// relate()-related functions
//  return 0 = false, 1 = true, 2 = error occured
//-----------------------------------------------------------

char GEOSrelateDisjoint(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->disjoint(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}
	catch (...)
	{
		return 2;
	}
}

char GEOSrelateTouches(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result =  g1->touches(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char GEOSrelateIntersects(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->intersects(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char GEOSrelateCrosses(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->crosses(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char GEOSrelateWithin(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->within(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

// call g1->contains(g2)
// returns 0 = false
//         1 = true
//         2 = error was trapped
char GEOSrelateContains(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->contains(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char GEOSrelateOverlaps(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->overlaps(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}


//-------------------------------------------------------------------
// low-level relate functions
//------------------------------------------------------------------

char GEOSrelatePattern(Geometry *g1, Geometry*g2,char *pat)
{
	try {
		bool result;
		string s = pat;
		result = g1->relate(g2,pat);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char *GEOSrelate(Geometry *g1, Geometry*g2)
{

	try {

		IntersectionMatrix *im = g1->relate(g2);

		string s;
		char *result;
		if (im == NULL)
				return NULL;

		s= im->toString();
		result = (char*) malloc( s.length() + 1);
		strcpy(result, s.c_str() );

		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}



//-----------------------------------------------------------------
// isValid
//-----------------------------------------------------------------


char GEOSisvalid(Geometry *g1)
{
	try {
		bool result;
		result =g1->isValid();
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}

}


//-----------------------------------------------------------------
// general purpose
//-----------------------------------------------------------------

char GEOSequals(Geometry *g1, Geometry*g2)
{
	try {
		bool result;
		result = g1->equals(g2);
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}



char *GEOSasText(Geometry *g1)
{
	try
	{
		string s = g1->toString();


		char *result;
		result = (char*) malloc( s.length() + 1);
		strcpy(result, s.c_str() );
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

char GEOSisEmpty(Geometry *g1)
{
	try
	{
		return g1->isEmpty();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char GEOSisSimple(Geometry *g1)
{
	try
	{
		return g1->isSimple();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}

char GEOSisRing(Geometry *g1)
{
	try
	{
		return (( (LinearRing*)g1)->isRing());
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 2;
	}

	catch (...)
	{
		return 2;
	}
}



//free the result of this
char *GEOSGeometryType(Geometry *g1)
{
	try
	{
		string s = g1->getGeometryType();


		char *result;
		result = (char*) malloc( s.length() + 1);
		strcpy(result, s.c_str() );
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

// Return postgis geometry type index
int GEOSGeometryTypeId(Geometry *g1)
{
	try
	{
		GeometryTypeId id = g1->getGeometryTypeId();
		switch (id)
		{
			case GEOS_POINT:
				return POINTTYPE;
			case GEOS_LINESTRING:
				return LINETYPE;
			case GEOS_POLYGON:
				return POLYGONTYPE;
			case GEOS_MULTIPOINT:
				return MULTIPOINTTYPE;
			case GEOS_MULTILINESTRING:
				return MULTILINETYPE;
			case GEOS_MULTIPOLYGON:
				return MULTIPOLYGONTYPE;
			case GEOS_GEOMETRYCOLLECTION:
				return COLLECTIONTYPE;
			default:
				return 0;
		}
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return -1;
	}

	catch (...)
	{
		return -1;
	}
}




//-------------------------------------------------------------------
// GEOS functions that return geometries
//-------------------------------------------------------------------

Geometry *GEOSIntersection(Geometry *g1,Geometry *g2)
{
	try
	{
		Geometry *g3 = g1->intersection(g2);
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *GEOSBuffer(Geometry *g1,double width)
{
	try
	{
		Geometry *g3 = g1->buffer(width);
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *GEOSConvexHull(Geometry *g1)
{
	try
	{
		Geometry *g3 = g1->convexHull();
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *GEOSDifference(Geometry *g1,Geometry *g2)
{
	try
	{
		Geometry *g3 = g1->difference(g2);
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *GEOSBoundary(Geometry *g1)
{
	try
	{
		Geometry *g3 = g1->getBoundary();
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *GEOSSymDifference(Geometry *g1,Geometry *g2)
{
	try
	{
		Geometry *g3 = g1->symDifference(g2);
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}

Geometry *GEOSUnion(Geometry *g1,Geometry *g2)
{
	try
	{
		Geometry *g3 = g1->Union(g2);
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}
	catch (...)
	{
		return NULL;
	}
}


Geometry *GEOSpointonSurface(Geometry *g1)
{
	try
	{
		Geometry *g3 = g1->getInteriorPoint();
		return g3;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch (...)
	{
		return NULL;
	}
}





//-------------------------------------------------------------------
// memory management functions
//------------------------------------------------------------------


//BUG:: this leaks memory, but delete kills the PrecisionModel for ALL the geometries
void GEOSdeleteGeometry(Geometry *a)
{
	try{
		delete a;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		//return NULL;
	}

	catch(...)
	{
		// do nothing!
	}
}

void GEOSdeleteChar(char *a)
{
	try{
	   free(a);
	}
	catch (GEOSException *ge) // ???
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		//return NULL;
	}

	catch(...)
	{
		// do nothing!
	}
}


//-------------------------------------------------------------------
//GEOS => POSTGIS conversions
//-------------------------------------------------------------------


// free the result when done!
// g1 must be a point
POINT3D  *GEOSGetCoordinate(Geometry *g1)
{
	try{
		POINT3D		*result = (POINT3D*) malloc (sizeof(POINT3D));
		const Coordinate *c =g1->getCoordinate();

		result->x = c->x;
		result->y = c->y;
		result->z = c->z;
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch(...)
	{
		return NULL;
	}

}


//must free the result when done
// result is an array length g1->getNumCoordinates()
POINT3D  *GEOSGetCoordinates(Geometry *g1)
{
	if ( g1->getGeometryTypeId() == GEOS_POLYGON )
	{
		return GEOSGetCoordinates_Polygon((Polygon *)g1);
	}

	try {
		int numPoints = g1->getNumPoints();
		POINT3D *result = (POINT3D*) malloc (sizeof(POINT3D) * numPoints );
		int t;
		CoordinateList *cl = g1->getCoordinates();
		Coordinate		c;

		for (t=0;t<numPoints;t++)
		{
			c =cl->getAt(t);

			result[t].x = c.x;
			result[t].y = c.y;
			result[t].z = c.z;
		}

		delete cl;
		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch(...)
	{
		return NULL;
	}

}

// A somewhat optimized version for polygon types.
POINT3D  *GEOSGetCoordinates_Polygon(Polygon *g1)
{
	try {
		int t, r, outidx=0;
		const CoordinateList *cl;
		Coordinate c;
		const LineString *lr;
		int npts, nrings;
		POINT3D *result;
		
		npts = g1->getNumPoints();
		result = (POINT3D*) malloc (sizeof(POINT3D) * npts);
		
		// Exterior ring 
		lr = g1->getExteriorRing();
		cl = lr->getCoordinatesRO();
		npts = lr->getNumPoints();
		for (t=0; t<npts; t++)
		{
			c = cl->getAt(t);

			result[outidx].x = c.x;
			result[outidx].y = c.y;
			result[outidx].z = c.z;
			outidx++;
		}

		// Interior rings
		nrings = g1->getNumInteriorRing();
		for (r=0; r<nrings; r++)
		{
			lr = g1->getInteriorRingN(r);
			cl = lr->getCoordinatesRO();
			npts = lr->getNumPoints();
			for (t=0; t<npts; t++)
			{
				c = cl->getAt(t);
				result[outidx].x = c.x;
				result[outidx].y = c.y;
				result[outidx].z = c.z;
				outidx++;
			}
		}

		return result;
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch(...)
	{
		return NULL;
	}

}




int      GEOSGetNumCoordinate(Geometry *g1)
{
	try{
		return g1->getNumPoints();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 0;
	}

	catch(...)
	{
		return 0;
	}
}

int      GEOSGetNumInteriorRings(Geometry *g1)
{
	try{
		Polygon *p = (Polygon *) g1;
		return p->getNumInteriorRing();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 0;
	}

	catch(...)
	{
		return 0;
	}
}


//only call on GCs (or multi*)
int      GEOSGetNumGeometries(Geometry *g1)
{
	try{
		GeometryCollection *gc = (GeometryCollection *) g1;
		return gc->getNumGeometries();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 0;
	}

	catch(...)
	{
		return 0;
	}
}


//call only on GEOMETRYCOLLECTION or MULTI*
const Geometry *GEOSGetGeometryN(Geometry *g1, int n)
{
	try{
		const GeometryCollection *gc = (GeometryCollection *) g1;
		return gc->getGeometryN(n);
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch(...)
	{
		return NULL;
	}
}


//call only on polygon
const Geometry *GEOSGetExteriorRing(Geometry *g1)
{
	try{
		Polygon *p = (Polygon *) g1;
		return p->getExteriorRing();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 0;
	}

	catch(...)
	{
		return 0;
	}
}

//call only on polygon
const Geometry *GEOSGetInteriorRingN(Geometry *g1,int n)
{
	try{
		Polygon *p = (Polygon *) g1;
		return p->getInteriorRingN(n);
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch(...)
	{
		return NULL;
	}
}

Geometry *GEOSGetCentroid(Geometry *g)
{
	try{
		return g->getCentroid();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return NULL;
	}

	catch(...)
	{
		return NULL;
	}
}


int      GEOSGetSRID(Geometry *g1)
{
	try{
		return g1->getSRID();
	}
	catch (GEOSException *ge)
	{
		NOTICE_MESSAGE((char *)ge->toString().c_str());
		delete ge;
		return 0;
	}

	catch(...)
	{
		return 0;
	}
}



