#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liblwgeom.h"

// convert this point into its serialize form
// result's first char will be the 8bit type.  See serialized form doc
char *
lwpoint_serialize(LWPOINT *point)
{
	size_t size, retsize;
	char *result;

	size = lwpoint_serialize_size(point);
	result = lwalloc(size);
	lwpoint_serialize_buf(point, result, &retsize);

	if ( retsize != size )
	{
		lwerror("lwpoint_serialize_size returned %d, ..serialize_buf returned %d", size, retsize);
	}

	return result;
}

// convert this point into its serialize form writing it into
// the given buffer, and returning number of bytes written into
// the given int pointer.
// result's first char will be the 8bit type.  See serialized form doc
void
lwpoint_serialize_buf(LWPOINT *point, char *buf, int *retsize)
{
	int size=1;
	char hasSRID;
	char *loc;

	hasSRID = (point->SRID != -1);

	if (hasSRID) size +=4;  //4 byte SRID
	if (point->hasbbox) size += sizeof(BOX2DFLOAT4); // bvol

	if (point->ndims == 3) size += 24; //x,y,z
	else if (point->ndims == 2) size += 16 ; //x,y,z
	else if (point->ndims == 4) size += 32 ; //x,y,z,m

	buf[0] = (unsigned char) lwgeom_makeType_full(point->ndims,
		hasSRID, POINTTYPE, point->hasbbox);
	loc = buf+1;

	if (point->hasbbox)
	{
		lwgeom_compute_bbox_p((LWGEOM *)point, (BOX2DFLOAT4 *)loc);
		loc += sizeof(BOX2DFLOAT4);
	}

	if (hasSRID)
	{
		memcpy(loc, &point->SRID, sizeof(int32));
		loc += 4;
	}

	//copy in points

	if (point->ndims == 3) getPoint3d_p(point->point, 0, loc);
	else if (point->ndims == 2) getPoint2d_p(point->point, 0, loc);
	else if (point->ndims == 4) getPoint4d_p(point->point, 0, loc);

	if (retsize) *retsize = size;
}

// find bounding box (standard one)  zmin=zmax=0 if 2d (might change to NaN)
BOX3D *
lwpoint_findbbox(LWPOINT *point)
{
#ifdef DEBUG
	lwnotice("lwpoint_findbbox called with point %p", point);
#endif
	if (point == NULL)
	{
#ifdef DEBUG
		lwnotice("lwpoint_findbbox returning NULL");
#endif
		return NULL;
	}

#ifdef DEBUG
	lwnotice("lwpoint_findbbox returning pointArray_bbox return");
#endif

	return pointArray_bbox(point->point);
}

// convenience functions to hide the POINTARRAY
// TODO: obsolete this
POINT2D
lwpoint_getPoint2d(const LWPOINT *point)
{
	POINT2D result;

	if (point == NULL)
			return result;

	return getPoint2d(point->point,0);
}

// convenience functions to hide the POINTARRAY
POINT3D
lwpoint_getPoint3d(const LWPOINT *point)
{
	POINT3D result;

	if (point == NULL)
			return result;

	return getPoint3d(point->point,0);
}

// find length of this deserialized point
size_t
lwpoint_serialize_size(LWPOINT *point)
{
	size_t size = 1; // type

	if ( point->SRID != -1 ) size += 4; // SRID
	if ( point->hasbbox ) size += sizeof(BOX2DFLOAT4);

	size += point->ndims * sizeof(double); // point

	return size; 
}

// construct a new point.  point will not be copied
// use SRID=-1 for unknown SRID (will have 8bit type's S = 0)
LWPOINT *
lwpoint_construct(int ndims, int SRID, POINTARRAY *point)
{
	LWPOINT *result ;

	if (point == NULL)
		return NULL; // error

	result = lwalloc(sizeof(LWPOINT));
	result->type = POINTTYPE;
	result->ndims = ndims;
	result->SRID = SRID;

	result->point = point;

	return result;
}

// given the LWPOINT serialized form (or a pointer into a muli* one)
// construct a proper LWPOINT.
// serialized_form should point to the 8bit type format (with type = 1)
// See serialized form doc
LWPOINT *
lwpoint_deserialize(char *serialized_form)
{
	unsigned char type;
	LWPOINT *result;
	char *loc = NULL;
	POINTARRAY *pa;

#ifdef DEBUG
	lwnotice("lwpoint_deserialize called");
#endif

	result = (LWPOINT*) lwalloc(sizeof(LWPOINT)) ;

	type = (unsigned char) serialized_form[0];

	if ( lwgeom_getType(type) != POINTTYPE) return NULL;
	result->type = POINTTYPE;

	loc = serialized_form+1;

	if (lwgeom_hasBBOX(type))
	{
#ifdef DEBUG
		lwnotice("lwpoint_deserialize: input has bbox");
#endif
		result->hasbbox = 1;
		loc += sizeof(BOX2DFLOAT4);
	}
	else result->hasbbox = 0;

	if ( lwgeom_hasSRID(type))
	{
#ifdef DEBUG
		lwnotice("lwpoint_deserialize: input has SRID");
#endif
		result->SRID = get_int32(loc);
		loc += 4; // type + SRID
	}
	else
	{
		result->SRID = -1;
	}

	// we've read the type (1 byte) and SRID (4 bytes, if present)

	pa = pointArray_construct(loc, lwgeom_ndims(type), 1);

	result->point = pa;
	result->ndims = lwgeom_ndims(type);

	return result;
}

void pfree_point    (LWPOINT *pt)
{
	pfree_POINTARRAY(pt->point);
	lwfree(pt);
}

void printLWPOINT(LWPOINT *point)
{
	lwnotice("LWPOINT {");
	lwnotice("    ndims = %i", (int)point->ndims);
	lwnotice("    BBOX = %i", point->hasbbox ? 1 : 0 );
	lwnotice("    SRID = %i", (int)point->SRID);
	printPA(point->point);
	lwnotice("}");
}

int
lwpoint_compute_bbox_p(LWPOINT *point, BOX2DFLOAT4 *box)
{
	return ptarray_compute_bbox_p(point->point, box);
}

// Clone LWPOINT object. POINTARRAY is not copied.
LWPOINT *
lwpoint_clone(const LWPOINT *g)
{
	LWPOINT *ret = lwalloc(sizeof(LWPOINT));
	memcpy(ret, g, sizeof(LWPOINT));
	return ret;
}

// Add 'what' to this point at position 'where'.
// where=0 == prepend
// where=-1 == append
// Returns a MULTIPOINT or a GEOMETRYCOLLECTION
LWGEOM *
lwpoint_add(const LWPOINT *to, uint32 where, const LWGEOM *what)
{
	LWCOLLECTION *col;
	LWGEOM **geoms;
	int newtype;

	if ( where != -1 && where != 0 )
	{
		lwerror("lwpoint_add only supports 0 or -1 as second argument, got %d", where);
		return NULL;
	}

	// dimensions compatibility are checked by caller


	// Construct geoms array
	geoms = lwalloc(sizeof(LWGEOM *)*2);
	if ( where == -1 ) // append
	{
		geoms[0] = lwgeom_clone((LWGEOM *)to);
		geoms[1] = lwgeom_clone(what);
	}
	else // prepend
	{
		geoms[0] = lwgeom_clone(what);
		geoms[1] = lwgeom_clone((LWGEOM *)to);
	}
	// reset SRID and wantbbox flag from component types
	geoms[0]->SRID = geoms[1]->SRID = -1;
	geoms[0]->hasbbox = geoms[1]->hasbbox = 0;

	// Find appropriate geom type
	if ( what->type == POINTTYPE ) newtype = MULTIPOINTTYPE;
	else newtype = COLLECTIONTYPE;

	col = lwcollection_construct(newtype, to->ndims, to->SRID,
		(what->hasbbox || to->hasbbox ), 2, geoms);
	
	return (LWGEOM *)col;
}
