/**********************************************************************
 * $Id:$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2010 - Oslandia
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

/* basic LWTRIANGLE manipulation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liblwgeom_internal.h"



/* construct a new LWTRIANGLE.
 * use SRID=-1 for unknown SRID (will have 8bit type's S = 0)
 */
LWTRIANGLE*
lwtriangle_construct(int SRID, BOX2DFLOAT4 *bbox, POINTARRAY *points)
{
	LWTRIANGLE *result;
	int hasz, hasm;

	hasz = TYPE_HASZ(points->dims);
	hasm = TYPE_HASM(points->dims);

	result = (LWTRIANGLE*) lwalloc(sizeof(LWTRIANGLE));
	result->type = lwgeom_makeType_full(hasz, hasm, (SRID!=-1), TRIANGLETYPE, 0);
	result->SRID = SRID;
	result->points = points;
	result->bbox = bbox;

	return result;
}

LWTRIANGLE*
lwtriangle_construct_empty(int srid, char hasz, char hasm)
{
	LWTRIANGLE *result = lwalloc(sizeof(LWTRIANGLE));
	result->type = lwgeom_makeType_full(hasz, hasm, (srid>0), TRIANGLETYPE, 0);
	result->SRID = srid;
	result->points = NULL;
	result->bbox = NULL;
	return result;
}


/*
 * given the LWTRIANGLE serialized form
 * construct a proper LWTRIANGLE.
 * serialized_form should point to the 8bit type format
 * See serialized form doc
 */
LWTRIANGLE *
lwtriangle_deserialize(uchar *serialized_form)
{
	LWTRIANGLE *result;
	POINTARRAY *pa;
	int ndims, hasz, hasm;
	uint32 npoints;
	uchar type;
	uchar *loc;

	LWDEBUG(3, "lwtriangle_deserialize called");

	if (serialized_form == NULL)
	{
		lwerror("lwtriangle_deserialize called with NULL arg");
		return NULL;
	}

	result = (LWTRIANGLE*) lwalloc(sizeof(LWTRIANGLE));

	type = serialized_form[0];
	result->type = type;

	ndims = TYPE_NDIMS(type);
	hasz = TYPE_HASZ(type);
	hasm = TYPE_HASM(type);
	loc = serialized_form;

	if ( TYPE_GETTYPE(type) != TRIANGLETYPE)
	{
		lwerror("lwtriangle_deserialize: attempt to deserialize a triangle which is really a %s",
		        lwtype_name(type));
		return NULL;
	}

	loc = serialized_form+1;

	if (lwgeom_hasBBOX(type))
	{
		LWDEBUG(3, "lwtriangle_deserialize: input has bbox");

		result->bbox = lwalloc(sizeof(BOX2DFLOAT4));
		memcpy(result->bbox, loc, sizeof(BOX2DFLOAT4));
		loc += sizeof(BOX2DFLOAT4);
	}
	else
	{
		result->bbox = NULL;
	}

	if ( lwgeom_hasSRID(type))
	{
		result->SRID = lw_get_int32(loc);
		loc +=4; /* type + SRID */
	}
	else
	{
		result->SRID = -1;
	}

	npoints = lw_get_uint32(loc);
	/*lwnotice("triangle npoints = %d", npoints); */
	loc +=4;
	pa = pointArray_construct(loc, TYPE_HASZ(type), TYPE_HASM(type), npoints);
	result->points = pa;

	return result;
}

/*
 * create the serialized form of the triangle
 * result's first char will be the 8bit type.  See serialized form doc
 * points copied
 */
uchar *
lwtriangle_serialize(LWTRIANGLE *triangle)
{
	size_t size, retsize;
	uchar *result;

	size = lwtriangle_serialize_size(triangle);
	result = lwalloc(size);
	lwtriangle_serialize_buf(triangle, result, &retsize);

	if ( retsize != size )
	{
		lwerror("lwtriangle_serialize_size returned %d, ..serialize_buf returned %d", size, retsize);
	}

	return result;
}

/*
 * create the serialized form of the triangle writing it into the
 * given buffer, and returning number of bytes written into
 * the given int pointer.
 * result's first char will be the 8bit type.  See serialized form doc
 * points copied
 */
void
lwtriangle_serialize_buf(LWTRIANGLE *triangle, uchar *buf, size_t *retsize)
{
	char hasSRID;
	uchar *loc;
	int ptsize;
	size_t size;

	LWDEBUGF(2, "lwtriangle_serialize_buf(%p, %p, %p) called",
	         triangle, buf, retsize);

	if (triangle == NULL)
		lwerror("lwtriangle_serialize:: given null triangle");

	if ( TYPE_GETZM(triangle->type) != TYPE_GETZM(triangle->points->dims) )
		lwerror("Dimensions mismatch in lwtriangle");

	ptsize = pointArray_ptsize(triangle->points);

	hasSRID = (triangle->SRID != -1);

	buf[0] = (uchar) lwgeom_makeType_full(
	             TYPE_HASZ(triangle->type), TYPE_HASM(triangle->type),
	             hasSRID, TRIANGLETYPE, triangle->bbox ? 1 : 0);
	loc = buf+1;

	LWDEBUGF(3, "lwtriangle_serialize_buf added type (%d)", triangle->type);

	if (triangle->bbox)
	{
		memcpy(loc, triangle->bbox, sizeof(BOX2DFLOAT4));
		loc += sizeof(BOX2DFLOAT4);

		LWDEBUG(3, "lwtriangle_serialize_buf added BBOX");
	}

	if (hasSRID)
	{
		memcpy(loc, &triangle->SRID, sizeof(int32));
		loc += sizeof(int32);

		LWDEBUG(3, "lwtriangle_serialize_buf added SRID");
	}

	memcpy(loc, &triangle->points->npoints, sizeof(uint32));
	loc += sizeof(uint32);

	LWDEBUGF(3, "lwtriangle_serialize_buf added npoints (%d)",
	         triangle->points->npoints);

	/*copy in points */
	size = triangle->points->npoints*ptsize;
	memcpy(loc, getPoint_internal(triangle->points, 0), size);
	loc += size;

	LWDEBUGF(3, "lwtriangle_serialize_buf copied serialized_pointlist (%d bytes)",
	         ptsize * triangle->points->npoints);

	if (retsize) *retsize = loc-buf;

	/*printBYTES((uchar *)result, loc-buf); */

	LWDEBUGF(3, "lwtriangle_serialize_buf returning (loc: %p, size: %d)",
	         loc, loc-buf);
}


/* find bounding box (standard one)  zmin=zmax=0 if 2d (might change to NaN) */
BOX3D *
lwtriangle_compute_box3d(LWTRIANGLE *triangle)
{
	BOX3D *result;

	if (triangle== NULL) return NULL;
	result  = ptarray_compute_box3d(triangle->points);

	return result;
}

size_t
lwgeom_size_triangle(const uchar *serialized_triangle)
{
	int type = (uchar) serialized_triangle[0];
	uint32 result = 1;  /*type */
	const uchar *loc;
	uint32 npoints;

	LWDEBUG(2, "lwgeom_size_triangle called");

	if ( lwgeom_getType(type) != TRIANGLETYPE)
		lwerror("lwgeom_size_triangle::attempt to find the length of a non-triangle");


	loc = serialized_triangle+1;

	if (lwgeom_hasBBOX(type))
	{
		loc += sizeof(BOX2DFLOAT4);
		result +=sizeof(BOX2DFLOAT4);
	}

	if ( lwgeom_hasSRID(type))
	{
		loc += 4; /* type + SRID */
		result +=4;
	}

	/* we've read the type (1 byte) and SRID (4 bytes, if present) */
	npoints = lw_get_uint32(loc);
	result += sizeof(uint32); /* npoints */

	result += TYPE_NDIMS(type) * sizeof(double) * npoints;

	LWDEBUGF(3, "lwgeom_size_triangle returning %d", result);

	return result;
}


/* find length of this deserialized triangle */
size_t
lwtriangle_serialize_size(LWTRIANGLE *triangle)
{
	size_t size = 1;  /* type */

	LWDEBUG(2, "lwtriangle_serialize_size called");

	if ( triangle->SRID != -1 ) size += 4; /* SRID */
	if ( triangle->bbox ) size += sizeof(BOX2DFLOAT4);

	size += 4; /* npoints */
	size += pointArray_ptsize(triangle->points)*triangle->points->npoints;

	LWDEBUGF(3, "lwtriangle_serialize_size returning %d", size);

	return size;
}


void lwtriangle_free(LWTRIANGLE  *triangle)
{
	if (triangle->bbox) lwfree(triangle->bbox);
	ptarray_free(triangle->points);
	lwfree(triangle);
}

void printLWTRIANGLE(LWTRIANGLE *triangle)
{
	lwnotice("LWTRIANGLE {");
	lwnotice("    ndims = %i", (int)TYPE_NDIMS(triangle->type));
	lwnotice("    SRID = %i", (int)triangle->SRID);
	printPA(triangle->points);
	lwnotice("}");
}

int
lwtriangle_compute_box2d_p(const LWTRIANGLE *triangle, BOX2DFLOAT4 *box)
{
	return ptarray_compute_box2d_p(triangle->points, box);
}

/* Clone LWTRIANGLE object. POINTARRAY are not copied. */
LWTRIANGLE *
lwtriangle_clone(const LWTRIANGLE *g)
{
	LWTRIANGLE *ret = lwalloc(sizeof(LWTRIANGLE));
	LWDEBUGF(2, "lwtriangle_clone called with %p", g);
	memcpy(ret, g, sizeof(LWTRIANGLE));
	if ( g->bbox ) ret->bbox = box2d_clone(g->bbox);
	return ret;
}

void
lwtriangle_force_clockwise(LWTRIANGLE *triangle)
{
	if ( ptarray_isccw(triangle->points) )
		ptarray_reverse(triangle->points);
}

void
lwtriangle_reverse(LWTRIANGLE *triangle)
{
	ptarray_reverse(triangle->points);
}

void
lwtriangle_release(LWTRIANGLE *lwtriangle)
{
	lwgeom_release(lwtriangle_as_lwgeom(lwtriangle));
}

/* check coordinate equality  */
char
lwtriangle_same(const LWTRIANGLE *t1, const LWTRIANGLE *t2)
{
	return ptarray_same(t1->points, t2->points);
}

/*
 * Construct a triangle from a LWLINE being
 * the shell
 * Pointarray from intput geom are cloned.
 * Input line must have 4 points, and be closed.
 */
LWTRIANGLE *
lwtriangle_from_lwline(const LWLINE *shell)
{
	LWTRIANGLE *ret;
	POINTARRAY *pa;

	if ( shell->points->npoints != 4 )
		lwerror("lwtriangle_from_lwline: shell must have exactly 4 points");

	if (   (!TYPE_HASZ(shell->type) && !ptarray_isclosed2d(shell->points)) ||
	        (TYPE_HASZ(shell->type) && !ptarray_isclosed3d(shell->points)) )
		lwerror("lwtriangle_from_lwline: shell must be closed");

	pa = ptarray_clone(shell->points);
	ret = lwtriangle_construct(shell->SRID, NULL, pa);

	if (lwtriangle_is_repeated_points(ret))
		lwerror("lwtriangle_from_lwline: some points are repeated in triangle");

	return ret;
}

char
lwtriangle_is_repeated_points(LWTRIANGLE *triangle)
{
	char ret;
	POINTARRAY *pa;

	pa = ptarray_remove_repeated_points(triangle->points);
	ret = ptarray_same(pa, triangle->points);
	ptarray_free(pa);

	return ret;
}
