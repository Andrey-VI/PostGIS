/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * Copyright 2009 Paul Ramsey <pramsey@cleverelephant.ca>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include <math.h>
#include "libgeom.h"

/* 
* This function can only be used on LWGEOM that is built on top of
* GSERIALIZED, otherwise alignment errors will ensue.
*/
int getPoint2d_p_ro(const POINTARRAY *pa, int n, POINT2D **point)
{
	uchar *pa_ptr = NULL;
	assert(pa);
	assert(n >= 0);
	assert(n < pa->npoints);

	pa_ptr = getPoint_internal(pa, n);
	//printf( "pa_ptr[0]: %g\n", *((double*)pa_ptr));
	*point = (POINT2D*)pa_ptr;

	return G_SUCCESS;
}

int ptarray_calculate_gbox_geodetic(POINTARRAY *pa, GBOX *gbox)
{
	//static double wgs84_radius = 6371008.7714;
	int i;
/*	int pt_spacing; */
	double *sphericalcoords;
	POINT2D *pt;

	assert(gbox);
	assert(pa);
	
/*	pt_spacing = 2;
	if( TYPE_HASZ(pa->dims) ) pt_spacing++;
	if( TYPE_HASM(pa->dims) ) pt_spacing++;
*/
	
	if ( pa->npoints == 0 ) return G_FAILURE;
	
	sphericalcoords = (double*)(pa->serialized_pointlist);
	
	for( i = 0; i < pa->npoints; i++ )
	{
		getPoint2d_p_ro(pa, i, &pt);
		/* Convert to radians */
		/* register double theta = sphericalcoords[pt_spacing*i] * PI / 180.0; */
		/* register double phi = sphericalcoords[pt_spacing*i + 1] * PI / 180; */
		register double theta = pt->x * PI / 180.0;
		register double phi = pt->y * PI / 180;
		/* Convert to geocentric */
		register double x = cos(theta);
		register double y = sin(theta);
		register double z = sin(phi);
		/* Initialize the box */
		if( i == 0 )
		{ 
			gbox->xmin = gbox->xmax = x;
			gbox->ymin = gbox->ymax = y;
			gbox->zmin = gbox->zmax = z;
			gbox->mmin = gbox->mmax = 0.0;
		}
		/* Expand the box where necessary */
		if( x < gbox->xmin ) gbox->xmin = x;
		if( y < gbox->ymin ) gbox->ymin = y;
		if( z < gbox->zmin ) gbox->zmin = z;
		if( x > gbox->xmax ) gbox->xmax = x;
		if( y > gbox->ymax ) gbox->ymax = y;
		if( z > gbox->zmax ) gbox->zmax = z;
	}
	
	return G_SUCCESS;
	
}

static int lwpoint_calculate_gbox_geodetic(LWPOINT *point, GBOX *gbox)
{
	assert(point);
	if( ptarray_calculate_gbox_geodetic(point->point, gbox) == G_FAILURE )
		return G_FAILURE;
	return G_SUCCESS;
}

static int lwline_calculate_gbox_geodetic(LWLINE *line, GBOX *gbox)
{
	assert(line);
	if( ptarray_calculate_gbox_geodetic(line->points, gbox) == G_FAILURE )
		return G_FAILURE;
	return G_SUCCESS;
}

static int lwpolygon_calculate_gbox_geodetic(LWPOLY *poly, GBOX *gbox)
{
	GBOX ringbox;
	int i;
	int first = G_TRUE;
	assert(poly);
	if( poly->nrings == 0 )
		return G_FAILURE;
	ringbox.flags = gbox->flags;
	for( i = 0; i < poly->nrings; i++ )
	{
		if( ptarray_calculate_gbox_geodetic(poly->rings[i], &ringbox) == G_FAILURE )
			return G_FAILURE;
		if( first )
		{
			gbox_duplicate(gbox, &ringbox);
			first = G_FALSE;
		}
		else
		{
			gbox_merge(gbox, &ringbox);
		}
	}
	return G_SUCCESS;
}

static int lwcollection_calculate_gbox_geodetic(LWCOLLECTION *coll, GBOX *gbox)
{
	GBOX subbox;
	int i;
	int result = G_FAILURE;
	int first = G_TRUE;
	assert(coll);
	if( coll->ngeoms == 0 )
		return G_FAILURE;
	
	subbox.flags = gbox->flags;
	
	for( i = 0; i < coll->ngeoms; i++ )
	{
		if( lwgeom_calculate_gbox_geodetic((LWGEOM*)(coll->geoms[i]), &subbox) == G_FAILURE )
		{
			continue;
		}
		else
		{
			if( first )
			{
				gbox_duplicate(gbox, &subbox);
				first = G_FALSE;
			}
			else
			{
				gbox_merge(gbox, &subbox);
			}
			result = G_SUCCESS;
		}
	}
	return result;
}

int lwgeom_calculate_gbox_geodetic(const LWGEOM *geom, GBOX *gbox)
{
	int result = G_FAILURE;
	LWDEBUGF(4, "got type %d", TYPE_GETTYPE(geom->type));
	if ( ! FLAGS_GET_GEODETIC(gbox->flags) )
	{
		lwerror("lwgeom_get_gbox_geodetic: non-geodetic gbox provided");
	}
	switch (TYPE_GETTYPE(geom->type))
	{
		case POINTTYPE:
			result = lwpoint_calculate_gbox_geodetic((LWPOINT*)geom, gbox);
			break;
		case LINETYPE:
			result = lwline_calculate_gbox_geodetic((LWLINE *)geom, gbox);
			break;
		case POLYGONTYPE:
			result = lwpolygon_calculate_gbox_geodetic((LWPOLY *)geom, gbox);
			break;
		case MULTIPOINTTYPE:
		case MULTILINETYPE:
		case MULTIPOLYGONTYPE:
		case COLLECTIONTYPE:
			result = lwcollection_calculate_gbox_geodetic((LWCOLLECTION *)geom, gbox);
			break;
		default:
			lwerror("unsupported input geometry type: %d", TYPE_GETTYPE(geom->type));
			break;
	}
	return result;
}



static int ptarray_check_geodetic(POINTARRAY *pa)
{
	int t;
	POINT2D *pt;

	assert(pa);

	for (t=0; t<pa->npoints; t++)
	{
		getPoint2d_p_ro(pa, t, &pt);
		//printf( "%d (%g, %g)\n", t, pt->x, pt->y);
		if ( pt->x < -180.0 || pt->y < -90.0 || pt->x > 180.0 || pt->y > 90.0 )
			return G_FALSE;
	}

	return G_TRUE;
}

static int lwpoint_check_geodetic(LWPOINT *point)
{
	assert(point);
	return ptarray_check_geodetic(point->point);
}

static int lwline_check_geodetic(LWLINE *line)
{
	assert(line);
	return ptarray_check_geodetic(line->points);
}

static int lwpoly_check_geodetic(LWPOLY *poly)
{
	int i = 0;
	assert(poly);
	
	for( i = 0; i < poly->nrings; i++ )
	{
		if( ptarray_check_geodetic(poly->rings[i]) == G_FALSE )
			return G_FALSE;
	}
	return G_TRUE;
}

static int lwcollection_check_geodetic(LWCOLLECTION *col)
{
	int i = 0;
	assert(col);
	
	for( i = 0; i < col->ngeoms; i++ )
	{
		if( lwgeom_check_geodetic(col->geoms[i]) == G_FALSE )
			return G_FALSE;
	}
	return G_TRUE;
}

int lwgeom_check_geodetic(const LWGEOM *geom)
{
	switch (TYPE_GETTYPE(geom->type))
	{
		case POINTTYPE:
			return lwpoint_check_geodetic((LWPOINT *)geom);
		case LINETYPE:
			return lwline_check_geodetic((LWLINE *)geom);
		case POLYGONTYPE:
			return lwpoly_check_geodetic((LWPOLY *)geom);
		case MULTIPOINTTYPE:
		case MULTILINETYPE:
		case MULTIPOLYGONTYPE:
		case COLLECTIONTYPE:
			return lwcollection_check_geodetic((LWCOLLECTION *)geom);
		default:
			lwerror("unsupported input geometry type: %d", TYPE_GETTYPE(geom->type));
	}
	return G_FALSE;
}


/*
** Count points in an LWGEOM. 
*/

static int lwcollection_count_vertices(LWCOLLECTION *col)
{
	int i = 0;
	int v = 0; /* vertices */
	assert(col);
	for ( i = 0; i < col->ngeoms; i++ )
	{
		v += lwgeom_count_vertices(col->geoms[i]);
	}
	return v;
}

static int lwpolygon_count_vertices(LWPOLY *poly)
{
	int i = 0;
	int v = 0; /* vertices */
	assert(poly);
	for ( i = 0; i < poly->nrings; i ++ )
	{
		v += poly->rings[i]->npoints;
	}
	return v;
}

static int lwline_count_vertices(LWLINE *line)
{
	assert(line);
	if ( ! line->points )
		return 0;
	return line->points->npoints;
}

static int lwpoint_count_vertices(LWPOINT *point)
{
	assert(point);
	if ( ! point->point )
		return 0;
	return 1;
}

int lwgeom_count_vertices(LWGEOM *geom)
{
	int result = 0;
	LWDEBUGF(4, "got type %d", TYPE_GETTYPE(geom->type));
	switch (TYPE_GETTYPE(geom->type))
	{
		case POINTTYPE:
			result = lwpoint_count_vertices((LWPOINT *)geom);;
			break;
		case LINETYPE:
			result = lwline_count_vertices((LWLINE *)geom);
			break;
		case POLYGONTYPE:
			result = lwpolygon_count_vertices((LWPOLY *)geom);
			break;
		case MULTIPOINTTYPE:
		case MULTILINETYPE:
		case MULTIPOLYGONTYPE:
		case COLLECTIONTYPE:
			result = lwcollection_count_vertices((LWCOLLECTION *)geom);
			break;
		default:
			lwerror("unsupported input geometry type: %d", TYPE_GETTYPE(geom->type));
			break;
	}
	LWDEBUGF(3, "counted %d vertices", result);
	return result;
}

int lwgeom_needs_bbox(LWGEOM *geom)
{
	assert(geom);
	if( TYPE_GETTYPE(geom->type) == POINTTYPE )
	{
		return G_FALSE;
	}
	return G_TRUE;
}





