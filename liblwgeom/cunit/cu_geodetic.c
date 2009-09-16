/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2009 Paul Ramsey <pramsey@cleverelephant.ca>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include "cu_geodetic.h"

/*
** Called from test harness to register the tests in this file.
*/
CU_pSuite register_geodetic_suite(void)
{
	CU_pSuite pSuite;
	pSuite = CU_add_suite("Geodetic Suite", init_geodetic_suite, clean_geodetic_suite);
	if (NULL == pSuite)
	{
		CU_cleanup_registry();
		return NULL;
	}

	if (
	    (NULL == CU_add_test(pSuite, "test_signum()", test_signum))  ||
	    (NULL == CU_add_test(pSuite, "test_gbox_from_spherical_coordinates()", test_gbox_from_spherical_coordinates)) /* ||
	    (NULL == CU_add_test(pSuite, "test_gserialized_get_gbox_geocentric()", test_gserialized_get_gbox_geocentric)) ||
	    (NULL == CU_add_test(pSuite, "test_gbox_calculation()", test_gbox_calculation)) */
	)
	{
		CU_cleanup_registry();
		return NULL;
	}
	return pSuite;
}

/*
** The suite initialization function.
** Create any re-used objects.
*/
int init_geodetic_suite(void)
{
	return 0;
}

/*
** The suite cleanup function.
** Frees any global objects.
*/
int clean_geodetic_suite(void)
{
	return 0;
}


void test_signum(void)
{
    CU_ASSERT_EQUAL(signum(-5.0),-1);
    CU_ASSERT_EQUAL(signum(5.0),1);
}

void test_gbox_from_spherical_coordinates(void)
{
	double ll[64];
	GBOX *box = gbox_new(gflags(0, 0, 1));
    GBOX *good;
	int rv;
	POINTARRAY *pa;
	ll[0] = -3.083333333333333333333333333333333;
	ll[1] = 9.83333333333333333333333333333333;
	ll[2] = 15.5;
	ll[3] = -5.25;

	pa = pointArray_construct((uchar*)ll, 0, 0, 2);
	rv = ptarray_calculate_gbox_geodetic(pa, box);
	good = gbox_from_string("GBOX((0.95958795,-0.05299812,-0.09150161),(0.99495869,0.26611729,0.17078275))");
//	printf("\n%s\n", gbox_to_string(box));
//  printf("%s\n", "(0.95958795, -0.05299812, -0.09150161)  (0.99495869, 0.26611729, 0.17078275)");
	CU_ASSERT_DOUBLE_EQUAL(box->xmin, good->xmin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->xmax, good->xmax, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->ymin, good->ymin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->ymax, good->ymax, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->zmin, good->zmin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->zmax, good->zmax, 0.0001);
	lwfree(pa);
    lwfree(good);


	ll[0] = -35.0;
	ll[1] = 52.5;
	ll[2] = 50.0;
	ll[3] = 60.0;

	pa = pointArray_construct((uchar*)ll, 0, 0, 2);
	rv = ptarray_calculate_gbox_geodetic(pa, box);
	good = gbox_from_string("GBOX((0.32139380,-0.34917121,0.79335334),(0.49866816,0.38302222,0.90147645))");
//	printf("\n%s\n", gbox_to_string(box));
//  printf("%s\n", "(0.32139380, -0.34917121, 0.79335334)  (0.49866816, 0.38302222, 0.90147645)");
	CU_ASSERT_DOUBLE_EQUAL(box->xmin, good->xmin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->xmax, good->xmax, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->ymin, good->ymin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->ymax, good->ymax, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->zmin, good->zmin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->zmax, good->zmax, 0.0001);
	lwfree(pa);
    lwfree(good);


	ll[0] = -122.5;
	ll[1] = 56.25;
	ll[2] = -123.5;
	ll[3] = 69.166666;

	pa = pointArray_construct((uchar*)ll, 0, 0, 2);
	rv = ptarray_calculate_gbox_geodetic(pa, box);
	good = gbox_from_string("GBOX((-0.29850766,-0.46856318,0.83146961),(-0.19629681,-0.29657213,0.93461892))");
//  printf("\n%s\n", gbox_to_string(box));
//  printf("%s\n", "(-0.29850766, -0.46856318, 0.83146961)  (-0.19629681, -0.29657213, 0.93461892)");
	CU_ASSERT_DOUBLE_EQUAL(box->xmin, good->xmin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->xmax, good->xmax, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->ymin, good->ymin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->ymax, good->ymax, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->zmin, good->zmin, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(box->zmax, good->zmax, 0.0001);
	lwfree(pa);
    lwfree(good);

	lwfree(box);
	
}

void test_gserialized_get_gbox_geocentric(void)
{
	LWGEOM *lwg;
	GSERIALIZED *g;
	GBOX *gbox;
	int i;

	char wkt[][512] = { 
		"POINT(45 45)",
		"MULTILINESTRING((45 45, 45 45),(45 45, 45 45))",
		"MULTILINESTRING((45 45, 45 45),(45 45, 45.5 45.5),(45 45, 46 46))",
		"GEOMETRYCOLLECTION(MULTILINESTRING((45 45, 45 45),(45 45, 45.5 45.5),(45 45, 46 46)))",
		"MULTIPOLYGON(((45 45, 45 45, 45 45, 45 45),(45 45, 45 45, 45 45, 45 45)))",
		"LINESTRING(45 45, 45.1 45.1)",
		"MULTIPOINT(45 45, 45.05 45.05, 45.1 45.1)"
	};
    
    double xmin[] = {
		0.707106781187,
		0.707106781187,
		0.694658370459,
		0.694658370459,
		0.707106781187,
		0.705871570679,
		0.705871570679
	};
	
    for ( i = 0; i < 7; i++ )
    {
		//printf("%s\n", wkt[i]);
		lwg = lwgeom_from_ewkt(wkt[i], PARSER_CHECK_NONE);
		g = gserialized_from_lwgeom(lwg, 1, 0);
		g->flags = FLAGS_SET_GEODETIC(g->flags, 1);
		lwgeom_free(lwg);
		gbox = gserialized_calculate_gbox_geocentric(g);
		//printf("%s\n", gbox_to_string(box, g->flags));
		CU_ASSERT_DOUBLE_EQUAL(gbox->xmin, xmin[i], 0.00000001);
		lwfree(g);
		lwfree(gbox);
	}

}

/* 
* Build LWGEOM on top of *aligned* structure so we can use the read-only 
* point access methods on them. 
*/
static LWGEOM* lwgeom_over_gserialized(char *wkt)
{
	LWGEOM *lwg;
	GSERIALIZED *g;
	
	lwg = lwgeom_from_ewkt(wkt, PARSER_CHECK_NONE);
	g = gserialized_from_lwgeom(lwg, 1, 0);
	lwgeom_free(lwg);
	return lwgeom_from_gserialized(g);
}

void test_gbox_calculation(void)
{

	LWGEOM *geom;
	int i = 0;
	GBOX *gbox = gbox_new(gflags(0,0,0));
	BOX3D *box3d;
	
	char ewkt[][512] = { 
		"POINT(0 0.2)",
		"LINESTRING(-1 -1,-1 2.5,2 2,2 -1)",
		"SRID=1;MULTILINESTRING((-1 -1,-1 2.5,2 2,2 -1),(-1 -1,-1 2.5,2 2,2 -1),(-1 -1,-1 2.5,2 2,2 -1),(-1 -1,-1 2.5,2 2,2 -1))",
		"POLYGON((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0))",
		"SRID=4326;MULTIPOLYGON(((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0),(-0.5 -0.5,-0.5 -0.4,-0.4 -0.4,-0.4 -0.5,-0.5 -0.5)),((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0),(-0.5 -0.5,-0.5 -0.4,-0.4 -0.4,-0.4 -0.5,-0.5 -0.5)))",
		"SRID=4326;GEOMETRYCOLLECTION(POINT(0 1),POLYGON((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0)),MULTIPOLYGON(((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0),(-0.5 -0.5,-0.5 -0.4,-0.4 -0.4,-0.4 -0.5,-0.5 -0.5))))",
		"POINT(0 220.2)",
		"LINESTRING(-1 -1,-1231 2.5,2 2,2 -1)",
		"SRID=1;MULTILINESTRING((-1 -131,-1 2.5,2 2,2 -1),(-1 -1,-1 2.5,2 2,2 -1),(-1 -1,-1 2.5,2 2,2 -1),(-1 -1,-1 2.5,2 2,2 -1))",
		"POLYGON((-1 -1,-1 2.5,2 2,2 -133,-1 -1),(0 0,0 1,1 1,1 0,0 0))",
		"SRID=4326;MULTIPOLYGON(((-1 -1,-1 2.5,211 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0),(-0.5 -0.5,-0.5 -0.4,-0.4 -0.4,-0.4 -0.5,-0.5 -0.5)),((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0),(-0.5 -0.5,-0.5 -0.4,-0.4 -0.4,-0.4 -0.5,-0.5 -0.5)))",
		"SRID=4326;GEOMETRYCOLLECTION(POINT(0 1),POLYGON((-1 -1,-1111 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0)),MULTIPOLYGON(((-1 -1,-1 2.5,2 2,2 -1,-1 -1),(0 0,0 1,1 1,1 0,0 0),(-0.5 -0.5,-0.5 -0.4,-0.4 -0.4,-0.4 -0.5,-0.5 -0.5))))",
	};
		
	for( i = 0; i < 6; i++ )
	{
		geom = lwgeom_over_gserialized(ewkt[i]);
		lwgeom_calculate_gbox(geom, gbox);
		box3d = lwgeom_compute_box3d(geom);
		//printf("%g %g\n", gbox->xmin, box3d->xmin);
		CU_ASSERT_EQUAL(gbox->xmin, box3d->xmin);
		CU_ASSERT_EQUAL(gbox->xmax, box3d->xmax);
		CU_ASSERT_EQUAL(gbox->ymin, box3d->ymin);
		CU_ASSERT_EQUAL(gbox->ymax, box3d->ymax);
		lwfree(box3d);
	}
	
	lwfree(gbox);


}






