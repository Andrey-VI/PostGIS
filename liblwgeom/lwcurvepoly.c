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

/* basic LWCURVEPOLY manipulation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liblwgeom_internal.h"


LWCURVEPOLY *
lwcurvepoly_deserialize(uchar *srl)
{
	LWCURVEPOLY *result;
	LWGEOM_INSPECTED *insp;
	int type = lwgeom_getType(srl[0]);
	int i;

	LWDEBUG(3, "lwcurvepoly_deserialize called.");

	if (type != CURVEPOLYTYPE)
	{
		lwerror("lwcurvepoly_deserialize called on NON curvepoly: %d",
		        type);
		return NULL;
	}

	insp = lwgeom_inspect(srl);

	result = lwalloc(sizeof(LWCURVEPOLY));
	result->type = insp->type;
	result->SRID = insp->SRID;
	result->nrings = insp->ngeometries;
	result->rings = lwalloc(sizeof(LWGEOM *)*insp->ngeometries);

	if (lwgeom_hasBBOX(srl[0]))
	{
		result->bbox = lwalloc(sizeof(BOX2DFLOAT4));
		memcpy(result->bbox, srl + 1, sizeof(BOX2DFLOAT4));
	}
	else result->bbox = NULL;

	for (i = 0; i < insp->ngeometries; i++)
	{
		result->rings[i] = lwgeom_deserialize(insp->sub_geoms[i]);
		if (lwgeom_getType(result->rings[i]->type) != CIRCSTRINGTYPE
		        && lwgeom_getType(result->rings[i]->type) != LINETYPE
		        && lwgeom_getType(result->rings[i]->type) != COMPOUNDTYPE)
		{
			lwerror("Only Circular curves, Linestrings and Compound curves are supported as rings, not %s (%d)", lwtype_name(result->rings[i]->type), result->rings[i]->type);
			lwfree(result);
			lwfree(insp);
			return NULL;
		}
		if (TYPE_NDIMS(result->rings[i]->type) != TYPE_NDIMS(result->type))
		{
			lwerror("Mixed dimensions (curvepoly %d, ring %d)",
			        TYPE_NDIMS(result->type), i,
			        TYPE_NDIMS(result->rings[i]->type));
			lwfree(result);
			lwfree(insp);
			return NULL;
		}
	}
	return result;
}

LWCURVEPOLY *
lwcurvepoly_construct_empty(int srid, char hasz, char hasm)
{
	LWCURVEPOLY *ret;

	ret = lwalloc(sizeof(LWCURVEPOLY));
	ret->type = lwgeom_makeType_full(hasz, hasm, (srid!=-1), CURVEPOLYTYPE, 0);
	ret->SRID = srid;
	ret->nrings = 0;
	ret->maxrings = 1; /* Allocate room for sub-members, just in case. */
	ret->rings = lwalloc(ret->maxrings * sizeof(LWGEOM*));
	ret->bbox = NULL;

	return ret;
}

int lwcurvepoly_add_ring(LWCURVEPOLY *poly, LWGEOM *ring)
{
	int ringtype, i;
	
	/* Can't do anything with NULLs */
	if( ! poly || ! ring ) 
	{
		LWDEBUG(4,"NULL inputs!!! quitting");
		return LW_FALSE;
	}

	/* Check that we're not working with garbage */
	if ( poly->rings == NULL && (poly->nrings || poly->maxrings) )
	{
		LWDEBUG(4,"mismatched nrings/maxrings");
		lwerror("Curvepolygon is in inconsistent state. Null memory but non-zero collection counts.");
	}

	/* Check that we're adding an allowed ring type */
	ringtype = TYPE_GETTYPE(ring->type);
	if ( ! ( ringtype == LINETYPE || ringtype == CIRCSTRINGTYPE || ringtype == COMPOUNDTYPE ) )
	{
		LWDEBUGF(4,"got incorrect ring type: %s",lwtype_name(ringtype));
		return LW_FALSE;
	}

		
	/* In case this is a truly empty, make some initial space  */
	if ( poly->rings == NULL )
	{
		poly->maxrings = 2;
		poly->nrings = 0;
		poly->rings = lwalloc(poly->maxrings * sizeof(LWGEOM*));
	}

	/* Allocate more space if we need it */
	if ( poly->nrings == poly->maxrings )
	{
		poly->maxrings *= 2;
		poly->rings = lwrealloc(poly->rings, sizeof(LWGEOM*) * poly->maxrings);
	}

	/* Make sure we don't already have a reference to this geom */
	for ( i = 0; i < poly->nrings; i++ )
	{
		if ( poly->rings[i] == ring )
		{
			LWDEBUGF(4, "Found duplicate geometry in collection %p == %p", poly->rings[i], ring);
			return LW_TRUE;
		}
	}

	/* Add the ring and increment the ring count */
	poly->rings[poly->nrings] = (LWGEOM*)ring;
	poly->nrings++;
	return LW_TRUE;	
}


