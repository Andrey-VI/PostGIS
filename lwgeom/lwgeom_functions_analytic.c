#include "postgres.h"
#include "fmgr.h"
#include "liblwgeom.h"
#include "lwgeom_pg.h"
#include "math.h"

/***********************************************************************
 * Simple Douglas-Peucker line simplification. 
 * No checks are done to avoid introduction of self-intersections.
 * No topology relations are considered.
 *
 * --strk@keybit.net;
 ***********************************************************************/

#define VERBOSE 0

#if VERBOSE > 0
#define REPORT_POINTS_REDUCTION
#define REPORT_RINGS_REDUCTION
#define REPORT_RINGS_ADJUSTMENTS
#endif

/* Prototypes */
void DP_findsplit2d(POINTARRAY *pts, int p1, int p2, int *split, double *dist);
POINTARRAY *DP_simplify2d(POINTARRAY *inpts, double epsilon);
LWLINE *simplify2d_lwline(LWLINE *iline, double dist);
LWPOLY *simplify2d_lwpoly(LWPOLY *ipoly, double dist);
Datum LWGEOM_simplify2d(PG_FUNCTION_ARGS);


/*
 * Search farthest point from segment p1-p2
 * returns distance in an int pointer
 */
void
DP_findsplit2d(POINTARRAY *pts, int p1, int p2, int *split, double *dist)
{
   int k;
   POINT2D *pa, *pb, *pk;
   double tmp;

#if VERBOSE > 4
elog(NOTICE, "DP_findsplit called");
#endif

   *dist = -1;
   *split = p1;

   if (p1 + 1 < p2)
   {

      pa = (POINT2D *)getPoint(pts, p1);
      pb = (POINT2D *)getPoint(pts, p2);

#if VERBOSE > 4
elog(NOTICE, "DP_findsplit: P%d(%f,%f) to P%d(%f,%f)",
   p1, pa->x, pa->y, p2, pb->x, pb->y);
#endif

      for (k=p1+1; k<p2; k++)
      {
         pk = (POINT2D *)getPoint(pts, k);

#if VERBOSE > 4
elog(NOTICE, "DP_findsplit: P%d(%f,%f)", k, pk->x, pk->y);
#endif

         /* distance computation */
         tmp = distance2d_pt_seg(pk, pa, pb);

         if (tmp > *dist) 
         {
            *dist = tmp;	/* record the maximum */
            *split = k;
#if VERBOSE > 4
elog(NOTICE, "DP_findsplit: P%d is farthest (%g)", k, *dist);
#endif
         }
      }

   } /* length---should be redone if can == 0 */

   else
   {
#if VERBOSE > 3
elog(NOTICE, "DP_findsplit: segment too short, no split/no dist");
#endif
   }

}


POINTARRAY *
DP_simplify2d(POINTARRAY *inpts, double epsilon)
{
	int stack[inpts->npoints];	/* recursion stack */
	int sp=-1;			/* recursion stack pointer */
	int p1, split; 
	double dist;
	POINTARRAY *outpts;
	int ptsize = pointArray_ptsize(inpts);

	p1 = 0;
	stack[++sp] = inpts->npoints-1;

#if VERBOSE > 4
	elog(NOTICE, "DP_simplify called input has %d pts and %d dims (ptsize: %d)", inpts->npoints, inpts->ndims, ptsize);
#endif

	// allocate space for output POINTARRAY
	outpts = palloc(sizeof(POINTARRAY));
	outpts->dims = inpts->dims;
	outpts->npoints=1;
	outpts->serialized_pointlist = (char *)palloc(ptsize*inpts->npoints);
	memcpy(getPoint(outpts, 0), getPoint(inpts, 0), ptsize);

#if VERBOSE > 3
	elog(NOTICE, "DP_simplify: added P0 to simplified point array (size 1)");
#endif


	do
	{

		DP_findsplit2d(inpts, p1, stack[sp], &split, &dist);
#if VERBOSE > 3
		elog(NOTICE, "DP_simplify: farthest point from P%d-P%d is P%d (dist. %g)", p1, stack[sp], split, dist);
#endif

		if (dist > epsilon) {
			stack[++sp] = split;
		} else {
			outpts->npoints++;
			memcpy(getPoint(outpts, outpts->npoints-1),
				getPoint(inpts, stack[sp]),
				ptsize);
#if VERBOSE > 3
			elog(NOTICE, "DP_simplify: added P%d to simplified point array (size: %d)", stack[sp], outpts->npoints);
#endif
			p1 = stack[sp--];
		}
#if VERBOSE > 5
		elog(NOTICE, "stack pointer = %d", sp);
#endif
	}
	while (! (sp<0) );

	/*
	 * If we have reduced the number of points realloc
	 * outpoints array to free up some memory.
	 * Might be turned on and off with a SAVE_MEMORY define ...
	 */
	if ( outpts->npoints < inpts->npoints )
	{
		outpts->serialized_pointlist = (char *)repalloc(
			outpts->serialized_pointlist,
			ptsize*outpts->npoints);
		if ( outpts->serialized_pointlist == NULL ) {
			elog(ERROR, "Out of virtual memory");
		}
	}

	return outpts;
}

LWLINE *
simplify2d_lwline(LWLINE *iline, double dist)
{
	POINTARRAY *ipts;
	POINTARRAY *opts;
	LWLINE *oline;

#if VERBOSE
   elog(NOTICE, "simplify2d_lwline called");
#endif

	ipts = iline->points;
	opts = DP_simplify2d(ipts, dist);
	oline = lwline_construct(iline->SRID, NULL, opts);

	return oline;
}

// TODO
LWPOLY *
simplify2d_lwpoly(LWPOLY *ipoly, double dist)
{
	POINTARRAY *ipts;
	POINTARRAY **orings = NULL;
	LWPOLY *opoly;
	int norings=0, ri;

#ifdef REPORT_RINGS_REDUCTION
	elog(NOTICE, "simplify_polygon3d: simplifying polygon with %d rings", ipoly->nrings);
#endif

	orings = (POINTARRAY **)palloc(sizeof(POINTARRAY *)*ipoly->nrings);

	for (ri=0; ri<ipoly->nrings; ri++)
	{
		POINTARRAY *opts;

		ipts = ipoly->rings[ri];

		opts = DP_simplify2d(ipts, dist);


		if ( opts->npoints < 2 )
		{
			/* There as to be an error in DP_simplify */
			elog(NOTICE, "DP_simplify returned a <2 pts array");
			pfree(opts);
			continue;
		}

		if ( opts->npoints < 4 )
		{
			pfree(opts);
#ifdef REPORT_RINGS_ADJUSTMENTS
			elog(NOTICE, "simplify_polygon3d: ring%d skipped ( <4 pts )", ri);
#endif

			if ( ri ) continue;
			else break;
		}


#ifdef REPORT_POINTS_REDUCTION
		elog(NOTICE, "simplify_polygon3d: ring%d simplified from %d to %d points", ri, ipts->npoints, opts->npoints);
#endif


		/*
		 * Add ring to simplified ring array
		 * (TODO: dinamic allocation of pts_per_ring)
		 */
		orings[norings] = opts;
		norings++;

	}

#ifdef REPORT_RINGS_REDUCTION
elog(NOTICE, "simplify_polygon3d: simplified polygon with %d rings", norings);
#endif

	if ( ! norings ) return NULL;

	opoly = lwpoly_construct(ipoly->SRID, NULL, norings, orings);

	return opoly;
}

PG_FUNCTION_INFO_V1(LWGEOM_simplify2d);
Datum LWGEOM_simplify2d(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom = (PG_LWGEOM *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	LWGEOM_EXPLODED *exp = lwgeom_explode(SERIALIZED_FORM(geom));
	double dist = PG_GETARG_FLOAT8(1);
	int i;
	char **newlines;
	int newlinesnum=0;
	char **newpolys;
	int newpolysnum=0;
	PG_LWGEOM *result;
	char *serialized;

	// no lines, no points... return input
	if ( exp->nlines + exp->npolys == 0 )
	{
		pfree_exploded(exp);
	 	PG_RETURN_POINTER(geom);
	}

	if ( exp->nlines )
	{
#if VERBOSE
		elog(NOTICE, "%d lines in exploded geom", exp->nlines);
#endif
		newlines = palloc(sizeof(char *)*exp->nlines);
		for ( i=0; i<exp->nlines; i++ )
		{
			LWLINE *iline = lwline_deserialize(exp->lines[i]);
#if VERBOSE
			elog(NOTICE, " line %d deserialized", i);
#endif
			LWLINE *oline = simplify2d_lwline(iline, dist);
#if VERBOSE
			elog(NOTICE, " line %d simplified", i);
#endif
			if ( oline == NULL ) continue;
			newlines[newlinesnum] = lwline_serialize(oline);
			newlinesnum++;
		}
		pfree(exp->lines);
		exp->lines = newlines;
		exp->nlines = newlinesnum;
	}

	if ( exp->npolys )
	{
		newpolys = palloc(sizeof(char *)*exp->npolys);
		for ( i=0; i<exp->npolys; i++ )
		{
			LWPOLY *ipoly = lwpoly_deserialize(exp->polys[i]);
			LWPOLY *opoly = simplify2d_lwpoly(ipoly, dist);
			if ( opoly == NULL ) continue;
			newpolys[newpolysnum] = lwpoly_serialize(opoly);
			newpolysnum++;
		}
		pfree(exp->polys);
		exp->polys = newpolys;
		exp->npolys = newpolysnum;
	}

	// copy 1 (when lwexploded_serialize_buf will be implemented this
	// can be avoided)
	serialized = lwexploded_serialize(exp, lwgeom_hasBBOX(geom->type));
	pfree_exploded(exp);


	// copy 2 (see above)
	result = PG_LWGEOM_construct(serialized,
		pglwgeom_getSRID(geom), lwgeom_hasBBOX(geom->type));

	PG_RETURN_POINTER(result);
}

/***********************************************************************
 * --strk@keybit.net;
 ***********************************************************************/

/***********************************************************************
 * Interpolate a point along a line, useful for Geocoding applications
 * SELECT line_interpolate_point( 'LINESTRING( 0 0, 2 2'::geometry, .5 )
 * returns POINT( 1 1 ).
 * Works in 2d space only.
 *
 * Initially written by: jsunday@rochgrp.com
 * Ported to LWGEOM by: strk@refractions.net
 ***********************************************************************/

Datum LWGEOM_line_interpolate_point(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(LWGEOM_line_interpolate_point);
Datum LWGEOM_line_interpolate_point(PG_FUNCTION_ARGS)
{
	PG_LWGEOM *geom = (PG_LWGEOM *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	double distance = PG_GETARG_FLOAT8(1);
	LWLINE *line;
	LWPOINT *point;
	POINTARRAY *ipa, *opa;
	POINT4D pt;
	char *srl;
	int nsegs, i;
	double length, slength, tlength;

	if( distance < 0 || distance > 1 ) {
		elog(ERROR,"line_interpolate_point: 2nd arg isnt within [0,1]");
		PG_RETURN_NULL();
	}

	if( lwgeom_getType(geom->type) != LINETYPE ) {
		elog(ERROR,"line_interpolate_point: 1st arg isnt a line");
		PG_RETURN_NULL();
	}

	line = lwline_deserialize(SERIALIZED_FORM(geom));
	ipa = line->points;

	/* If distance is one of the two extremes, return the point on that
	 * end rather than doing any expensive computations
	 */
	if ( distance == 0.0 || distance == 1.0 )
	{
		if ( distance == 0.0 )
			getPoint4d_p(ipa, 0, &pt);
		else
			getPoint4d_p(ipa, ipa->npoints-1, &pt);

		opa = pointArray_construct((char *)&pt,
			TYPE_HASZ(line->type),
			TYPE_HASM(line->type),
			1);
		point = lwpoint_construct(line->SRID, 0, opa);
		srl = lwpoint_serialize(point);
		pfree_point(point);
		PG_RETURN_POINTER(PG_LWGEOM_construct(srl, line->SRID, 0));
	}

	/* Interpolate a point on the line */
	nsegs = ipa->npoints - 1;
	length = lwgeom_pointarray_length2d(ipa);
	tlength = 0;
	for( i = 0; i < nsegs; i++ ) {
		POINT2D *p1, *p2;

		p1 = (POINT2D *)getPoint(ipa, i);
		p2 = (POINT2D *)getPoint(ipa, i+1);

		/* Find the relative length of this segment */
		slength = distance2d_pt_pt(p1, p2)/length;

		/* If our target distance is before the total length we've seen
		 * so far. create a new point some distance down the current
		 * segment.
		 */
		if( distance < tlength + slength ) {
			double dseg = (distance - tlength) / slength;
			pt.x = (p1->x) + ((p2->x - p1->x) * dseg);
			pt.y = (p1->y) + ((p2->y - p1->y) * dseg);
			pt.z = 0;
			pt.m = 0;
			opa = pointArray_construct((char *)&pt,
				TYPE_HASZ(line->type),
				TYPE_HASM(line->type),
				1);
			point = lwpoint_construct(line->SRID, 0, opa);
			srl = lwpoint_serialize(point);
			pfree_point(point);
			PG_RETURN_POINTER(PG_LWGEOM_construct(srl, line->SRID, 0));
		}
		tlength += slength;
	}

	/* Return the last point on the line. This shouldn't happen, but
	 * could if there's some floating point rounding errors. */
	getPoint4d_p(ipa, ipa->npoints-1, &pt);
	opa = pointArray_construct((char *)&pt,
		TYPE_HASZ(line->type),
		TYPE_HASM(line->type),
		1);
	point = lwpoint_construct(line->SRID, 0, opa);
	srl = lwpoint_serialize(point);
	pfree_point(point);
	PG_RETURN_POINTER(PG_LWGEOM_construct(srl, line->SRID, 0));
}
/***********************************************************************
 * --jsunday@rochgrp.com;
 ***********************************************************************/

/***********************************************************************
 *
 *  Grid application function for postgis.
 *
 *  WHAT IS
 *  -------
 *
 *  This is a grid application function for postgis.
 *  You use it to stick all of a geometry points to
 *  a custom grid defined by its origin and cell size
 *  in geometry units.
 * 
 *  Points reduction is obtained by collapsing all
 *  consecutive points falling on the same grid node and
 *  removing all consecutive segments S1,S2 having
 *  S2.startpoint = S1.endpoint and S2.endpoint = S1.startpoint.
 * 
 *  ISSUES
 *  ------
 * 
 *  Only 2D is contemplated in grid application.
 * 
 *  Consecutive coincident segments removal does not work
 *  on first/last segments (They are not considered consecutive).
 * 
 *  Grid application occurs on a geometry subobject basis, thus no
 *  points reduction occurs for multipoint geometries.
 *
 *  USAGE TIPS
 *  ----------
 * 
 *  Run like this:
 * 
 *     SELECT apply_grid(<geometry>, <originX>, <originY>, <sizeX>, <sizeY>);
 * 
 *     Grid cells are not visible on a screen as long as the screen
 *     point size is equal or bigger then the grid cell size.
 *     This assumption may be used to reduce the number of points in
 *     a map for a given display scale.
 * 
 *     Keeping multiple resolution versions of the same data may be used
 *     in conjunction with MINSCALE/MAXSCALE keywords of mapserv to speed
 *     up rendering.
 * 
 *     Check also the use of DP simplification to reduce grid visibility.
 *     I'm still researching about the relationship between grid size and
 *     DP epsilon values - please tell me if you know more about this.
 * 
 * 
 * --strk@keybit.net;
 *
 ***********************************************************************/

#define CHECK_RING_IS_CLOSE
#define SAMEPOINT(a,b) ((a)->x==(b)->x&&(a)->y==(b)->y)

typedef struct gridspec_t {
   double ipx;
   double ipy;
   double xsize;
   double ysize;
} gridspec;

/* Forward declarations */
LWGEOM *lwgeom_grid(LWGEOM *lwgeom, gridspec *grid);
LWCOLLECTION *lwcollection_grid(LWCOLLECTION *coll, gridspec *grid);
LWPOINT * lwpoint_grid(LWPOINT *point, gridspec *grid);
LWPOLY * lwpoly_grid(LWPOLY *poly, gridspec *grid);
LWLINE *lwline_grid(LWLINE *line, gridspec *grid);
POINTARRAY *ptarray_grid(POINTARRAY *pa, gridspec *grid);
Datum LWGEOM_apply_grid(PG_FUNCTION_ARGS);

/*
 * Stick an array of points to the given gridspec.
 * Return "gridded" points in *outpts and their number in *outptsn.
 *
 * Two consecutive points falling on the same grid cell are collapsed
 * into one single point.
 *
 */
POINTARRAY *
ptarray_grid(POINTARRAY *pa, gridspec *grid)
{
	POINT2D pbuf; 
	int numoutpoints=0;
	int pn; /* point number */
	char *opts;
	POINTARRAY *opa;
	size_t ptsize;

#if VERBOSE
	elog(NOTICE, "ptarray_grid called on %p", pa);
#endif

	ptsize = sizeof(POINT2D);

#if VERBOSE
	elog(NOTICE, "ptarray_grid: ptsize: %d", ptsize);
#endif

	opts = (char *)palloc(ptsize * pa->npoints);
	if ( opts == NULL )
	{
		elog(ERROR, "Out of virtual memory");
		return NULL;
	}

	for (pn=0; pn<pa->npoints; pn++)
	{
		getPoint2d_p(pa, pn, &pbuf);

		POINT2D *lastpoint = NULL;
		POINT2D *lastpoint2 = NULL;

		if ( grid->xsize )
			pbuf.x = rint((pbuf.x - grid->ipx)/grid->xsize) *
				grid->xsize + grid->ipx;
		if ( grid->ysize )
			pbuf.y = rint((pbuf.y - grid->ipy)/grid->ysize) *
				grid->ysize + grid->ipy;

		/* Points have been already drawn */
		if ( numoutpoints )
		{
			lastpoint = (POINT2D *)(opts+((numoutpoints-1)*ptsize));

			/*
			 * Skip point if falling on the same cell then
			 * the previous one
			 */
			if ( SAMEPOINT(&pbuf, lastpoint) ) continue;

			/*
			 * Skip this and previous point if they make a
			 * back_and_forw line.
			 * WARNING! this migth be a valid reduction only for
			 * polygons, since a linestring might want to
			 * preserve that!
			 */
			if ( numoutpoints )
			{
				lastpoint2 = (POINT2D *)(opts+((numoutpoints-2)*ptsize));
				if (  SAMEPOINT(&pbuf, lastpoint2) )
				{
					numoutpoints--;
					continue;
				}
			}
		}

		memcpy(opts+(numoutpoints*ptsize), &pbuf, ptsize);
		numoutpoints++;

	}

	if ( numoutpoints )
	{
		/* Srhink allocated memory for outpoints if needed */
		if ( numoutpoints < pa->npoints )
		{
			opts = (char *)repalloc(opts, ptsize*numoutpoints);
			if ( opts == NULL )
			{
				elog(ERROR, "Out of virtual memory");
				return NULL;
			}
		}
	}

	/*
	 * This should never happen since no reduction occurs if not
	 * based on other drawn points
	 */
	else
	{
		elog(NOTICE, "No points drawn out of %d input points, error?",
			pa->npoints);
		pfree(opts);
		opts = NULL;
		return NULL;
	}

	opa = pointArray_construct(opts, 0, 0, numoutpoints);

	return opa;
}

LWLINE *
lwline_grid(LWLINE *line, gridspec *grid)
{
	LWLINE *oline;
	POINTARRAY *opa;

	opa = ptarray_grid(line->points, grid);

	/* Skip line3d with less then 2 points */
	if ( opa->npoints < 2 ) return NULL;

	// TODO: grid bounding box...
	oline = lwline_construct(line->SRID, NULL, opa);

	return oline;
}

LWPOLY *
lwpoly_grid(LWPOLY *poly, gridspec *grid)
{
	LWPOLY *opoly;
	int ri;
	POINTARRAY **newrings = NULL;
	int nrings = 0;
	double minvisiblearea;

	/*
	 * TODO: control this assertion
	 * it is assumed that, since the grid size will be a pixel,
	 * a visible ring should show at least a white pixel inside,
	 * thus, for a square, that would be grid_xsize*grid_ysize
	 */
	minvisiblearea = grid->xsize * grid->ysize;

	nrings = 0;

#ifdef REPORT_RINGS_REDUCTION
	elog(NOTICE, "grid_polygon3d: applying grid to polygon with %d rings",
   		poly->nrings);
#endif

	for (ri=0; ri<poly->nrings; ri++)
	{
		POINTARRAY *ring = poly->rings[ri];
		POINTARRAY *newring;

#ifdef CHECK_RING_IS_CLOSE
		POINT2D *p1, *p2;
		p1 = (POINT2D *)getPoint(ring, 0);
		p2 = (POINT2D *)getPoint(ring, ring->npoints-1);
		if ( ! SAMEPOINT(p1, p2) )
			elog(NOTICE, "Before gridding: first point != last point");
#endif

		newring = ptarray_grid(ring, grid);

		/* Skip ring if not composed by at least 4 pts (3 segments) */
		if ( newring->npoints < 4 )
		{
			pfree(newring);
#ifdef REPORT_RINGS_ADJUSTMENTS
			elog(NOTICE, "grid_polygon3d: ring%d skipped ( <4 pts )", ri);
#endif
			if ( ri ) continue;
			else break; /* this is the external ring, no need to work on holes */
		}

#ifdef CHECK_RING_IS_CLOSE
		p1 = (POINT2D *)getPoint(newring, 0);
		p2 = (POINT2D *)getPoint(newring, newring->npoints-1);
		if ( ! SAMEPOINT(p1, p2) )
			elog(NOTICE, "After gridding: first point != last point");
#endif



#ifdef REPORT_POINTS_REDUCTION
elog(NOTICE, "grid_polygon3d: ring%d simplified from %d to %d points", ri,
	ring->npoints, newring->npoints);
#endif


		/*
		 * Add ring to simplified ring array
		 * (TODO: dinamic allocation of pts_per_ring)
		 */
		if ( ! nrings ) {
			newrings = palloc(sizeof(POINTARRAY *));
		} else {
			newrings = repalloc(newrings, sizeof(POINTARRAY *)*(nrings+1));
		}
		if ( ! newrings ) {
			elog(ERROR, "Out of virtual memory");
			return NULL;
		}
		newrings[nrings++] = newring;
	}

#ifdef REPORT_RINGS_REDUCTION
elog(NOTICE, "grid_polygon3d: simplified polygon with %d rings", nrings);
#endif

	if ( ! nrings ) return NULL;

	opoly = lwpoly_construct(poly->SRID, NULL, nrings, newrings);
	return opoly;
}

LWPOINT *
lwpoint_grid(LWPOINT *point, gridspec *grid)
{
	POINT2D *p = (POINT2D *)getPoint(point->point, 0);
	double x, y;
	x = rint((p->x - grid->ipx)/grid->xsize) *
		grid->xsize + grid->ipx;
	y = rint((p->y - grid->ipy)/grid->ysize) *
		grid->ysize + grid->ipy;

#if VERBOSE
	elog(NOTICE, "lwpoint_grid called");
#endif
	return make_lwpoint2d(point->SRID, x, y);
}

LWCOLLECTION *
lwcollection_grid(LWCOLLECTION *coll, gridspec *grid)
{
	unsigned int i;
	LWGEOM **geoms;
	unsigned int ngeoms=0;

	geoms = palloc(coll->ngeoms * sizeof(LWGEOM *));

	for (i=0; i<coll->ngeoms; i++)
	{
		LWGEOM *g = lwgeom_grid(coll->geoms[i], grid);
		if ( g ) geoms[ngeoms++] = g;
	}

	if ( ! ngeoms ) return lwcollection_construct_empty(coll->SRID, 0, 0);

	return lwcollection_construct(TYPE_GETTYPE(coll->type), coll->SRID,
		NULL, ngeoms, geoms);
}

LWGEOM *
lwgeom_grid(LWGEOM *lwgeom, gridspec *grid)
{
	switch(TYPE_GETTYPE(lwgeom->type))
	{
		case POINTTYPE:
			return (LWGEOM *)lwpoint_grid((LWPOINT *)lwgeom, grid);
		case LINETYPE:
			return (LWGEOM *)lwline_grid((LWLINE *)lwgeom, grid);
		case POLYGONTYPE:
			return (LWGEOM *)lwpoly_grid((LWPOLY *)lwgeom, grid);
		case MULTIPOINTTYPE:
		case MULTILINETYPE:
		case MULTIPOLYGONTYPE:
		case COLLECTIONTYPE:
			return (LWGEOM *)lwcollection_grid((LWCOLLECTION *)lwgeom, grid);
		default:
			elog(ERROR, "lwgeom_grid: Unknown geometry type: %d",
				TYPE_GETTYPE(lwgeom->type));
			return NULL;
	}
}

PG_FUNCTION_INFO_V1(LWGEOM_apply_grid);
Datum LWGEOM_apply_grid(PG_FUNCTION_ARGS)
{
	Datum datum;
	PG_LWGEOM *in_geom;
	LWGEOM *in_lwgeom;
	PG_LWGEOM *out_geom = NULL;
	LWGEOM *out_lwgeom;
	size_t size;
	gridspec grid;

	if ( PG_ARGISNULL(0) ) PG_RETURN_NULL();
	datum = PG_GETARG_DATUM(0);
	in_geom = (PG_LWGEOM *)PG_DETOAST_DATUM(datum);

	if ( PG_ARGISNULL(1) ) PG_RETURN_NULL();
	grid.ipx = PG_GETARG_FLOAT8(1);

	if ( PG_ARGISNULL(2) ) PG_RETURN_NULL();
	grid.ipy = PG_GETARG_FLOAT8(2);

	if ( PG_ARGISNULL(3) ) PG_RETURN_NULL();
	grid.xsize = PG_GETARG_FLOAT8(3);

	if ( PG_ARGISNULL(4) ) PG_RETURN_NULL();
	grid.ysize = PG_GETARG_FLOAT8(4);

	/* 0-sided grid == grid */
	if ( grid.xsize == 0 && grid.ysize == 0 ) PG_RETURN_POINTER(in_geom);

	in_lwgeom = lwgeom_deserialize(SERIALIZED_FORM(in_geom));

#if VERBOSE
	elog(NOTICE, "apply_grid got a %s", lwgeom_typename(TYPE_GETTYPE(in_lwgeom->type)));
#endif

   	out_lwgeom = lwgeom_grid(in_lwgeom, &grid);
	if ( out_lwgeom == NULL ) PG_RETURN_NULL();

#if VERBOSE
	elog(NOTICE, "apply_grid made a %s", lwgeom_typename(TYPE_GETTYPE(out_lwgeom->type)));
#endif

	size = lwgeom_serialize_size(out_lwgeom);
	out_geom = palloc(size+4);
	out_geom->size = size+4;
	lwgeom_serialize_buf(out_lwgeom, SERIALIZED_FORM(out_geom), NULL);

	PG_RETURN_POINTER(out_geom);
}
/***********************************************************************
 * --strk@keybit.net
 ***********************************************************************/
