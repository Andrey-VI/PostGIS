/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2001-2006 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include <stdio.h>
#include <string.h>

#include "liblwgeom_internal.h"

POINTARRAY*
ptarray_construct(char hasz, char hasm, uint32 npoints)
{
	uchar dims = 0;
	size_t size;
	uchar *ptlist;
	POINTARRAY *pa;

	FLAGS_SET_Z(dims, hasz?1:0);
	FLAGS_SET_M(dims, hasm?1:0);
	size = FLAGS_NDIMS(dims)*npoints*sizeof(double);

	if ( size )
		ptlist = (uchar *)lwalloc(size);
	else
		ptlist = NULL;

	pa = lwalloc(sizeof(POINTARRAY));
	pa->dims = dims;
	pa->serialized_pointlist = ptlist;
	pa->npoints = npoints;
	pa->maxpoints = npoints;

	return pa;

}

POINTARRAY*
ptarray_construct_empty(char hasz, char hasm, int maxpoints)
{
	uchar dims = 0;
	POINTARRAY *pa = lwalloc(sizeof(POINTARRAY));
	pa->serialized_pointlist = NULL;
	
	/* Set our dimsionality info on the bitmap */
	FLAGS_SET_Z(dims, hasz?1:0);
	FLAGS_SET_M(dims, hasm?1:0);
	pa->dims = dims;
	
	/* We will be allocating a bit of room */
	pa->npoints = 0;
	pa->maxpoints = maxpoints;
	
	/* Allocate the coordinate array */
	pa->serialized_pointlist = lwalloc(maxpoints * ptarray_point_size(pa));

	return pa;
}

/*
* Add a point into a pointarray. Only adds as many dimensions as the 
* pointarray supports.
*/
int
ptarray_insert_point(POINTARRAY *pa, POINT4D *p, int where)
{
	size_t point_size = ptarray_point_size(pa);
	LWDEBUGF(5,"pa = %p; p = %p; where = %d", pa, p, where);
	LWDEBUGF(5,"pa->npoints = %d; pa->maxpoints = %d", pa->npoints, pa->maxpoints);
	
	/* Error on invalid offset value */
	if ( where > pa->npoints || where < 0)
	{
		lwerror("ptarray_insert_point: offset out of range (%d)", where);
		return LW_FAILURE;
	}
	
	/* If we have no storage, let's allocate some */
	if( pa->maxpoints == 0 || ! pa->serialized_pointlist ) 
	{
		pa->maxpoints = 32;
		pa->npoints = 0;
		pa->serialized_pointlist = lwalloc(ptarray_point_size(pa) * pa->maxpoints);
	}

	/* Error out if we have a bad situation */
	if ( pa->npoints > pa->maxpoints )
		lwerror("npoints (%d) is greated than maxpoints (%d)", pa->npoints, pa->maxpoints);
	
	/* Check if we have enough storage, add more if necessary */
	if( pa->npoints == pa->maxpoints )
	{
		pa->maxpoints *= 2;
		pa->serialized_pointlist = lwrealloc(pa->serialized_pointlist, ptarray_point_size(pa) * pa->maxpoints);
	}
	
	/* Make space to insert the new point */
	if( where < pa->npoints )
	{
		size_t copy_size = point_size * (pa->npoints - where);
		memcpy(getPoint_internal(pa, where+1), getPoint_internal(pa, where), copy_size);
		LWDEBUGF(5,"copying %d bytes to start vertex %d from start vertex %d", copy_size, where+1, where);
	}
	
	/* Copy the new point into the gap */
	ptarray_set_point4d(pa, where, p);
	LWDEBUGF(5,"copying new point to start vertex %d", point_size, where);
	
	/* We have one more point */
	pa->npoints++;
	
	return LW_SUCCESS;
}

int
ptarray_append_point(POINTARRAY *pa, POINT4D *pt, int repeated_points)
{

	/* Check for pathology */
	if( ! pa || ! pt ) 
	{
		lwerror("ptarray_append_point: null input");
		return LW_FAILURE;
	}

	/* Check for duplicate end point */
	if ( repeated_points == REPEATED_POINTS_NOT_OK && pa->npoints > 0 )
	{
		POINT4D tmp;
		getPoint4d_p(pa, pa->npoints-1, &tmp);
		LWDEBUGF(4,"checking for duplicate end point (pt = POINT(%g %g) pa->npoints-q = POINT(%g %g))",pt->x,pt->y,tmp.x,tmp.y);

		/* Return LW_SUCCESS and do nothing else if previous point in list is equal to this one */
		if ( (pt->x == tmp.x) && (pt->y == tmp.y) &&
		     (TYPE_HASZ(pa->dims) ? pt->z == tmp.z : 1) &&
		     (TYPE_HASM(pa->dims) ? pt->m == tmp.m : 1) )
		{
			return LW_SUCCESS;
		}
	}

	/* Append is just a special case of insert */
	return ptarray_insert_point(pa, pt, pa->npoints);
}

int
ptarray_append_ptarray(POINTARRAY *pa1, POINTARRAY *pa2, int splice_ends)
{

	/* Check for pathology */
	if( ! pa1 || ! pa2 ) 
	{
		lwerror("ptarray_append_ptarray: null input");
		return LW_FAILURE;
	}
	
	if( pa1->dims != pa2->dims )
	{
		lwerror("ptarray_append_ptarray: appending mixed dimensionality is not allowed");
		return LW_FAILURE;
	}

	/* Check for duplicate end point */
	if ( splice_ends && pa1->npoints > 0 && pa2->npoints > 0 )
	{
		POINT4D tmp1, tmp2;
		getPoint4d_p(pa1, pa1->npoints-1, &tmp1);
		getPoint4d_p(pa2, 0, &tmp2);

		/* If the end point and start point are the same, then strip off the end point */
		if (p4d_same(tmp1, tmp2)) 
		{
			pa1->npoints--;
		}
	}
	
	/* Check if we need extra space */
	if ( pa1->maxpoints - pa1->npoints - pa2->npoints < 0 )
	{
		
	}

	return 0;
}

/*
* Add a point into a pointarray. Only adds as many dimensions as the 
* pointarray supports.
*/
int
ptarray_remove_point(POINTARRAY *pa, int where)
{
	size_t ptsize = ptarray_point_size(pa);

	/* Check for pathology */
	if( ! pa ) 
	{
		lwerror("ptarray_remove_point: null input");
		return LW_FAILURE;
	}
	
	/* Error on invalid offset value */
	if ( where >= pa->npoints || where < 0)
	{
		lwerror("ptarray_remove_point: offset out of range (%d)", where);
		return LW_FAILURE;
	}
	
	/* If the point is any but the last, we need to copy the data back one point */
	if( where < pa->npoints - 1 )
	{
		memcpy(getPoint_internal(pa, where), getPoint_internal(pa, where+1), ptsize * (pa->npoints - where - 1));
	}
	
	/* We have one less point */
	pa->npoints--;
	
	return LW_SUCCESS;
}

POINTARRAY* ptarray_construct_reference_data(char hasz, char hasm, uint32 npoints, uchar *ptlist)
{
	POINTARRAY *pa = lwalloc(sizeof(POINTARRAY));
	LWDEBUGF(5, "hasz = %d, hasm = %d, npoints = %d, ptlist = %p", hasz, hasm, npoints, ptlist);
	FLAGS_SET_Z(pa->dims, hasz?1:0);
	FLAGS_SET_M(pa->dims, hasm?1:0);
	pa->npoints = npoints;
	pa->maxpoints = npoints;
	pa->serialized_pointlist = ptlist;
	return pa;
}


POINTARRAY*
ptarray_construct_copy_data(char hasz, char hasm, uint32 npoints, const uchar *ptlist)
{
	POINTARRAY *pa = lwalloc(sizeof(POINTARRAY));

	FLAGS_SET_Z(pa->dims, hasz?1:0);
	FLAGS_SET_M(pa->dims, hasm?1:0);
	pa->npoints = npoints;
	pa->maxpoints = npoints;

	if ( npoints > 0 )
	{
		pa->serialized_pointlist = lwalloc(ptarray_point_size(pa) * npoints);
		memcpy(pa->serialized_pointlist, ptlist, ptarray_point_size(pa) * npoints);
	}
	else
	{
		pa->serialized_pointlist = NULL;
	}

	return pa;
}

void ptarray_free(POINTARRAY *pa)
{
	/**
	*  	TODO: \todo	Turn this on after retrofitting all calls to lwfree_ in /lwgeom
	*		if( pa->serialized_pointlist )
	*		lwfree(pa->serialized_pointlist);
	 */

	lwfree(pa);
}

void ptarray_freeall(POINTARRAY *pa)
{
	if(pa)
	{
		if(pa->serialized_pointlist)
			lwfree(pa->serialized_pointlist);	
		lwfree(pa);
	}
}


void
ptarray_reverse(POINTARRAY *pa)
{
	/* TODO change this to double array operations once point array is double alligned */
	POINT4D pbuf;
	uint32 i;
	int ptsize = ptarray_point_size(pa);
	int last = pa->npoints-1;
	int mid = last/2;

	for (i=0; i<=mid; i++)
	{
		uchar *from, *to;
		from = getPoint_internal(pa, i);
		to = getPoint_internal(pa, (last-i));
		memcpy((uchar *)&pbuf, to, ptsize);
		memcpy(to, from, ptsize);
		memcpy(from, (uchar *)&pbuf, ptsize);
	}

}


/**
 * Reverse X and Y axis on a given POINTARRAY
 */
POINTARRAY*
ptarray_flip_coordinates(POINTARRAY *pa)
{
	int i;
	double d;
	POINT4D p;

	for (i=0 ; i < pa->npoints ; i++)
	{
		getPoint4d_p(pa, i, &p);
		d = p.y;
		p.y = p.x;
		p.x = d;
		ptarray_set_point4d(pa, i, &p);
	}

	return pa;
}


/**
 * @brief calculate the 2d bounding box of a set of points
 * 		write result to the provided BOX2DFLOAT4
 * 		Return 0 if bounding box is NULL (empty geom)
 */
int
ptarray_compute_box2d_p(const POINTARRAY *pa, BOX2DFLOAT4 *result)
{
	BOX3D box3d;

	/* Note: we perform the calculation using BOX3D and then
	   convert to BOX2DFLOAT4 to prevent cumulative floating
	   point rounding errors when calculating using BOX2DFLOAT4
	   directly. */

	if (pa->npoints == 0) return 0;

	ptarray_compute_box3d_p(pa, &box3d);

	box3d_to_box2df_p(&box3d, result);

	return 1;
}

/**
 * @brief Calculate the 2d bounding box of a set of points.
 * @return allocated #BOX2DFLOAT4 or NULL (for empty array).
 */
BOX2DFLOAT4 *
ptarray_compute_box2d(const POINTARRAY *pa)
{
	BOX3D box3d;
	BOX2DFLOAT4 *result;

	/* Note: we perform the calculation using BOX3D and then
	   convert to BOX2DFLOAT4 to prevent cumulative floating
	   point rounding errors when calculating using BOX2DFLOAT4
	   directly. */

	result = lwalloc(sizeof(BOX2DFLOAT4));

	ptarray_compute_box3d_p(pa, &box3d);
	box3d_to_box2df_p(&box3d, result);

	return result;
}

/**
 * @brief Returns a modified #POINTARRAY so that no segment is
 * 		longer than the given distance (computed using 2d).
 *
 * Every input point is kept.
 * Z and M values for added points (if needed) are set to 0.
 */
POINTARRAY *
ptarray_segmentize2d(const POINTARRAY *ipa, double dist)
{
	double	segdist;
	POINT4D	p1, p2;
	POINT4D pbuf;
	POINTARRAY *opa;
	int ipoff=0; /* input point offset */
	int hasz = FLAGS_GET_Z(ipa->dims);
	int hasm = FLAGS_GET_M(ipa->dims);

	pbuf.x = pbuf.y = pbuf.z = pbuf.m = 0;

	/* Initial storage */
	opa = ptarray_construct_empty(hasz, hasm, ipa->npoints);
	
	/* Add first point */
	getPoint4d_p(ipa, ipoff, &p1);
	ptarray_append_point(opa, &p1, REPEATED_POINTS_NOT_OK);

	ipoff++;

	while (ipoff<ipa->npoints)
	{
		/*
		 * We use these pointers to avoid
		 * "strict-aliasing rules break" warning raised
		 * by gcc (3.3 and up).
		 *
		 * It looks that casting a variable address (also
		 * referred to as "type-punned pointer")
		 * breaks those "strict" rules.
		 *
		 */
		POINT4D *p1ptr=&p1, *p2ptr=&p2;

		getPoint4d_p(ipa, ipoff, &p2);

		segdist = distance2d_pt_pt((POINT2D *)p1ptr, (POINT2D *)p2ptr);

		if (segdist > dist) /* add an intermediate point */
		{
			pbuf.x = p1.x + (p2.x-p1.x)/segdist * dist;
			pbuf.y = p1.y + (p2.y-p1.y)/segdist * dist;
			if( hasz ) 
				pbuf.z = p1.z + (p2.z-p1.z)/segdist * dist;
			if( hasm )
				pbuf.m = p1.m + (p2.m-p1.m)/segdist * dist;
			ptarray_append_point(opa, &pbuf, REPEATED_POINTS_NOT_OK);
			p1 = pbuf;
		}
		else /* copy second point */
		{
			ptarray_append_point(opa, &p2, REPEATED_POINTS_NOT_OK);
			p1 = p2;
			ipoff++;
		}
	}

	return opa;
}

char
ptarray_same(const POINTARRAY *pa1, const POINTARRAY *pa2)
{
	uint32 i;
	size_t ptsize;

	if ( FLAGS_GET_ZM(pa1->dims) != FLAGS_GET_ZM(pa2->dims) ) return LW_FALSE;
	LWDEBUG(5,"dimensions are the same");
	
	if ( pa1->npoints != pa2->npoints ) return LW_FALSE;
	LWDEBUG(5,"npoints are the same");

	ptsize = ptarray_point_size(pa1);
	LWDEBUGF(5, "ptsize = %d", ptsize);

	for (i=0; i<pa1->npoints; i++)
	{
		if ( memcmp(getPoint_internal(pa1, i), getPoint_internal(pa2, i), ptsize) )
			return LW_FALSE;
		LWDEBUGF(5,"point #%d is the same",i);
	}

	return LW_TRUE;
}



/**
 * @brief Add a point in a pointarray.
 *
 * @param pa the source POINTARRAY
 * @param p the point to add
 * @param pdims number of ordinates in p (2..4)
 * @param where to insert the point. 0 prepends, pa->npoints appends
 *
 * @returns a newly constructed POINTARRAY using a newly allocated buffer
 *          for the actual points, or NULL on error.
 */
POINTARRAY *
ptarray_addPoint(const POINTARRAY *pa, uchar *p, size_t pdims, uint32 where)
{
	POINTARRAY *ret;
	POINT4D pbuf;
	size_t ptsize = ptarray_point_size(pa);

	LWDEBUGF(3, "pa %x p %x size %d where %d",
	         pa, p, pdims, where);

	if ( pdims < 2 || pdims > 4 )
	{
		lwerror("ptarray_addPoint: point dimension out of range (%d)",
		        pdims);
		return NULL;
	}

	if ( where > pa->npoints )
	{
		lwerror("ptarray_addPoint: offset out of range (%d)",
		        where);
		return NULL;
	}

	LWDEBUG(3, "called with a %dD point");

	pbuf.x = pbuf.y = pbuf.z = pbuf.m = 0.0;
	memcpy((uchar *)&pbuf, p, pdims*sizeof(double));

	LWDEBUG(3, "initialized point buffer");

	ret = ptarray_construct(FLAGS_GET_Z(pa->dims),
	                        FLAGS_GET_M(pa->dims), pa->npoints+1);

	if ( where == -1 ) where = pa->npoints;

	if ( where )
	{
		memcpy(getPoint_internal(ret, 0), getPoint_internal(pa, 0), ptsize*where);
	}

	memcpy(getPoint_internal(ret, where), (uchar *)&pbuf, ptsize);

	if ( where+1 != ret->npoints )
	{
		memcpy(getPoint_internal(ret, where+1),
		       getPoint_internal(pa, where),
		       ptsize*(pa->npoints-where));
	}

	return ret;
}


/**
 * @brief Remove a point from a pointarray.
 * 	@param which -  is the offset (starting at 0)
 * @return #POINTARRAY is newly allocated
 */
POINTARRAY *
ptarray_removePoint(POINTARRAY *pa, uint32 which)
{
	POINTARRAY *ret;
	size_t ptsize = ptarray_point_size(pa);

	LWDEBUGF(3, "pa %x which %d", pa, which);

#if PARANOIA_LEVEL > 0
	if ( which > pa->npoints-1 )
	{
		lwerror("ptarray_removePoint: offset (%d) out of range (%d..%d)",
		        which, 0, pa->npoints-1);
		return NULL;
	}

	if ( pa->npoints < 3 )
	{
		lwerror("ptarray_removePointe: can't remove a point from a 2-vertex POINTARRAY");
	}
#endif

	ret = ptarray_construct(FLAGS_GET_Z(pa->dims),
	                        FLAGS_GET_M(pa->dims), pa->npoints-1);

	/* copy initial part */
	if ( which )
	{
		memcpy(getPoint_internal(ret, 0), getPoint_internal(pa, 0), ptsize*which);
	}

	/* copy final part */
	if ( which < pa->npoints-1 )
	{
		memcpy(getPoint_internal(ret, which), getPoint_internal(pa, which+1),
		       ptsize*(pa->npoints-which-1));
	}

	return ret;
}


/**
 * @brief Merge two given POINTARRAY and returns a pointer
 * on the new aggregate one.
 * Warning: this function free the two inputs POINTARRAY
 * @return #POINTARRAY is newly allocated
 */
POINTARRAY *
ptarray_merge(POINTARRAY *pa1, POINTARRAY *pa2)
{
	POINTARRAY *pa;
	size_t ptsize = ptarray_point_size(pa1);

	if (FLAGS_GET_ZM(pa1->dims) != FLAGS_GET_ZM(pa2->dims))
		lwerror("ptarray_cat: Mixed dimension");

	pa = ptarray_construct( FLAGS_GET_Z(pa1->dims),
	                        FLAGS_GET_M(pa1->dims),
	                        pa1->npoints + pa2->npoints);

	memcpy(         getPoint_internal(pa, 0),
	                getPoint_internal(pa1, 0),
	                ptsize*(pa1->npoints));

	memcpy(         getPoint_internal(pa, pa1->npoints),
	                getPoint_internal(pa2, 0),
	                ptsize*(pa2->npoints));

	lwfree(pa1);
	lwfree(pa2);

	return pa;
}


/**
 * @brief Clone a pointarray
 */
POINTARRAY *
ptarray_clone(const POINTARRAY *in)
{
	POINTARRAY *out = lwalloc(sizeof(POINTARRAY));
	size_t size;

	LWDEBUG(3, "ptarray_clone called.");

	out->dims = in->dims;
	out->npoints = in->npoints;
	out->maxpoints = in->maxpoints;

	size = in->npoints * ptarray_point_size(in);
	out->serialized_pointlist = lwalloc(size);
	memcpy(out->serialized_pointlist, in->serialized_pointlist, size);

	return out;
}

/**
* Check for ring closure using whatever dimensionality is declared on the 
* pointarray.
*/
int
ptarray_isclosed(const POINTARRAY *in)
{
	int ndims = FLAGS_NDIMS(in->dims);
	if ( memcmp(getPoint_internal(in, 0), getPoint_internal(in, in->npoints-1), ndims*sizeof(double)) ) return 0;
	return 1;
}


int
ptarray_isclosed2d(const POINTARRAY *in)
{
	if ( memcmp(getPoint_internal(in, 0), getPoint_internal(in, in->npoints-1), sizeof(POINT2D)) ) return 0;
	return 1;
}

int
ptarray_isclosed3d(const POINTARRAY *in)
{
	if ( memcmp(getPoint_internal(in, 0), getPoint_internal(in, in->npoints-1), sizeof(POINT3D)) ) return 0;
	return 1;
}


POINTARRAY*
ptarray_force_dims(const POINTARRAY *pa, int hasz, int hasm)
{
	/* TODO handle zero-length point arrays */
	int i;
	int in_hasz = FLAGS_GET_Z(pa->dims);
	int in_hasm = FLAGS_GET_M(pa->dims);
	POINT4D pt;
	POINTARRAY *pa_out = ptarray_construct_empty(hasz, hasm, pa->npoints);
	
	for( i = 0; i < pa->npoints; i++ )
	{
		getPoint4d_p(pa, i, &pt);
		if( hasz && ! in_hasz )
			pt.z = 0.0;
		if( hasm && ! in_hasm )
			pt.m = 0.0;
		ptarray_append_point(pa_out, &pt, REPEATED_POINTS_OK);
	} 

	return pa_out;
}

/**
* @brief Calculate the #BOX3D bounding box of a set of points
* @return An lwalloc'ed #BOX3D, or NULL on empty array.
*         zmin / zmax values are set to #NO_Z_VALUE if not available.
*/
BOX3D *
ptarray_compute_box3d(const POINTARRAY *pa)
{
	BOX3D *result = lwalloc(sizeof(BOX3D));

	if ( ! ptarray_compute_box3d_p(pa, result) )
	{
		lwfree(result);
		return NULL;
	}

	return result;
}

/**
* @brief calculate the #BOX3D bounding box of a set of points
* 		zmin/zmax values are set to #NO_Z_VALUE if not available.
* write result to the provided #BOX3D
* @return 0 if bounding box is NULL (empty geom) and 1 otherwise
*/
int
ptarray_compute_box3d_p(const POINTARRAY *pa, BOX3D *result)
{
	int t;
	POINT3DZ pt;

	LWDEBUGF(3, "ptarray_compute_box3d call (array has %d points)", pa->npoints);

	if (pa->npoints == 0) return 0;

	getPoint3dz_p(pa, 0, &pt);

	LWDEBUG(3, "got point 0");

	result->xmin = pt.x;
	result->xmax = pt.x;
	result->ymin = pt.y;
	result->ymax = pt.y;

	if ( FLAGS_GET_Z(pa->dims) )
	{
		result->zmin = pt.z;
		result->zmax = pt.z;
	}
	else
	{
		result->zmin = NO_Z_VALUE;
		result->zmax = NO_Z_VALUE;
	}

	LWDEBUGF(3, "scanning other %d points", pa->npoints);

	for (t=1; t<pa->npoints; t++)
	{
		getPoint3dz_p(pa,t,&pt);
		if (pt.x < result->xmin) result->xmin = pt.x;
		if (pt.y < result->ymin) result->ymin = pt.y;
		if (pt.x > result->xmax) result->xmax = pt.x;
		if (pt.y > result->ymax) result->ymax = pt.y;

		if ( FLAGS_GET_Z(pa->dims) )
		{
			if (pt.z > result->zmax) result->zmax = pt.z;
			if (pt.z < result->zmin) result->zmin = pt.z;
		}
	}

	LWDEBUG(3, "returning box");

	return 1;
}

POINTARRAY *
ptarray_substring(POINTARRAY *ipa, double from, double to)
{
	POINTARRAY *dpa;
	POINT4D pt;
	POINT4D p1, p2;
	POINT4D *p1ptr=&p1; /* don't break strict-aliasing rule */
	POINT4D *p2ptr=&p2;
	int nsegs, i;
	double length, slength, tlength;
	int state = 0; /* 0=before, 1=inside */

	/*
	 * Create a dynamic pointarray with an initial capacity
	 * equal to full copy of input points
	 */
	dpa = ptarray_construct_empty(FLAGS_GET_Z(ipa->dims), FLAGS_GET_M(ipa->dims), ipa->npoints);

	/* Compute total line length */
	length = lwgeom_pointarray_length2d(ipa);


	LWDEBUGF(3, "Total length: %g", length);


	/* Get 'from' and 'to' lengths */
	from = length*from;
	to = length*to;


	LWDEBUGF(3, "From/To: %g/%g", from, to);


	tlength = 0;
	getPoint4d_p(ipa, 0, &p1);
	nsegs = ipa->npoints - 1;
	for ( i = 0; i < nsegs; i++ )
	{
		double dseg;

		getPoint4d_p(ipa, i+1, &p2);


		LWDEBUGF(3 ,"Segment %d: (%g,%g,%g,%g)-(%g,%g,%g,%g)",
		         i, p1.x, p1.y, p1.z, p1.m, p2.x, p2.y, p2.z, p2.m);


		/* Find the length of this segment */
		slength = distance2d_pt_pt((POINT2D *)p1ptr, (POINT2D *)p2ptr);

		/*
		 * We are before requested start.
		 */
		if ( state == 0 ) /* before */
		{

			LWDEBUG(3, " Before start");

			/*
			 * Didn't reach the 'from' point,
			 * nothing to do
			 */
			if ( from > tlength + slength ) goto END;

			else if ( from == tlength + slength )
			{

				LWDEBUG(3, "  Second point is our start");

				/*
				 * Second point is our start
				 */
				ptarray_append_point(dpa, &p2, REPEATED_POINTS_OK);
				state=1; /* we're inside now */
				goto END;
			}

			else if ( from == tlength )
			{

				LWDEBUG(3, "  First point is our start");

				/*
				 * First point is our start
				 */
				ptarray_append_point(dpa, &p1, REPEATED_POINTS_OK);

				/*
				 * We're inside now, but will check
				 * 'to' point as well
				 */
				state=1;
			}

			else  /* tlength < from < tlength+slength */
			{

				LWDEBUG(3, "  Seg contains first point");

				/*
				 * Our start is between first and
				 * second point
				 */
				dseg = (from - tlength) / slength;
				interpolate_point4d(&p1, &p2, &pt, dseg);

				ptarray_append_point(dpa, &pt, REPEATED_POINTS_OK);

				/*
				 * We're inside now, but will check
				 * 'to' point as well
				 */
				state=1;
			}
		}

		if ( state == 1 ) /* inside */
		{

			LWDEBUG(3, " Inside");

			/*
			 * Didn't reach the 'end' point,
			 * just copy second point
			 */
			if ( to > tlength + slength )
			{
				ptarray_append_point(dpa, &p2, REPEATED_POINTS_NOT_OK);
				goto END;
			}

			/*
			 * 'to' point is our second point.
			 */
			else if ( to == tlength + slength )
			{

				LWDEBUG(3, " Second point is our end");

				ptarray_append_point(dpa, &p2, REPEATED_POINTS_NOT_OK);
				break; /* substring complete */
			}

			/*
			 * 'to' point is our first point.
			 * (should only happen if 'to' is 0)
			 */
			else if ( to == tlength )
			{

				LWDEBUG(3, " First point is our end");

				ptarray_append_point(dpa, &p1, REPEATED_POINTS_NOT_OK);

				break; /* substring complete */
			}

			/*
			 * 'to' point falls on this segment
			 * Interpolate and break.
			 */
			else if ( to < tlength + slength )
			{

				LWDEBUG(3, " Seg contains our end");

				dseg = (to - tlength) / slength;
				interpolate_point4d(&p1, &p2, &pt, dseg);

				ptarray_append_point(dpa, &pt, REPEATED_POINTS_NOT_OK);

				break;
			}

			else
			{
				LWDEBUG(3, "Unhandled case");
			}
		}


END:

		tlength += slength;
		memcpy(&p1, &p2, sizeof(POINT4D));
	}

	LWDEBUGF(3, "Out of loop, ptarray has %d points", dpa->npoints);

	return dpa;
}

/*
 * Write into the *ret argument coordinates of the closes point on
 * the given segment to the reference input point.
 */
void
closest_point_on_segment(POINT2D *p, POINT2D *A, POINT2D *B, POINT2D *ret)
{
	double r;

	if (  ( A->x == B->x) && (A->y == B->y) )
	{
		*ret = *A;
		return;
	}

	/*
	 * We use comp.graphics.algorithms Frequently Asked Questions method
	 *
	 * (1)           AC dot AB
	 *           r = ----------
	 *                ||AB||^2
	 *	r has the following meaning:
	 *	r=0 P = A
	 *	r=1 P = B
	 *	r<0 P is on the backward extension of AB
	 *	r>1 P is on the forward extension of AB
	 *	0<r<1 P is interior to AB
	 *
	 */
	r = ( (p->x-A->x) * (B->x-A->x) + (p->y-A->y) * (B->y-A->y) )/( (B->x-A->x)*(B->x-A->x) +(B->y-A->y)*(B->y-A->y) );

	if (r<0)
	{
		*ret = *A;
		return;
	}
	if (r>1)
	{
		*ret = *B;
		return;
	}

	ret->x = A->x + ( (B->x - A->x) * r );
	ret->y = A->y + ( (B->y - A->y) * r );
}

/*
 * Given a point, returns the location of closest point on pointarray
 * and, optionally, it's actual distance from the point array.
 */
double
ptarray_locate_point(POINTARRAY *pa, POINT2D *p, double* mindistout)
{
	double mindist=-1;
	double tlen, plen;
	int t, seg=-1;
	POINT2D	start, end;
	POINT2D proj;

	getPoint2d_p(pa, 0, &start);
	for (t=1; t<pa->npoints; t++)
	{
		double dist;
		getPoint2d_p(pa, t, &end);
		dist = distance2d_pt_seg(p, &start, &end);

		if (t==1 || dist < mindist )
		{
			mindist = dist;
			seg=t-1;
		}

		if ( mindist == 0 ) break;

		start = end;
	}

	LWDEBUGF(3, "Closest segment: %d", seg);

	/*
	 * If mindist is not 0 we need to project the
	 * point on the closest segment.
	 */
	if ( mindist > 0 )
	{
		getPoint2d_p(pa, seg, &start);
		getPoint2d_p(pa, seg+1, &end);
		closest_point_on_segment(p, &start, &end, &proj);
	}
	else
	{
		proj = *p;
	}

	LWDEBUGF(3, "Closest point on segment: %g,%g", proj.x, proj.y);

	tlen = lwgeom_pointarray_length2d(pa);

	LWDEBUGF(3, "tlen %g", tlen);

	plen=0;
	getPoint2d_p(pa, 0, &start);
	for (t=0; t<seg; t++, start=end)
	{
		getPoint2d_p(pa, t+1, &end);
		plen += distance2d_pt_pt(&start, &end);

		LWDEBUGF(4, "Segment %d made plen %g", t, plen);
	}

	plen+=distance2d_pt_pt(&proj, &start);

	LWDEBUGF(3, "plen %g, tlen %g", plen, tlen);
	LWDEBUGF(3, "mindist: %g", mindist);

	if ( mindistout ) *mindistout = mindist;

	return plen/tlen;
}

/**
 * @brief Longitude shift for a pointarray.
 *  	Y remains the same
 *  	X is converted:
 *	 		from -180..180 to 0..360
 *	 		from 0..360 to -180..180
 *  	X < 0 becomes X + 360
 *  	X > 180 becomes X - 360
 */
void
ptarray_longitude_shift(POINTARRAY *pa)
{
	int i;
	double x;

	for (i=0; i<pa->npoints; i++)
	{
		memcpy(&x, getPoint_internal(pa, i), sizeof(double));
		if ( x < 0 ) x+= 360;
		else if ( x > 180 ) x -= 360;
		memcpy(getPoint_internal(pa, i), &x, sizeof(double));
	}
}


/*
 * Returns a POINTARRAY with consecutive equal points
 * removed. Equality test on all dimensions of input.
 *
 * Always returns a newly allocated object.
 *
 */
POINTARRAY *
ptarray_remove_repeated_points(POINTARRAY *in)
{
	POINTARRAY* out;
	size_t ptsize;
	size_t ipn, opn;

	LWDEBUG(3, "ptarray_remove_repeated_points called.");

	/* Single or zero point arrays can't have duplicates */
	if ( in->npoints < 3 ) return ptarray_clone(in);

	ptsize = ptarray_point_size(in);

	LWDEBUGF(3, "ptsize: %d", ptsize);

	/* Allocate enough space for all points */
	out = ptarray_construct(FLAGS_GET_Z(in->dims),
	                        FLAGS_GET_M(in->dims), in->npoints);

	/* Now fill up the actual points (NOTE: could be optimized) */

	opn=1;
	memcpy(getPoint_internal(out, 0), getPoint_internal(in, 0), ptsize);
	LWDEBUGF(3, " first point copied, out points: %d", opn);
	for (ipn=1; ipn<in->npoints; ++ipn)
	{
		if ( (ipn==in->npoints-1 && opn==1) || memcmp(getPoint_internal(in, ipn-1),
		        getPoint_internal(in, ipn), ptsize) )
		{
			/* The point is different from the previous,
			 * we add it to output */
			memcpy(getPoint_internal(out, opn++),
			       getPoint_internal(in, ipn), ptsize);
			LWDEBUGF(3, " Point %d differs from point %d. Out points: %d",
			         ipn, ipn-1, opn);
		}
	}

	LWDEBUGF(3, " in:%d out:%d", out->npoints, opn);
	out->npoints = opn;

	return out;
}

static void
ptarray_dp_findsplit(POINTARRAY *pts, int p1, int p2, int *split, double *dist)
{
	int k;
	POINT2D pa, pb, pk;
	double tmp;

	LWDEBUG(4, "function called");

	*dist = -1;
	*split = p1;

	if (p1 + 1 < p2)
	{

		getPoint2d_p(pts, p1, &pa);
		getPoint2d_p(pts, p2, &pb);

		LWDEBUGF(4, "P%d(%f,%f) to P%d(%f,%f)",
		         p1, pa.x, pa.y, p2, pb.x, pb.y);

		for (k=p1+1; k<p2; k++)
		{
			getPoint2d_p(pts, k, &pk);

			LWDEBUGF(4, "P%d(%f,%f)", k, pk.x, pk.y);

			/* distance computation */
			tmp = distance2d_pt_seg(&pk, &pa, &pb);

			if (tmp > *dist)
			{
				*dist = tmp;	/* record the maximum */
				*split = k;

				LWDEBUGF(4, "P%d is farthest (%g)", k, *dist);
			}
		}

	} /* length---should be redone if can == 0 */
	else
	{
		LWDEBUG(3, "segment too short, no split/no dist");
	}

}

POINTARRAY *
ptarray_simplify(POINTARRAY *inpts, double epsilon)
{
	int *stack;			/* recursion stack */
	int sp=-1;			/* recursion stack pointer */
	int p1, split;
	double dist;
	POINTARRAY *outpts;
	POINT4D pt;

	/* Allocate recursion stack */
	stack = lwalloc(sizeof(int)*inpts->npoints);

	p1 = 0;
	stack[++sp] = inpts->npoints-1;

	LWDEBUGF(2, "Input has %d pts and %d dims", inpts->npoints, inpts->dims);

	/* Allocate output POINTARRAY, and add first point. */
	outpts = ptarray_construct_empty(FLAGS_GET_Z(inpts->dims), FLAGS_GET_M(inpts->dims), inpts->npoints);
	getPoint4d_p(inpts, 0, &pt);
	ptarray_append_point(outpts, &pt, REPEATED_POINTS_NOT_OK);

	LWDEBUG(3, "Added P0 to simplified point array (size 1)");

	do
	{

		ptarray_dp_findsplit(inpts, p1, stack[sp], &split, &dist);

		LWDEBUGF(3, "Farthest point from P%d-P%d is P%d (dist. %g)", p1, stack[sp], split, dist);

		if (dist > epsilon)
		{
			stack[++sp] = split;
		}
		else
		{
			getPoint4d_p(inpts, stack[sp], &pt);
			ptarray_append_point(outpts, &pt, REPEATED_POINTS_NOT_OK);
			
			LWDEBUGF(4, "Added P%d to simplified point array (size: %d)", stack[sp], outpts->npoints);

			p1 = stack[sp--];
		}

		LWDEBUGF(4, "stack pointer = %d", sp);
	}
	while (! (sp<0) );

	lwfree(stack);
	return outpts;
}

int
p4d_same(POINT4D p1, POINT4D p2)
{
	if( p1.x == p2.x && p1.y == p2.y && p1.z == p2.z && p1.m == p2.m )
		return LW_TRUE;
	else
		return LW_FALSE;
}
