/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2010 Olivier Courtin <olivier.courtin@oslandia.com>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include <stdlib.h>
#include "liblwgeom.h"
#include "lwhomogenize.h"


/*
 * Known limitation: not (yet ?) support SQL/MM Curves.
 */


/*
** Given a generic geometry, return the "simplest" form.
**
** eg:
**     LINESTRING() => LINESTRING()
**
**     MULTILINESTRING(with a single line) => LINESTRING()
**
**     GEOMETRYCOLLECTION(MULTILINESTRING()) => MULTILINESTRING()
**
**     GEOMETRYCOLLECTION(MULTILINESTRING(), MULTILINESTRING(), POINT())
**      => GEOMETRYCOLLECTION(MULTILINESTRING(), POINT())
*/
LWGEOM *
lwgeom_homogenize(const LWGEOM *geom)
{
	LWGEOM *hgeom;
	uint32 type=TYPE_GETTYPE(geom->type);

	/* EMPTY Geometry */
	if (lwgeom_is_empty(geom)) return lwgeom_clone(geom);

	/* Already a simple Geometry */
	switch (type)
	{
	case POINTTYPE:
	case LINETYPE:
	case POLYGONTYPE:
		return lwgeom_clone(geom);
	}

	/* A MULTI */
	switch (type)
	{
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:

		/* A MULTI with a single geometry inside */
		if (((LWCOLLECTION *) geom)->ngeoms == 1)
		{

			hgeom = lwgeom_clone((LWGEOM *)
			                     ((LWCOLLECTION *)geom)->geoms[0]);

			hgeom->SRID = geom->SRID;
			if (geom->bbox)
				hgeom->bbox = box2d_clone(geom->bbox);

			return hgeom;
		}

		/* A 'real' MULTI */
		return lwgeom_clone(geom);
	}

	if (type == COLLECTIONTYPE)
		return lwcollection_homogenize((LWCOLLECTION *) geom);

	lwerror("lwgeom_homogenize: Geometry Type not supported (%i)",
	        lwtype_name(type));

	return NULL; /*Never reach */
}


/*
** Given a generic collection, return the "simplest" form.
**
** eg: GEOMETRYCOLLECTION(MULTILINESTRING()) => MULTILINESTRING()
**
**     GEOMETRYCOLLECTION(MULTILINESTRING(), MULTILINESTRING(), POINT())
**      => GEOMETRYCOLLECTION(MULTILINESTRING(), POINT())
**
** In general, if the subcomponents are homogeneous, return a properly
** typed collection.
** Otherwise, return a generic collection, with the subtypes in minimal
** typed collections.
*/
LWGEOM *
lwcollection_homogenize(const LWCOLLECTION *col)
{
	uint32 i, srid;
	uchar hasz, hasm;
	LWGEOM *res = NULL;
	LWCOLLECTION *coll;
	LWGEOM_HOMOGENIZE *geoms;

	if (!col) lwerror("lwcollection_homogenize: Null input geometry.");

	/* EMPTY Geometry case */
	if (col->ngeoms == 0)
		return (LWGEOM *) lwcollection_construct_empty(col->SRID, 0, 0);

	hasz = TYPE_HASZ(col->type);
	hasm = TYPE_HASM(col->type);
	srid = col->SRID;

	/* LWGEOM_HOMOGENIZE struct setup */
	geoms = lwalloc(sizeof(LWGEOM_HOMOGENIZE));
	geoms->points = lwmpoint_construct_empty(-1, hasz, hasm);
	geoms->lines = lwmline_construct_empty(-1, hasz, hasm);
	geoms->polys = lwmpoly_construct_empty(-1, hasz, hasm);

	LWDEBUGF(4, "geoms->points %p", geoms->points);

	/* Parse each sub geom and update LWGEOM_HOMOGENIZE struct */
	for (i=0 ; i < col->ngeoms ; i++)
		geoms = lwcollection_homogenize_subgeom(geoms, col->geoms[i]);

	/* Check if struct is mixed typed, and need a COLLECTION as output */
	if ((geoms->points->ngeoms && geoms->lines->ngeoms) ||
	        (geoms->points->ngeoms && geoms->polys->ngeoms) ||
	        (geoms->lines->ngeoms  && geoms->polys->ngeoms))
	{
		LWDEBUGF(4,"geoms->points->ngeoms %d  geoms->lines->ngeoms %d  geoms->polys->ngeoms %d", geoms->points->ngeoms, geoms->lines->ngeoms, geoms->polys->ngeoms);
		coll = lwcollection_construct_empty(srid, hasz, hasm);

		LWDEBUGF(4,"coll->ngeoms %d", coll->ngeoms);

		if (col->bbox) coll->bbox = box2d_clone(col->bbox);

		if (geoms->points->ngeoms == 1)
			coll = lwcollection_add_lwgeom(coll, lwgeom_clone((LWGEOM*) geoms->points->geoms[0]));
		else if (geoms->points->ngeoms)
			coll = lwcollection_add_lwgeom(coll, lwgeom_clone((LWGEOM*) geoms->points));

		LWDEBUGF(4,"coll->ngeoms %d", coll->ngeoms);

		if (geoms->lines->ngeoms == 1)
			coll = lwcollection_add_lwgeom(coll, lwgeom_clone((LWGEOM*) geoms->lines->geoms[0]));
		else if (geoms->lines->ngeoms)
			coll = lwcollection_add_lwgeom(coll, lwgeom_clone((LWGEOM*) geoms->lines));

		LWDEBUGF(4,"coll->ngeoms %d", coll->ngeoms);

		if (geoms->polys->ngeoms == 1)
			coll = lwcollection_add_lwgeom(coll, lwgeom_clone((LWGEOM*) geoms->polys->geoms[0]));
		else if (geoms->polys->ngeoms)
			coll = lwcollection_add_lwgeom(coll, lwgeom_clone((LWGEOM*) geoms->polys));

		LWDEBUGF(4,"coll->ngeoms %d", coll->ngeoms);

		/* We could now free the struct */
		lwmpoint_release(geoms->points);
		lwmline_release(geoms->lines);
		lwmpoly_release(geoms->polys);
		lwfree(geoms);

		for ( i = 0; i < coll->ngeoms; i++ )
			LWDEBUGF(2,"TYPE_GETTYPE(coll->geoms[%d]->type) %d", i, TYPE_GETTYPE(coll->geoms[i]->type));


		return (LWGEOM *) coll;
	}

	/* Check if we have to return simple type (i.e not a MULTI) */
	if (geoms->points->ngeoms == 1)
		res = lwgeom_clone((LWGEOM *) geoms->points->geoms[0]);
	if (geoms->lines->ngeoms == 1)
		res = lwgeom_clone((LWGEOM *) geoms->lines->geoms[0]);
	if (geoms->polys->ngeoms == 1)
		res = lwgeom_clone((LWGEOM *) geoms->polys->geoms[0]);

	/* We have to return a single MULTI */
	if (geoms->points->ngeoms > 1)
		res = lwgeom_clone((LWGEOM *) geoms->points);
	if (geoms->lines->ngeoms > 1)
		res = lwgeom_clone((LWGEOM *) geoms->lines);
	if (geoms->polys->ngeoms > 1)
		res = lwgeom_clone((LWGEOM *) geoms->polys);

	/* We could now free the struct */
	lwmpoint_release(geoms->points);
	lwmline_release(geoms->lines);
	lwmpoly_release(geoms->polys);
	lwfree(geoms);

	/* Empty (and recursive) Geometry case */
	if (!res) return (LWGEOM *) lwcollection_construct_empty(col->SRID, 0, 0);

	/* Handle SRID and Bbox */
	res->SRID = srid;
	if (col->bbox) res->bbox = box2d_clone(col->bbox);

	return res;
}

static LWGEOM_HOMOGENIZE*
lwcollection_homogenize_subgeom(LWGEOM_HOMOGENIZE *hgeoms, LWGEOM *geom)
{
	uint32 i, type;

	if (!geom) lwerror("lwcollection_homogenize: Sub geometry is Null");

	type = TYPE_GETTYPE(geom->type);

	/* We manage the SRID in lwcollection_homogenize */
	geom->SRID = -1;

	if (type == POINTTYPE)
	{
		hgeoms->points = lwmpoint_add_lwpoint(hgeoms->points, (LWPOINT*)lwgeom_clone(geom));
	}
	else if (type == LINETYPE)
	{
		hgeoms->lines = lwmline_add_lwline(hgeoms->lines, (LWLINE*)lwgeom_clone(geom));
	}
	else if (type == POLYGONTYPE)
	{
		hgeoms->polys = lwmpoly_add_lwpoly(hgeoms->polys, (LWPOLY*)lwgeom_clone(geom));
	}
	else if ( lwgeom_is_collection(type) )
	{
		LWCOLLECTION *obj = (LWCOLLECTION*)geom;
		for (i=0; i < obj->ngeoms ; i++)
			hgeoms = lwcollection_homogenize_subgeom(hgeoms, obj->geoms[i]);
	}
	else
	{
		lwerror("lwcollection_homogenize: Unsupported geometry type");
	}

	return hgeoms;
}
