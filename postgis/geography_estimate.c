/**********************************************************************
 * $Id: geography_estimate.c 4211 2009-06-25 03:32:43Z robe $
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2001-2006 Refractions Research Inc.
 * Copyright 2009 Mark Cave-Ayland
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include "postgres.h"
#include "commands/vacuum.h"

#include "libgeom.h"
#include "lwgeom_pg.h"

/* Prototypes */
Datum geography_gist_selectivity(PG_FUNCTION_ARGS);
Datum geography_gist_join_selectivity(PG_FUNCTION_ARGS);
Datum geography_analyze(PG_FUNCTION_ARGS);


/**
* Place holder selectivity calculations to make the index work in
* the absence of real selectivity calculations.
*/

#define DEFAULT_GEOGRAPHY_SEL 0.000005

PG_FUNCTION_INFO_V1(geography_gist_selectivity);
Datum geography_gist_selectivity(PG_FUNCTION_ARGS)
{
	PG_RETURN_FLOAT8(DEFAULT_GEOGRAPHY_SEL);
}

PG_FUNCTION_INFO_V1(geography_gist_join_selectivity);
Datum geography_gist_join_selectivity(PG_FUNCTION_ARGS)
{
	PG_RETURN_FLOAT8(DEFAULT_GEOGRAPHY_SEL);
}


/**
 * 	Assign a number to the postgis statistics kind
 *
 * 	tgl suggested:
 *
 * 	1-100:	reserved for assignment by the core Postgres project
 * 	100-199: reserved for assignment by PostGIS
 * 	200-9999: reserved for other globally-known stats kinds
 * 	10000-32767: reserved for private site-local use
 *
 */
#define STATISTIC_KIND_GEOGRAPHY 101

/*
 * Define this if you want to use standard deviation based
 * histogram extent computation. If you do, you can also
 * tweak the deviation factor used in computation with
 * SDFACTOR.
 */
#define USE_STANDARD_DEVIATION 1
#define SDFACTOR 3.25


/* Information about the dimensions stored in the sample */
struct dimensions
{
	char axis;
	double min;
	double max;
};

/* Main geography statistics structure, stored in pg_statistics */
typedef struct GEOG_STATS_T
{
	/* Dimensionality of this column */
	float4 dims;

	/* x . y . z = total boxes in grid */
	float4 unitsx;
	float4 unitsy;
	float4 unitsz;

	/* average feature coverage of not-null features:
	   this is either an area for the case of a 2D column
	   or a volume in the case of a 3D column */
	float4 avgFeatureCoverage;

	/*
	 * average number of histogram cells
	 * covered by the sample not-null features
	 */
	float4 avgFeatureCells;

	/* histogram extent */
	float4 xmin, ymin, zmin, xmax, ymax, zmax;

	/*
	 * variable length # of floats for histogram
	 */
	float4 value[1];
}
GEOG_STATS;



/**
 * This function is called by the analyze function iff
 * the geography_analyze() function give it its pointer
 * (this is always the case so far).
 * The geography_analyze() function is also responsible
 * of deciding the number of "sample" rows we will receive
 * here. It is able to give use other 'custom' data, but we
 * won't use them so far.
 *
 * Our job is to build some statistics on the sample data
 * for use by operator estimators.
 *
 * Currently we only need statistics to estimate the number of rows
 * overlapping a given extent (estimation function bound
 * to the && operator).
 *
 */

static void
compute_geography_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc,
					   int samplerows, double totalrows)
{
	MemoryContext old_context;
	GEOG_STATS *geogstats;

	GBOX *sample_extent = NULL;
	GBOX **sampleboxes;
	GBOX histobox;
	int histocells;
	double sizex, sizey, sizez, edgelength;
	int unitsx = 0, unitsy = 0, unitsz = 0;
	int geog_stats_size;
	struct dimensions histodims[3];
	int ndims;
	
	double total_width = 0;
	int notnull_cnt = 0, examinedsamples = 0, total_count_cells=0, total_cells_coverage = 0;

#if USE_STANDARD_DEVIATION
	/* for standard deviation */
	double avgLOWx, avgLOWy, avgLOWz, avgHIGx, avgHIGy, avgHIGz;
	double sumLOWx = 0, sumLOWy = 0, sumLOWz = 0, sumHIGx = 0, sumHIGy = 0, sumHIGz = 0;
	double sdLOWx = 0, sdLOWy = 0, sdLOWz = 0, sdHIGx = 0, sdHIGy = 0, sdHIGz = 0;
	GBOX *newhistobox = NULL;
#endif

	bool isnull;
	int i;

	POSTGIS_DEBUG(2, "compute_geography_stats called");

	/*
	 * We'll build an histogram having from 40 to 400 boxesPerSide
	 * Total number of cells is determined by attribute stat
	 * target. It can go from  1600 to 160000 (stat target: 10,1000)
	 */
	histocells = 160 * stats->attr->attstattarget;

	/*
	 * Memory to store the bounding boxes from all of the sampled rows
	 */
	sampleboxes = palloc(sizeof(GBOX *) * samplerows);

	/*
	 * First scan:
	 *  o find extent of the sample rows
	 *  o count null-infinite/not-null values
	 *  o compute total_width
	 *  o compute total features's box area (for avgFeatureArea)
	 *  o sum features box coordinates (for standard deviation)
	 */
	for (i = 0; i < samplerows; i++)
	{
		GBOX gbox;
		Datum datum;
		GSERIALIZED *serialized;
		LWGEOM *geometry;

		/* Fetch the datum and cast it into a geography */
		datum = fetchfunc(stats, i, &isnull);

		/* Skip nulls */
		if (isnull)
			continue;

		serialized = (GSERIALIZED *)PG_DETOAST_DATUM(datum);
		geometry = lwgeom_from_gserialized(serialized);

		/* Convert coordinates to 3D geodesic */
		if (!lwgeom_calculate_gbox_geodetic(geometry, &gbox))
		{
			/* Unable to obtain or calculate a bounding box */
			POSTGIS_DEBUGF(3, "skipping geometry at position %d", i);

			continue;
                }

		/*
		 * Skip infinite geoms
		 */
		if ( ! finite(gbox.xmin) || ! finite(gbox.xmax) ||
			! finite(gbox.ymin) || ! finite(gbox.ymax) ||
			! finite(gbox.zmin) || ! finite(gbox.zmax) )
		{
			POSTGIS_DEBUGF(3, " skipped infinite geometry at position %d", i);

			continue;
		}

		/*
		 * Store bounding box in array
		 */
		sampleboxes[notnull_cnt] = palloc(sizeof(GBOX));
		memcpy(sampleboxes[notnull_cnt], &gbox, sizeof(GBOX));

		/*
		 * Add to sample extent union
		 */
		if ( ! sample_extent )
		{
			sample_extent = palloc(sizeof(GBOX));
			memcpy(sample_extent, &gbox, sizeof(GBOX));
		}
		else
		{
			sample_extent->xmax = LWGEOM_Maxf(sample_extent->xmax, gbox.xmax);
			sample_extent->ymax = LWGEOM_Maxf(sample_extent->ymax, gbox.ymax);
			sample_extent->zmax = LWGEOM_Maxf(sample_extent->zmax, gbox.zmax);
			sample_extent->xmin = LWGEOM_Minf(sample_extent->xmin, gbox.xmin);
			sample_extent->ymin = LWGEOM_Minf(sample_extent->ymin, gbox.ymin);
			sample_extent->zmin = LWGEOM_Minf(sample_extent->zmin, gbox.zmin);
		}

		/** TODO: ask if we need geom or bvol size for stawidth */
		total_width += serialized->size;

#if USE_STANDARD_DEVIATION
		/*
		 * Add bvol coordinates to sum for standard deviation
		 * computation.
		 */
		sumLOWx += gbox.xmin;
		sumLOWy += gbox.ymin;
		sumLOWz += gbox.zmin;
		sumHIGx += gbox.xmax;
		sumHIGy += gbox.ymax;
		sumHIGz += gbox.zmax;
#endif

		notnull_cnt++;

		/* give backend a chance of interrupting us */
		vacuum_delay_point();
	}

	POSTGIS_DEBUG(3, "End of 1st scan:");
	POSTGIS_DEBUGF(3, " Sample extent (min, max): (%g %g %g), (%g %g %g)", sample_extent->xmin, sample_extent->ymin,
		sample_extent->zmin, sample_extent->xmax, sample_extent->ymax, sample_extent->zmax);
	POSTGIS_DEBUGF(3, " No. of geometries sampled: %d", samplerows);
	POSTGIS_DEBUGF(3, " No. of non-null geometries sampled: %d", notnull_cnt);

	if ( ! notnull_cnt )
	{
		elog(NOTICE, " no notnull values, invalid stats");
		stats->stats_valid = false;
		return;
	}

#if USE_STANDARD_DEVIATION

	POSTGIS_DEBUG(3, "Standard deviation filter enabled");

	/*
	 * Second scan:
	 *  o compute standard deviation
	 */
	avgLOWx = sumLOWx / notnull_cnt;
	avgLOWy = sumLOWy / notnull_cnt;
	avgLOWz = sumLOWz / notnull_cnt;
	avgHIGx = sumHIGx / notnull_cnt;
	avgHIGy = sumHIGy / notnull_cnt;
	avgHIGz = sumHIGz / notnull_cnt;

	for (i = 0; i < notnull_cnt; i++)
	{
		GBOX *box;
		box = (GBOX *)sampleboxes[i];

		sdLOWx += (box->xmin - avgLOWx) * (box->xmin - avgLOWx);
		sdLOWy += (box->ymin - avgLOWy) * (box->ymin - avgLOWy);
		sdLOWz += (box->zmin - avgLOWz) * (box->zmin - avgLOWz);
		sdHIGx += (box->xmax - avgHIGx) * (box->xmax - avgHIGx);
		sdHIGy += (box->ymax - avgHIGy) * (box->ymax - avgHIGy);
		sdHIGz += (box->zmax - avgHIGz) * (box->zmax - avgHIGz);
	}
	sdLOWx = sqrt(sdLOWx / notnull_cnt);
	sdLOWy = sqrt(sdLOWy / notnull_cnt);
	sdLOWz = sqrt(sdLOWz / notnull_cnt);
	sdHIGx = sqrt(sdHIGx / notnull_cnt);
	sdHIGy = sqrt(sdHIGy / notnull_cnt);
	sdHIGz = sqrt(sdHIGz / notnull_cnt);

	POSTGIS_DEBUG(3, " standard deviations:");
	POSTGIS_DEBUGF(3, "  LOWx - avg:%f sd:%f", avgLOWx, sdLOWx);
	POSTGIS_DEBUGF(3, "  LOWy - avg:%f sd:%f", avgLOWy, sdLOWy);
	POSTGIS_DEBUGF(3, "  LOWz - avg:%f sd:%f", avgLOWz, sdLOWz);
	POSTGIS_DEBUGF(3, "  HIGx - avg:%f sd:%f", avgHIGx, sdHIGx);
	POSTGIS_DEBUGF(3, "  HIGy - avg:%f sd:%f", avgHIGy, sdHIGy);
	POSTGIS_DEBUGF(3, "  HIGz - avg:%f sd:%f", avgHIGz, sdHIGz);

	histobox.xmin = LW_MAX((avgLOWx - SDFACTOR * sdLOWx), sample_extent->xmin);
	histobox.ymin = LW_MAX((avgLOWy - SDFACTOR * sdLOWy), sample_extent->ymin);
	histobox.zmin = LW_MAX((avgLOWz - SDFACTOR * sdLOWz), sample_extent->zmin);
	histobox.xmax = LW_MIN((avgHIGx + SDFACTOR * sdHIGx), sample_extent->xmax);
	histobox.ymax = LW_MIN((avgHIGy + SDFACTOR * sdHIGy), sample_extent->ymax);
	histobox.zmax = LW_MIN((avgHIGz + SDFACTOR * sdHIGz), sample_extent->zmax);

	POSTGIS_DEBUGF(3, " sd_extent: xmin, ymin, zmin: %f, %f, %f",
				   histobox.xmin, histobox.ymin, histobox.zmin);
	POSTGIS_DEBUGF(3, " sd_extent: xmax, ymax, zmax: %f, %f, %f",
				   histobox.xmax, histobox.ymax, histobox.zmax);

	/*
	 * Third scan:
	 *   o skip hard deviants
	 *   o compute new histogram box
	 */
	for (i = 0; i < notnull_cnt; i++)
	{
		GBOX *box;
		box = (GBOX *)sampleboxes[i];

		if ( box->xmin > histobox.xmax || box->xmax < histobox.xmin ||
			box->ymin > histobox.ymax || box->ymax < histobox.ymin ||
			box->zmin > histobox.zmax || box->zmax < histobox.zmin)
		{
			POSTGIS_DEBUGF(4, " feat %d is an hard deviant, skipped", i);

			sampleboxes[i] = NULL;
			continue;
		}

		if ( ! newhistobox )
		{
			newhistobox = palloc(sizeof(GBOX));
			memcpy(newhistobox, box, sizeof(GBOX));
		}
		else
		{
			if ( box->xmin < newhistobox->xmin )
				newhistobox->xmin = box->xmin;
			if ( box->ymin < newhistobox->ymin )
				newhistobox->ymin = box->ymin;
			if ( box->zmin < newhistobox->zmin )
				newhistobox->zmin = box->zmin;
			if ( box->xmax > newhistobox->xmax )
				newhistobox->xmax = box->xmax;
			if ( box->ymax > newhistobox->ymax )
				newhistobox->ymax = box->ymax;
			if ( box->zmax > newhistobox->zmax )
				newhistobox->zmax = box->zmax;
		}
	}

	/*
	 * Set histogram extent as the intersection between
	 * standard deviation based histogram extent
	 * and computed sample extent after removal of
	 * hard deviants (there might be no hard deviants).
	 */
	if ( histobox.xmin < newhistobox->xmin )
		histobox.xmin = newhistobox->xmin;
	if ( histobox.ymin < newhistobox->ymin )
		histobox.ymin = newhistobox->ymin;
	if ( histobox.zmin < newhistobox->zmin )
		histobox.zmin = newhistobox->zmin;
	if ( histobox.xmax > newhistobox->xmax )
		histobox.xmax = newhistobox->xmax;
	if ( histobox.ymax > newhistobox->ymax )
		histobox.ymax = newhistobox->ymax;
	if ( histobox.zmax > newhistobox->zmax )
		histobox.zmax = newhistobox->zmax;

#else /* ! USE_STANDARD_DEVIATION */

	/*
	* Set histogram extent box
	*/
	histobox.xmin = sample_extent->xmin;
	histobox.ymin = sample_extent->ymin;
	histobox.zmin = sample_extent->zmin;
	histobox.xmax = sample_extent->xmax;
	histobox.ymax = sample_extent->ymax;
	histobox.zmax = sample_extent->zmax;

#endif /* USE_STANDARD_DEVIATION */


	POSTGIS_DEBUGF(3, " histogram_extent: xmin, ymin, zmin: %f, %f, %f",
				   histobox.xmin, histobox.ymin, histobox.zmin);
	POSTGIS_DEBUGF(3, " histogram_extent: xmax, ymax, zmax: %f, %f, %f",
				   histobox.xmax, histobox.ymax, histobox.zmax);

	/* Calculate the size of each dimension */
	sizex = histobox.xmax - histobox.xmin;
	sizey = histobox.ymax - histobox.ymin;
	sizez = histobox.zmax - histobox.zmin;

	/* In order to calculate a suitable aspect ratio for the histogram, we need
	   to work out how many dimensions exist within our sample data (which we 
	   assume is representative of the whole data) */
	ndims = 0;
	if (sizex != 0)
	{
		histodims[ndims].axis = 'X';
		histodims[ndims].min = histobox.xmin;
		histodims[ndims].max = histobox.xmax;
		ndims++;
	}
		
	if (sizey != 0)
	{
		histodims[ndims].axis = 'Y';
		histodims[ndims].min = histobox.ymin;
		histodims[ndims].max = histobox.ymax;

		ndims++;
	}

	if (sizez != 0)
	{
		histodims[ndims].axis = 'Z';
		histodims[ndims].min = histobox.zmin;
		histodims[ndims].max = histobox.zmax;

		ndims++;
	}

	/* Based upon the number of dimensions, we now work out the number of units in each dimension.
	   The number of units is defined as the number of cell blocks in each dimension which make
	   up the total number of histocells; i.e. unitsx * unitsy * unitsz = histocells */

	/* Note: geodetic data is currently indexed in 3 dimensions; however code for remaining dimensions
	   is also included to allow for indexing 3D cartesian data at a later date */

	POSTGIS_DEBUGF(3, "Number of dimensions in sample set: %d", ndims);

	switch (ndims)
	{
		case 0:
			/* An empty column, or multiple points in exactly the same
			   position in space */
			unitsx = 1;
			unitsy = 1;
			unitsz = 1;
			histocells = 1;
			break;

		case 1: 
			/* Sample data all lies on a single line, so set the correct
			   units variables depending upon which axis is in use */
			for (i = 0; i < ndims; i++)
			{
				if ( (histodims[i].max - histodims[i].min) != 0)
				{
					/* We've found the non-zero dimension, so set the
					   units variables accordingly */
					switch(histodims[i].axis)
					{
						case 'X':
							unitsx = histocells;
							unitsy = 1;
							unitsz = 1;
							break;

						case 'Y':
							unitsx = 1;
							unitsy = histocells;
							unitsz = 1;
							break;

						case 'Z':
							unitsx = 1;
							unitsy = 1;
							unitsz = histocells;
							break;
					}
				}
			}
			break;

		case 2:
			/* Sample data lies within 2D space: divide the total area by the total
			   number of cells, and thus work out the edge size of the unit block */
			edgelength = sqrt(
					abs(histodims[0].max - histodims[0].min) *
					abs(histodims[1].max - histodims[1].min) / (double)histocells
				);

			/* The calculation is easy; the harder part is to work out which dimensions
			   we actually have to set the units variables appropriately */
			if (histodims[0].axis == 'X' && histodims[1].axis == 'Y')
			{
				/* X and Y */
				unitsx = abs(histodims[0].max - histodims[0].min) / edgelength;
				unitsy = abs(histodims[1].max - histodims[1].min) / edgelength;
				unitsz = 1;
			}
			else if (histodims[0].axis == 'Y' && histodims[1].axis == 'X')
			{
				/* Y and X */
				unitsx = abs(histodims[1].max - histodims[1].min) / edgelength;
				unitsy = abs(histodims[0].max - histodims[0].min) / edgelength;
				unitsz = 1;
			}
			else if (histodims[0].axis == 'X' && histodims[1].axis == 'Z')
			{
				/* X and Z */
				unitsx = abs(histodims[0].max - histodims[0].min) / edgelength;
				unitsy = 1;
				unitsz = abs(histodims[1].max - histodims[1].min) / edgelength;
			}
			else if (histodims[0].axis == 'Z' && histodims[1].axis == 'X')
			{
				/* Z and X */
				unitsx = abs(histodims[0].max - histodims[0].min) / edgelength;
				unitsy = 1;
				unitsz = abs(histodims[1].max - histodims[1].min) / edgelength;
			}
			else if (histodims[0].axis == 'Y' && histodims[1].axis == 'Z')
			{
				/* Y and Z */
				unitsx = 1;
				unitsy = abs(histodims[0].max - histodims[0].min) / edgelength;
				unitsz = (histodims[1].max - histodims[1].min) / edgelength;
			}
			else if (histodims[0].axis == 'Z' && histodims[1].axis == 'Y')
			{
				/* Z and X */
				unitsx = 1;
				unitsy = abs(histodims[1].max - histodims[1].min) / edgelength;
				unitsz = abs(histodims[0].max - histodims[0].min) / edgelength;
			}

			break;

		case 3:
			/* Sample data lies within 3D space: divide the total volume by the total
			   number of cells, and thus work out the edge size of the unit block */
			edgelength = pow(
				abs(histodims[0].max - histodims[0].min) *
				abs(histodims[1].max - histodims[1].min) *
				abs(histodims[2].max - histodims[2].min) / (double)histocells,
				(double)1/3);

			/* Units are simple in 3 dimensions */
			unitsx = abs(histodims[0].max - histodims[0].min) / edgelength;
			unitsy = abs(histodims[1].max - histodims[1].min) / edgelength;
			unitsz = abs(histodims[2].max - histodims[2].min) / edgelength;

			break;
	}

	POSTGIS_DEBUGF(3, " computed histogram grid size (X,Y,Z): %d x %d x %d (%d out of %d cells)", unitsx, unitsy, unitsz, unitsx * unitsy * unitsz, histocells);

	/*
	 * Create the histogram (GEOG_STATS)
	 */
	old_context = MemoryContextSwitchTo(stats->anl_context);
	geog_stats_size = sizeof(GEOG_STATS) + (histocells - 1) * sizeof(float4);
	geogstats = palloc(geog_stats_size);
	MemoryContextSwitchTo(old_context);

	geogstats->dims = ndims;
	geogstats->xmin = histobox.xmin;
	geogstats->ymin = histobox.ymin;
	geogstats->zmin = histobox.zmin;
	geogstats->xmax = histobox.xmax;
	geogstats->ymax = histobox.ymax;
	geogstats->zmax = histobox.zmax;
	geogstats->unitsx = unitsx;
	geogstats->unitsy = unitsy;
	geogstats->unitsz = unitsz;

	/* Initialize all values to 0 */
	for (i = 0; i < histocells; i++)
		geogstats->value[i] = 0;


	/*
	 * Fourth scan:
	 *  o fill histogram values with the number of
	 *    features' bbox overlaps: a feature's bvol
	 *    can fully overlap (1) or partially overlap
	 *    (fraction of 1) an histogram cell.
	 *
	 *  o compute total cells occupation
	 *
	 */

	POSTGIS_DEBUG(3, "Beginning histogram intersection calculations");

	for (i = 0; i < notnull_cnt; i++)
	{
		GBOX *box;

		/* Note these array index variables are zero-based */
		int x_idx_min, x_idx_max, x;
		int y_idx_min, y_idx_max, y;
		int z_idx_min, z_idx_max, z;
		int numcells = 0;

		box = (GBOX *)sampleboxes[i];
		if ( ! box ) continue; /* hard deviant.. */

		/* give backend a chance of interrupting us */
		vacuum_delay_point();

		POSTGIS_DEBUGF(4, " feat %d box is %f %f %f, %f %f %f",
			i, box->xmax, box->ymax, box->zmax, box->xmin, box->ymin, box->zmin);

		/* Find first overlapping unitsx cell */
		x_idx_min = (box->xmin - geogstats->xmin) / sizex * unitsx;
		if (x_idx_min <0) x_idx_min = 0;
		if (x_idx_min >= unitsx) x_idx_min = unitsx - 1;

		/* Find first overlapping unitsy cell */
		y_idx_min = (box->ymin - geogstats->ymin) / sizey * unitsy;
		if (y_idx_min <0) y_idx_min = 0;
		if (y_idx_min >= unitsy) y_idx_min = unitsy - 1;

		/* Find first overlapping unitsz cell */
		z_idx_min = (box->zmin - geogstats->zmin) / sizez * unitsz;
		if (z_idx_min <0) z_idx_min = 0;
		if (z_idx_min >= unitsz) z_idx_min = unitsz - 1;

		/* Find last overlapping unitsx cell */
		x_idx_max = (box->xmax - geogstats->xmin) / sizex * unitsx;
		if (x_idx_max <0) x_idx_max = 0;
		if (x_idx_max >= unitsx ) x_idx_max = unitsx - 1;

		/* Find last overlapping unitsy cell */
		y_idx_max = (box->ymax - geogstats->ymin) / sizey * unitsy;
		if (y_idx_max <0) y_idx_max = 0;
		if (y_idx_max >= unitsy) y_idx_max = unitsy - 1;

		/* Find last overlapping unitsz cell */
		z_idx_max = (box->zmax - geogstats->zmin) / sizez * unitsz;
		if (z_idx_max <0) z_idx_max = 0;
		if (z_idx_max >= unitsz) z_idx_max = unitsz - 1;

		POSTGIS_DEBUGF(4, " feat %d overlaps unitsx %d-%d, unitsy %d-%d, unitsz %d-%d",
			i, x_idx_min, x_idx_max, y_idx_min, y_idx_max, z_idx_min, z_idx_max);

		/* Calculate the feature coverage - this of course depends upon the number of dims */
		switch (ndims)
		{
			case 1:
				total_cells_coverage++;
				break;

			case 2:
				total_cells_coverage += (box->xmax - box->xmin) * (box->ymax - box->ymin);
				break;
			
			case 3:
				total_cells_coverage += (box->xmax - box->xmin) * (box->ymax - box->ymin) *
					(box->zmax - box->zmin);
				break;
		}

		/*
		 * the {x,y,z}_idx_{min,max}
		 * define the grid squares that the box intersects
		 */

		for (z = z_idx_min; z <= z_idx_max; z++)
		{
			for (y = y_idx_min; y <= y_idx_max; y++)
			{
				for (x = x_idx_min; x <= x_idx_max; x++)
				{
					geogstats->value[x + y * unitsx + z * unitsx * unitsy] += 1;
					numcells++;
				}
			}
		}

		/*
		 * before adding to the total cells
		 * we could decide if we really
		 * want this feature to count
		 */
		total_count_cells += numcells;

		examinedsamples++;
	}

	POSTGIS_DEBUGF(3, " examined_samples: %d/%d", examinedsamples, samplerows);

	if ( ! examinedsamples )
	{
		elog(NOTICE, " no examined values, invalid stats");
		stats->stats_valid = false;

		POSTGIS_DEBUG(3, " no stats have been gathered");

		return;
	}

	/** TODO: what about null features (TODO) */
	geogstats->avgFeatureCells = (float4)total_count_cells / examinedsamples;
	geogstats->avgFeatureCoverage = total_cells_coverage / examinedsamples;

	POSTGIS_DEBUGF(3, " histo: total_boxes_cells: %d", total_count_cells);
	POSTGIS_DEBUGF(3, " histo: avgFeatureCells: %f", geogstats->avgFeatureCells);
	POSTGIS_DEBUGF(3, " histo: avgFeatureCoverage: %f", geogstats->avgFeatureCoverage);	
	
	/*
	 * Normalize histogram
	 *
	 * We divide each histogram cell value
	 * by the number of samples examined.
	 *
	 */
	for (i = 0; i < histocells; i++)
		geogstats->value[i] /= examinedsamples;

#if POSTGIS_DEBUG_LEVEL >= 4
	/* Dump the resulting histogram for analysis */
	{
		int x, y, z;
		for (x = 0; x < unitsx; x++)
		{
			for (y = 0; y < unitsy; y++)
			{
				for (z = 0; z < unitsz; z++)
				{
					POSTGIS_DEBUGF(4, " histo[%d,%d,%d] = %.15f", x, y, z,
						geogstats->value[x + y * unitsx + z * unitsx * unitsy]);
				}
			}
		}
	}
#endif

	/*
	 * Write the statistics data
	 */
	stats->stakind[0] = STATISTIC_KIND_GEOGRAPHY;
	stats->staop[0] = InvalidOid;
	stats->stanumbers[0] = (float4 *)geogstats;
	stats->numnumbers[0] = geog_stats_size/sizeof(float4);

	stats->stanullfrac = (float4)(samplerows - notnull_cnt)/samplerows;
	stats->stawidth = total_width/notnull_cnt;
	stats->stadistinct = -1.0;

	POSTGIS_DEBUGF(3, " out: slot 0: kind %d (STATISTIC_KIND_GEOGRAPHY)",
				   stats->stakind[0]);
	POSTGIS_DEBUGF(3, " out: slot 0: op %d (InvalidOid)", stats->staop[0]);
	POSTGIS_DEBUGF(3, " out: slot 0: numnumbers %d", stats->numnumbers[0]);
	POSTGIS_DEBUGF(3, " out: null fraction: %d/%d=%g", (samplerows - notnull_cnt), samplerows, stats->stanullfrac);
	POSTGIS_DEBUGF(3, " out: average width: %d bytes", stats->stawidth);
	POSTGIS_DEBUG(3, " out: distinct values: all (no check done)");

	stats->stats_valid = true;

}


/**
 * This function will be called when the ANALYZE command is run
 * on a column of the "geography" type.
 *
 * It will need to return a stats builder function reference
 * and a "minimum" sample rows to feed it.
 * If we want analysis to be completely skipped we can return
 * FALSE and leave output vals untouched.
 *
 * What we know from this call is:
 *
 * 	o The pg_attribute row referring to the specific column.
 * 	  Could be used to get reltuples from pg_class (which
 * 	  might quite inexact though...) and use them to set an
 * 	  appropriate minimum number of sample rows to feed to
 * 	  the stats builder. The stats builder will also receive
 * 	  a more accurate "estimation" of the number or rows.
 *
 * 	o The pg_type row for the specific column.
 * 	  Could be used to set stat builder / sample rows
 * 	  based on domain type (when postgis will be implemented
 * 	  that way).
 *
 * Being this experimental we'll stick to a static stat_builder/sample_rows
 * value for now.
 *
 */

PG_FUNCTION_INFO_V1(geography_analyze);
Datum geography_analyze(PG_FUNCTION_ARGS)
{
	VacAttrStats *stats = (VacAttrStats *)PG_GETARG_POINTER(0);
	Form_pg_attribute attr = stats->attr;

	POSTGIS_DEBUG(2, "geography_analyze called");

	/* If the attstattarget column is negative, use the default value */
	/* NB: it is okay to scribble on stats->attr since it's a copy */
	if (attr->attstattarget < 0)
		attr->attstattarget = default_statistics_target;

	POSTGIS_DEBUGF(3, " attribute stat target: %d", attr->attstattarget);

	/* Setup the minimum rows and the algorithm function */
	stats->minrows = 300 * stats->attr->attstattarget;
	stats->compute_stats = compute_geography_stats;

	POSTGIS_DEBUGF(3, " minrows: %d", stats->minrows);

	/* Indicate we are done successfully */
	PG_RETURN_BOOL(true);
}
