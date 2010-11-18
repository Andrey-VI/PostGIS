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
#include <stdlib.h>
#include <stdarg.h>

#include "liblwgeom_internal.h"
#include "wktparse.h"
#include "libtgeom.h"


LWGEOM *
lwgeom_deserialize(uchar *srl)
{
	int type = lwgeom_getType(srl[0]);

	LWDEBUGF(2, "lwgeom_deserialize got %d - %s", type, lwtype_name(type));

	switch (type)
	{
	case POINTTYPE:
		return (LWGEOM *)lwpoint_deserialize(srl);
	case LINETYPE:
		return (LWGEOM *)lwline_deserialize(srl);
	case CIRCSTRINGTYPE:
		return (LWGEOM *)lwcircstring_deserialize(srl);
	case POLYGONTYPE:
		return (LWGEOM *)lwpoly_deserialize(srl);
	case TRIANGLETYPE:
		return (LWGEOM *)lwtriangle_deserialize(srl);
	case MULTIPOINTTYPE:
		return (LWGEOM *)lwmpoint_deserialize(srl);
	case MULTILINETYPE:
		return (LWGEOM *)lwmline_deserialize(srl);
	case MULTIPOLYGONTYPE:
		return (LWGEOM *)lwmpoly_deserialize(srl);
	case COLLECTIONTYPE:
		return (LWGEOM *)lwcollection_deserialize(srl);
	case COMPOUNDTYPE:
		return (LWGEOM *)lwcompound_deserialize(srl);
	case CURVEPOLYTYPE:
		return (LWGEOM *)lwcurvepoly_deserialize(srl);
	case MULTICURVETYPE:
		return (LWGEOM *)lwmcurve_deserialize(srl);
	case MULTISURFACETYPE:
		return (LWGEOM *)lwmsurface_deserialize(srl);
	case POLYHEDRALSURFACETYPE:
		return (LWGEOM *)lwpsurface_deserialize(srl);
	case TINTYPE:
		return (LWGEOM *)lwtin_deserialize(srl);
	default:
		lwerror("lwgeom_deserialize: Unknown geometry type: %s",
		        lwtype_name(type));

		return NULL;
	}

}

size_t
lwgeom_serialize_size(LWGEOM *lwgeom)
{
	int type = TYPE_GETTYPE(lwgeom->type);

	LWDEBUGF(2, "lwgeom_serialize_size(%s) called", lwtype_name(type));

	switch (type)
	{
	case POINTTYPE:
		return lwpoint_serialize_size((LWPOINT *)lwgeom);
	case LINETYPE:
		return lwline_serialize_size((LWLINE *)lwgeom);
	case POLYGONTYPE:
		return lwpoly_serialize_size((LWPOLY *)lwgeom);
	case TRIANGLETYPE:
		return lwtriangle_serialize_size((LWTRIANGLE *)lwgeom);
	case CIRCSTRINGTYPE:
		return lwcircstring_serialize_size((LWCIRCSTRING *)lwgeom);
	case CURVEPOLYTYPE:
	case COMPOUNDTYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTICURVETYPE:
	case MULTIPOLYGONTYPE:
	case MULTISURFACETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		return lwcollection_serialize_size((LWCOLLECTION *)lwgeom);
	default:
		lwerror("lwgeom_serialize_size: Unknown geometry type: %s",
		        lwtype_name(type));

		return 0;
	}
}

void
lwgeom_serialize_buf(LWGEOM *lwgeom, uchar *buf, size_t *retsize)
{
	int type = TYPE_GETTYPE(lwgeom->type);

	LWDEBUGF(2, "lwgeom_serialize_buf called with a %s",
	         lwtype_name(type));

	switch (type)
	{
	case POINTTYPE:
		lwpoint_serialize_buf((LWPOINT *)lwgeom, buf, retsize);
		break;
	case LINETYPE:
		lwline_serialize_buf((LWLINE *)lwgeom, buf, retsize);
		break;
	case POLYGONTYPE:
		lwpoly_serialize_buf((LWPOLY *)lwgeom, buf, retsize);
		break;
	case TRIANGLETYPE:
		lwtriangle_serialize_buf((LWTRIANGLE *)lwgeom, buf, retsize);
		break;
	case CIRCSTRINGTYPE:
		lwcircstring_serialize_buf((LWCIRCSTRING *)lwgeom, buf, retsize);
		break;
	case CURVEPOLYTYPE:
	case COMPOUNDTYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTICURVETYPE:
	case MULTIPOLYGONTYPE:
	case MULTISURFACETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		lwcollection_serialize_buf((LWCOLLECTION *)lwgeom, buf,
		                           retsize);
		break;
	default:
		lwerror("lwgeom_serialize_buf: Unknown geometry type: %s",
		        lwtype_name(type));
		return;
	}
	return;
}

uchar *
lwgeom_serialize(LWGEOM *lwgeom)
{
	size_t size = lwgeom_serialize_size(lwgeom);
	size_t retsize;
	uchar *serialized = lwalloc(size);

	lwgeom_serialize_buf(lwgeom, serialized, &retsize);

#if POSTGIS_DEBUG_LEVEL > 0
	if ( retsize != size )
	{
		lwerror("lwgeom_serialize: computed size %d, returned size %d",
		        size, retsize);
	}
#endif

	return serialized;
}

/** Force Right-hand-rule on LWGEOM polygons **/
void
lwgeom_force_clockwise(LWGEOM *lwgeom)
{
	LWCOLLECTION *coll;
	int i;

	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case POLYGONTYPE:
		lwpoly_force_clockwise((LWPOLY *)lwgeom);
		return;

	case TRIANGLETYPE:
		lwtriangle_force_clockwise((LWTRIANGLE *)lwgeom);
		return;

		/* Not handle POLYHEDRALSURFACE and TIN
		   as they are supposed to be well oriented */
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
		coll = (LWCOLLECTION *)lwgeom;
		for (i=0; i<coll->ngeoms; i++)
			lwgeom_force_clockwise(coll->geoms[i]);
		return;
	}
}

/** Reverse vertex order of LWGEOM **/
void
lwgeom_reverse(LWGEOM *lwgeom)
{
	int i;
	LWCOLLECTION *col;

	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case LINETYPE:
		lwline_reverse((LWLINE *)lwgeom);
		return;
	case POLYGONTYPE:
		lwpoly_reverse((LWPOLY *)lwgeom);
		return;
	case TRIANGLETYPE:
		lwtriangle_reverse((LWTRIANGLE *)lwgeom);
		return;
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		col = (LWCOLLECTION *)lwgeom;
		for (i=0; i<col->ngeoms; i++)
			lwgeom_reverse(col->geoms[i]);
		return;
	}
}

BOX3D *lwgeom_compute_box3d(const LWGEOM *lwgeom)
{
	if ( ! lwgeom ) return NULL;

	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case POINTTYPE:
		return lwpoint_compute_box3d((LWPOINT *)lwgeom);
	case LINETYPE:
		return lwline_compute_box3d((LWLINE *)lwgeom);
	case CIRCSTRINGTYPE:
		return lwcircstring_compute_box3d((LWCIRCSTRING *)lwgeom);
	case POLYGONTYPE:
		return lwpoly_compute_box3d((LWPOLY *)lwgeom);
	case TRIANGLETYPE:
		return lwtriangle_compute_box3d((LWTRIANGLE *)lwgeom);
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTICURVETYPE:
	case MULTIPOLYGONTYPE:
	case MULTISURFACETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		return lwcollection_compute_box3d((LWCOLLECTION *)lwgeom);
	}
	/* Never get here, please. */
	return NULL;
}


int
lwgeom_compute_box2d_p(const LWGEOM *lwgeom, BOX2DFLOAT4 *buf)
{
	LWDEBUGF(2, "lwgeom_compute_box2d_p called of %p of type %s.",
	         lwgeom, lwtype_name(TYPE_GETTYPE(lwgeom->type)));

	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case POINTTYPE:
		return lwpoint_compute_box2d_p((LWPOINT *)lwgeom, buf);
	case LINETYPE:
		return lwline_compute_box2d_p((LWLINE *)lwgeom, buf);
	case CIRCSTRINGTYPE:
		return lwcircstring_compute_box2d_p((LWCIRCSTRING *)lwgeom, buf);
	case POLYGONTYPE:
		return lwpoly_compute_box2d_p((LWPOLY *)lwgeom, buf);
	case TRIANGLETYPE:
		return lwtriangle_compute_box2d_p((LWTRIANGLE *)lwgeom, buf);
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTICURVETYPE:
	case MULTIPOLYGONTYPE:
	case MULTISURFACETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		return lwcollection_compute_box2d_p((LWCOLLECTION *)lwgeom, buf);
	}
	return 0;
}

/**
 * do not forget to lwfree() result
 */
BOX2DFLOAT4*
lwgeom_compute_box2d(const LWGEOM *lwgeom)
{
	BOX2DFLOAT4 *result = lwalloc(sizeof(BOX2DFLOAT4));
	if ( lwgeom_compute_box2d_p(lwgeom, result) ) return result;
	else
	{
		lwfree(result);
		return NULL;
	}
}

LWPOINT *
lwgeom_as_lwpoint(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == POINTTYPE )
		return (LWPOINT *)lwgeom;
	else return NULL;
}

LWLINE *
lwgeom_as_lwline(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == LINETYPE )
		return (LWLINE *)lwgeom;
	else return NULL;
}

LWCIRCSTRING *
lwgeom_as_lwcircstring(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == CIRCSTRINGTYPE )
		return (LWCIRCSTRING *)lwgeom;
	else return NULL;
}

LWCOMPOUND *
lwgeom_as_lwcompound(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == COMPOUNDTYPE )
		return (LWCOMPOUND *)lwgeom;
	else return NULL;
}

LWCURVEPOLY *
lwgeom_as_lwcurvepoly(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == CURVEPOLYTYPE )
		return (LWCURVEPOLY *)lwgeom;
	else return NULL;
}

LWPOLY *
lwgeom_as_lwpoly(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == POLYGONTYPE )
		return (LWPOLY *)lwgeom;
	else return NULL;
}

LWTRIANGLE *
lwgeom_as_lwtriangle(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == TRIANGLETYPE )
		return (LWTRIANGLE *)lwgeom;
	else return NULL;
}

LWCOLLECTION *
lwgeom_as_lwcollection(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( lwgeom_is_collection(lwgeom) )
		return (LWCOLLECTION*)lwgeom;
	else return NULL;
}

LWMPOINT *
lwgeom_as_lwmpoint(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == MULTIPOINTTYPE )
		return (LWMPOINT *)lwgeom;
	else return NULL;
}

LWMLINE *
lwgeom_as_lwmline(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == MULTILINETYPE )
		return (LWMLINE *)lwgeom;
	else return NULL;
}

LWMPOLY *
lwgeom_as_lwmpoly(const LWGEOM *lwgeom)
{
	if ( lwgeom == NULL ) return NULL;
	if ( TYPE_GETTYPE(lwgeom->type) == MULTIPOLYGONTYPE )
		return (LWMPOLY *)lwgeom;
	else return NULL;
}

LWPSURFACE *
lwgeom_as_lwpsurface(const LWGEOM *lwgeom)
{
	if ( TYPE_GETTYPE(lwgeom->type) == POLYHEDRALSURFACETYPE )
		return (LWPSURFACE *)lwgeom;
	else return NULL;
}

LWTIN *
lwgeom_as_lwtin(const LWGEOM *lwgeom)
{
	if ( TYPE_GETTYPE(lwgeom->type) == TINTYPE )
		return (LWTIN *)lwgeom;
	else return NULL;
}

LWGEOM *lwtin_as_lwgeom(const LWTIN *obj)
{
	return (LWGEOM *)obj;
}

LWGEOM *lwpsurface_as_lwgeom(const LWPSURFACE *obj)
{
	return (LWGEOM *)obj;
}

LWGEOM *lwmpoly_as_lwgeom(const LWMPOLY *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwmline_as_lwgeom(const LWMLINE *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwmpoint_as_lwgeom(const LWMPOINT *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwcollection_as_lwgeom(const LWCOLLECTION *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwcircstring_as_lwgeom(const LWCIRCSTRING *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwcurvepoly_as_lwgeom(const LWCURVEPOLY *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwcompound_as_lwgeom(const LWCOMPOUND *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwpoly_as_lwgeom(const LWPOLY *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwtriangle_as_lwgeom(const LWTRIANGLE *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwline_as_lwgeom(const LWLINE *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}
LWGEOM *lwpoint_as_lwgeom(const LWPOINT *obj)
{
	if ( obj == NULL ) return NULL;
	return (LWGEOM *)obj;
}


/**
** Look-up for the correct MULTI* type promotion for singleton types.
*/
static uchar MULTITYPE[16] =
{
	0,
	MULTIPOINTTYPE,
	MULTILINETYPE,
	MULTIPOLYGONTYPE,
	0,0,0,0,
	MULTICURVETYPE,
	MULTICURVETYPE,
	MULTISURFACETYPE,
	POLYHEDRALSURFACETYPE,
	0,
	TINTYPE,
	0,0
};

/**
* Create a new LWGEOM of the appropriate MULTI* type.
*/
LWGEOM *
lwgeom_as_multi(const LWGEOM *lwgeom)
{
	LWGEOM **ogeoms;
	LWGEOM *ogeom = NULL;
	BOX2DFLOAT4 *box = NULL;
	int type;

	ogeoms = lwalloc(sizeof(LWGEOM*));

	/*
	** This funx is a no-op only if a bbox cache is already present
	** in input.
	*/
	if ( lwgeom_is_collection(lwgeom) )
	{
		return lwgeom_clone(lwgeom);
	}

	type = TYPE_GETTYPE(lwgeom->type);

	if ( MULTITYPE[type] )
	{
		ogeoms[0] = lwgeom_clone(lwgeom);

		/* Sub-geometries are not allowed to have bboxes or SRIDs, move the bbox to the collection */
		box = ogeoms[0]->bbox;
		ogeoms[0]->bbox = NULL;
		ogeoms[0]->SRID = -1;

		ogeom = (LWGEOM *)lwcollection_construct(MULTITYPE[type], lwgeom->SRID, box, 1, ogeoms);
	}
	else
	{
		return lwgeom_clone(lwgeom);
	}

	return ogeom;
}

void
lwgeom_release(LWGEOM *lwgeom)
{
	uint32 i;
	LWCOLLECTION *col;

	if ( ! lwgeom )
		lwerror("lwgeom_release: someone called on 0x0");

	LWDEBUGF(3, "releasing type %s", lwtype_name(TYPE_GETTYPE(lwgeom->type)));

	/* Drop bounding box (always a copy) */
	if ( lwgeom->bbox )
	{
		LWDEBUGF(3, "lwgeom_release: releasing bbox. %p", lwgeom->bbox);

		lwfree(lwgeom->bbox);
	}

	/* Collection */
	if ( (col=lwgeom_as_lwcollection(lwgeom)) )
	{
		LWDEBUG(3, "lwgeom_release: Releasing collection.");

		for (i=0; i<col->ngeoms; i++)
		{
			lwgeom_release(col->geoms[i]);
		}
		lwfree(lwgeom);
	}

	/* Single element */
	else lwfree(lwgeom);

}


/** Clone an LWGEOM object. POINTARRAY are not copied. **/
LWGEOM *
lwgeom_clone(const LWGEOM *lwgeom)
{
	LWDEBUGF(2, "lwgeom_clone called with %p, %s",
	         lwgeom, lwtype_name(TYPE_GETTYPE(lwgeom->type)));

	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case POINTTYPE:
		return (LWGEOM *)lwpoint_clone((LWPOINT *)lwgeom);
	case LINETYPE:
		return (LWGEOM *)lwline_clone((LWLINE *)lwgeom);
	case CIRCSTRINGTYPE:
		return (LWGEOM *)lwcircstring_clone((LWCIRCSTRING *)lwgeom);
	case POLYGONTYPE:
		return (LWGEOM *)lwpoly_clone((LWPOLY *)lwgeom);
	case TRIANGLETYPE:
		return (LWGEOM *)lwtriangle_clone((LWTRIANGLE *)lwgeom);
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTICURVETYPE:
	case MULTISURFACETYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		return (LWGEOM *)lwcollection_clone((LWCOLLECTION *)lwgeom);
	default:
		return NULL;
	}
}


/**
 * Return an alloced string
 */
char*
lwgeom_to_ewkt(LWGEOM *lwgeom, int flags)
{
	LWGEOM_UNPARSER_RESULT lwg_unparser_result;
	uchar *serialized = lwgeom_serialize(lwgeom);
	int result;
	if ( ! serialized )
	{
		lwerror("Error serializing geom %p", lwgeom);
	}

	result = unparse_WKT(&lwg_unparser_result, serialized, lwalloc, lwfree, flags);
	lwfree(serialized);

	return lwg_unparser_result.wkoutput;
}

/**
 * Return an alloced string
 */
char*
lwgeom_to_hexwkb(LWGEOM *lwgeom, int flags, uint32 byteorder)
{
	LWGEOM_UNPARSER_RESULT lwg_unparser_result;
	uchar *serialized = lwgeom_serialize(lwgeom);
	int result;

	result = unparse_WKB(&lwg_unparser_result, serialized, lwalloc, lwfree, flags, byteorder,1);

	lwfree(serialized);
	return lwg_unparser_result.wkoutput;
}

/**
 * Return an alloced string
 */
uchar *
lwgeom_to_ewkb(LWGEOM *lwgeom, int flags, char byteorder, size_t *outsize)
{
	LWGEOM_UNPARSER_RESULT lwg_unparser_result;
	uchar *serialized = lwgeom_serialize(lwgeom);
	int result;

	/*
	 * We cast return to "unsigned" char as we are
	 * requesting a "binary" output, not HEX
	 * (last argument set to 0)
	 */
	result = unparse_WKB(&lwg_unparser_result, serialized, lwalloc, lwfree,
	                     flags, byteorder, 0);
	lwfree(serialized);
	return (uchar *)lwg_unparser_result.wkoutput;
}

/**
 * Make an LWGEOM object from a EWKB binary representation.
 * Currently highly unoptimized as it:
 * 	- convert EWKB to HEXEWKB
 *	- construct PG_LWGEOM
 *	- deserialize it
 */
LWGEOM*
lwgeom_from_ewkb(uchar *ewkb, int flags, size_t size)
{
	size_t hexewkblen = size*2;
	char *hexewkb;
	long int i;
	int result;
	LWGEOM *ret;
	LWGEOM_PARSER_RESULT lwg_parser_result;

	/* "HEXify" the EWKB */
	hexewkb = lwalloc(hexewkblen+1);
	for (i=0; i<size; ++i) deparse_hex(ewkb[i], &hexewkb[i*2]);
	hexewkb[hexewkblen] = '\0';

	/* Rely on grammar parser to construct a LWGEOM */
	result = serialized_lwgeom_from_ewkt(&lwg_parser_result, hexewkb, flags);
	if (result)
		lwerror("%s", (char *)lwg_parser_result.message);

	/* Free intermediate HEXified representation */
	lwfree(hexewkb);

	/* Deserialize */
	ret = lwgeom_deserialize(lwg_parser_result.serialized_lwgeom);

	return ret;
}

/**
 * Make an LWGEOM object from a EWKT representation.
 */
LWGEOM*
lwgeom_from_ewkt(char *ewkt, int flags)
{
	int result;
	LWGEOM *ret;
	LWGEOM_PARSER_RESULT lwg_parser_result;

	/* Rely on grammar parser to construct a LWGEOM */
	result = serialized_lwgeom_from_ewkt(&lwg_parser_result, ewkt, flags);
	if (result)
		lwerror("%s", (char *)lwg_parser_result.message);

	/* Deserialize */
	ret = lwgeom_deserialize(lwg_parser_result.serialized_lwgeom);

	return ret;
}

/*
 * Parser functions for working with serialized LWGEOMs. Useful for cases where
 * the function input is already serialized, e.g. some input and output functions
 */

/**
 * Make a serialzed LWGEOM object from a WKT input string
 */
int
serialized_lwgeom_from_ewkt(LWGEOM_PARSER_RESULT *lwg_parser_result, char *wkt_input, int flags)
{

	int result = parse_lwg(lwg_parser_result, wkt_input, flags,
	                       lwalloc, lwerror);

	LWDEBUGF(2, "serialized_lwgeom_from_ewkt with %s",wkt_input);

	return result;
}

/**
 * Return an alloced string
 */
int
serialized_lwgeom_to_ewkt(LWGEOM_UNPARSER_RESULT *lwg_unparser_result, uchar *serialized, int flags)
{
	int result;

	result = unparse_WKT(lwg_unparser_result, serialized, lwalloc, lwfree, flags);

	return result;
}

/**
 * Return an alloced string
 */
int
serialized_lwgeom_from_hexwkb(LWGEOM_PARSER_RESULT *lwg_parser_result, char *hexwkb_input, int flags)
{
	/* NOTE: it is actually the same combined WKT/WKB parser that decodes HEXEWKB into LWGEOMs! */
	int result = parse_lwg(lwg_parser_result, hexwkb_input, flags,
	                       lwalloc, lwerror);

	LWDEBUGF(2, "serialized_lwgeom_from_hexwkb with %s", hexwkb_input);

	return result;
}

/**
 * Return an alloced string
 */
int
serialized_lwgeom_to_hexwkb(LWGEOM_UNPARSER_RESULT *lwg_unparser_result, uchar *serialized, int flags, uint32 byteorder)
{
	int result;

	result = unparse_WKB(lwg_unparser_result, serialized, lwalloc, lwfree, flags, byteorder, 1);

	return result;
}

/**
 * Return an alloced string
 */
int
serialized_lwgeom_to_ewkb(LWGEOM_UNPARSER_RESULT *lwg_unparser_result, uchar *serialized, int flags, uint32 byteorder)
{
	int result;

	result = unparse_WKB(lwg_unparser_result, serialized, lwalloc, lwfree, flags, byteorder, 0);

	return result;
}

/**
 * @brief geom1 same as geom2
 *  	iff
 *      	+ have same type
 *			+ have same # objects
 *      	+ have same bvol
 *      	+ each object in geom1 has a corresponding object in geom2 (see above)
 *	@param lwgeom1
 *	@param lwgeom2
 */
char
lwgeom_same(const LWGEOM *lwgeom1, const LWGEOM *lwgeom2)
{
	LWDEBUGF(2, "lwgeom_same(%s, %s) called",
	         lwtype_name(TYPE_GETTYPE(lwgeom1->type)),
	         lwtype_name(TYPE_GETTYPE(lwgeom2->type)));

	if ( TYPE_GETTYPE(lwgeom1->type) != TYPE_GETTYPE(lwgeom2->type) )
	{
		LWDEBUG(3, " type differ");

		return LW_FALSE;
	}

	if ( TYPE_GETZM(lwgeom1->type) != TYPE_GETZM(lwgeom2->type) )
	{
		LWDEBUG(3, " ZM flags differ");

		return LW_FALSE;
	}

	/* Check boxes if both already computed  */
	if ( lwgeom1->bbox && lwgeom2->bbox )
	{
		/*lwnotice("bbox1:%p, bbox2:%p", lwgeom1->bbox, lwgeom2->bbox);*/
		if ( ! box2d_same(lwgeom1->bbox, lwgeom2->bbox) )
		{
			LWDEBUG(3, " bounding boxes differ");

			return LW_FALSE;
		}
	}

	/* geoms have same type, invoke type-specific function */
	switch (TYPE_GETTYPE(lwgeom1->type))
	{
	case POINTTYPE:
		return lwpoint_same((LWPOINT *)lwgeom1,
		                    (LWPOINT *)lwgeom2);
	case LINETYPE:
		return lwline_same((LWLINE *)lwgeom1,
		                   (LWLINE *)lwgeom2);
	case POLYGONTYPE:
		return lwpoly_same((LWPOLY *)lwgeom1,
		                   (LWPOLY *)lwgeom2);
	case TRIANGLETYPE:
		return lwtriangle_same((LWTRIANGLE *)lwgeom1,
		                       (LWTRIANGLE *)lwgeom2);
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		return lwcollection_same((LWCOLLECTION *)lwgeom1,
		                         (LWCOLLECTION *)lwgeom2);
	default:
		lwerror("lwgeom_same: unsupported geometry type: %s",
		        lwtype_name(TYPE_GETTYPE(lwgeom1->type)));
		return LW_FALSE;
	}

}

void
lwgeom_drop_bbox(LWGEOM *lwgeom)
{
	if ( lwgeom->bbox ) lwfree(lwgeom->bbox);
	lwgeom->bbox = NULL;
	TYPE_SETHASBBOX(lwgeom->type, 0);
}

/**
 * Ensure there's a box in the LWGEOM.
 * If the box is already there just return,
 * else compute it.
 */
void
lwgeom_add_bbox(LWGEOM *lwgeom)
{
	if ( lwgeom->bbox ) return;
	lwgeom->bbox = lwgeom_compute_box2d(lwgeom);
	TYPE_SETHASBBOX(lwgeom->type, 1);
}

void
lwgeom_drop_srid(LWGEOM *lwgeom)
{
	TYPE_SETHASSRID(lwgeom->type, 0);
	lwgeom->SRID = -1;
}

LWGEOM *
lwgeom_segmentize2d(LWGEOM *lwgeom, double dist)
{
	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case LINETYPE:
		return (LWGEOM *)lwline_segmentize2d((LWLINE *)lwgeom,
		                                     dist);
	case POLYGONTYPE:
		return (LWGEOM *)lwpoly_segmentize2d((LWPOLY *)lwgeom,
		                                     dist);
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
		return (LWGEOM *)lwcollection_segmentize2d(
		           (LWCOLLECTION *)lwgeom, dist);

	default:
		return lwgeom_clone(lwgeom);
	}
}

LWGEOM*
lwgeom_force_2d(const LWGEOM *geom)
{	
	return lwgeom_force_dims(geom, 0, 0);
}

LWGEOM*
lwgeom_force_3dz(const LWGEOM *geom)
{	
	return lwgeom_force_dims(geom, 1, 0);
}

LWGEOM*
lwgeom_force_3dm(const LWGEOM *geom)
{	
	return lwgeom_force_dims(geom, 0, 1);
}

LWGEOM*
lwgeom_force_4d(const LWGEOM *geom)
{	
	return lwgeom_force_dims(geom, 1, 1);
}

LWGEOM*
lwgeom_force_dims(const LWGEOM *geom, int hasz, int hasm)
{	
	switch(TYPE_GETTYPE(geom->type))
	{
		case POINTTYPE:
			return lwpoint_as_lwgeom(lwpoint_force_dims((LWPOINT*)geom, hasz, hasm));
		case CIRCSTRINGTYPE:
		case LINETYPE:
		case TRIANGLETYPE:
			return lwline_as_lwgeom(lwline_force_dims((LWLINE*)geom, hasz, hasm));
		case POLYGONTYPE:
			return lwpoly_as_lwgeom(lwpoly_force_dims((LWPOLY*)geom, hasz, hasm));
		case COMPOUNDTYPE:
		case CURVEPOLYTYPE:
		case MULTICURVETYPE:
		case MULTISURFACETYPE:
		case MULTIPOINTTYPE:
		case MULTILINETYPE:
		case MULTIPOLYGONTYPE:
		case POLYHEDRALSURFACETYPE:
		case TINTYPE:
		case COLLECTIONTYPE:
			return lwcollection_as_lwgeom(lwcollection_force_dims((LWCOLLECTION*)geom, hasz, hasm));
		default:
			lwerror("lwgeom_force_2d: unsupported geom type: %s", lwtype_name(TYPE_GETTYPE(geom->type)));
			return NULL;
	}
}

void
lwgeom_longitude_shift(LWGEOM *lwgeom)
{
	int i;
	switch (TYPE_GETTYPE(lwgeom->type))
	{
		LWPOINT *point;
		LWLINE *line;
		LWPOLY *poly;
		LWTRIANGLE *triangle;
		LWCOLLECTION *coll;

	case POINTTYPE:
		point = (LWPOINT *)lwgeom;
		ptarray_longitude_shift(point->point);
		return;
	case LINETYPE:
		line = (LWLINE *)lwgeom;
		ptarray_longitude_shift(line->points);
		return;
	case POLYGONTYPE:
		poly = (LWPOLY *)lwgeom;
		for (i=0; i<poly->nrings; i++)
			ptarray_longitude_shift(poly->rings[i]);
		return;
	case TRIANGLETYPE:
		triangle = (LWTRIANGLE *)lwgeom;
		ptarray_longitude_shift(triangle->points);
		return;
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		coll = (LWCOLLECTION *)lwgeom;
		for (i=0; i<coll->ngeoms; i++)
			lwgeom_longitude_shift(coll->geoms[i]);
		return;
	default:
		lwerror("lwgeom_longitude_shift: unsupported geom type: %s",
		        lwtype_name(TYPE_GETTYPE(lwgeom->type)));
	}
}

int 
lwgeom_is_collection(const LWGEOM *geom)
{
	if( ! geom ) return LW_FALSE;
	return lwtype_is_collection(TYPE_GETTYPE(geom->type));
}

/** Return TRUE if the geometry may contain sub-geometries, i.e. it is a MULTI* or COMPOUNDCURVE */
int
lwtype_is_collection(int type)
{

	switch (type)
	{
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
	case COMPOUNDTYPE:
	case MULTICURVETYPE:
	case MULTISURFACETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
		return LW_TRUE;
		break;

	default:
		return LW_FALSE;
	}
}

/**
* Given an lwtype number, what homogeneous collection can hold it?
*/
int 
lwtype_get_collectiontype(int type)
{
	switch (type)
	{
		case POINTTYPE:
			return MULTIPOINTTYPE;
		case LINETYPE:
			return MULTILINETYPE;
		case POLYGONTYPE:
			return MULTIPOLYGONTYPE;
		case CIRCSTRINGTYPE:
			return MULTICURVETYPE;
		case COMPOUNDTYPE:
			return MULTICURVETYPE;
		case CURVEPOLYTYPE:
			return MULTISURFACETYPE;
		case TRIANGLETYPE:
			return TINTYPE;
		default:
			return COLLECTIONTYPE;
	}
}


void lwgeom_free(LWGEOM *lwgeom)
{

	switch (TYPE_GETTYPE(lwgeom->type))
	{
	case POINTTYPE:
		lwpoint_free((LWPOINT *)lwgeom);
		break;
	case LINETYPE:
		lwline_free((LWLINE *)lwgeom);
		break;
	case POLYGONTYPE:
		lwpoly_free((LWPOLY *)lwgeom);
		break;
	case TRIANGLETYPE:
		lwtriangle_free((LWTRIANGLE *)lwgeom);
		break;
	case MULTIPOINTTYPE:
		lwmpoint_free((LWMPOINT *)lwgeom);
		break;
	case MULTILINETYPE:
		lwmline_free((LWMLINE *)lwgeom);
		break;
	case MULTIPOLYGONTYPE:
		lwmpoly_free((LWMPOLY *)lwgeom);
		break;
	case POLYHEDRALSURFACETYPE:
		lwpsurface_free((LWPSURFACE *)lwgeom);
		break;
	case TINTYPE:
		lwtin_free((LWTIN *)lwgeom);
		break;
	case COLLECTIONTYPE:
		lwcollection_free((LWCOLLECTION *)lwgeom);
		break;
	}
	return;

}

int lwgeom_needs_bbox(const LWGEOM *geom)
{
	assert(geom);
	if ( TYPE_GETTYPE(geom->type) == POINTTYPE )
	{
		return LW_FALSE;
	}
	return LW_TRUE;
}

/**
* Count points in an #LWGEOM.
*/
int lwgeom_count_vertices(const LWGEOM *geom)
{
	int result = 0;
	
	/* Null? Zero. */
	if( ! geom ) return 0;
	
	LWDEBUGF(4, "lwgeom_count_vertices got type %s",
	         lwtype_name(TYPE_GETTYPE(geom->type)));

	/* Empty? Zero. */
	if( lwgeom_is_empty(geom) ) return 0;

	switch (TYPE_GETTYPE(geom->type))
	{
	case POINTTYPE:
		result = 1;
		break;
	case TRIANGLETYPE:
	case CIRCSTRINGTYPE: 
	case LINETYPE:
		result = lwline_count_vertices((LWLINE *)geom);
		break;
	case POLYGONTYPE:
		result = lwpoly_count_vertices((LWPOLY *)geom);
		break;
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTICURVETYPE:
	case MULTISURFACETYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		result = lwcollection_count_vertices((LWCOLLECTION *)geom);
		break;
	default:
		lwerror("lwgeom_count_vertices: unsupported input geometry type: %s",
		        lwtype_name(TYPE_GETTYPE(geom->type)));
		break;
	}
	LWDEBUGF(3, "counted %d vertices", result);
	return result;
}

/**
* Count rings in an #LWGEOM.
*/
int lwgeom_count_rings(const LWGEOM *geom)
{
	int result = 0;
	
	/* Null? Empty? Zero. */
	if( ! geom || lwgeom_is_empty(geom) ) 
		return 0;

	switch (TYPE_GETTYPE(geom->type))
	{
	case POINTTYPE:
	case CIRCSTRINGTYPE: 
	case COMPOUNDTYPE:
	case MULTICURVETYPE:
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case LINETYPE:
		result = 0;
		break;
	case TRIANGLETYPE:
		result = 1;
		break;
	case POLYGONTYPE:
		result = ((LWPOLY *)geom)->nrings;
		break;
	case CURVEPOLYTYPE:
		result = ((LWCURVEPOLY *)geom)->nrings;
		break;
	case MULTISURFACETYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
	{
		LWCOLLECTION *col = (LWCOLLECTION*)geom;
		int i = 0;
		for( i = 0; i < col->ngeoms; i++ )
			result += lwgeom_count_rings(col->geoms[i]);
		break;
	}
	default:
		lwerror("lwgeom_count_rings: unsupported input geometry type: %s", lwtype_name(TYPE_GETTYPE(geom->type)));
		break;
	}
	LWDEBUGF(3, "counted %d rings", result);
	return result;
}

int lwgeom_is_empty(const LWGEOM *geom)
{
	int result = LW_FALSE;
	LWDEBUGF(4, "lwgeom_is_empty: got type %s",
	         lwtype_name(TYPE_GETTYPE(geom->type)));

	switch (TYPE_GETTYPE(geom->type))
	{
	case POINTTYPE:
		return lwpoint_is_empty((LWPOINT*)geom);
		break;
	case LINETYPE:
		return lwline_is_empty((LWLINE*)geom);
		break;
	case CIRCSTRINGTYPE:
		return lwcircstring_is_empty((LWCIRCSTRING*)geom);
		break;
	case POLYGONTYPE:
		return lwpoly_is_empty((LWPOLY*)geom);
		break;
	case TRIANGLETYPE:
		return lwtriangle_is_empty((LWTRIANGLE*)geom);
		break;
	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTICURVETYPE:
	case MULTISURFACETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
	case COLLECTIONTYPE:
		return lwcollection_is_empty((LWCOLLECTION *)geom);
		break;
	default:
		lwerror("lwgeom_is_empty: unsupported input geometry type: %s",
		        lwtype_name(TYPE_GETTYPE(geom->type)));
		break;
	}
	return result;
}

int lwgeom_has_srid(const LWGEOM *geom)
{
	if ( (int)(geom->SRID) > 0 )
		return LW_TRUE;

	return LW_FALSE;
}


static int lwcollection_dimensionality(LWCOLLECTION *col)
{
	int i;
	int dimensionality = 0;
	for ( i = 0; i < col->ngeoms; i++ )
	{
		int d = lwgeom_dimensionality(col->geoms[i]);
		if ( d > dimensionality )
			dimensionality = d;
	}
	return dimensionality;
}

extern int lwgeom_dimensionality(LWGEOM *geom)
{
	int dim;

	LWDEBUGF(3, "lwgeom_dimensionality got type %s",
	         lwtype_name(TYPE_GETTYPE(geom->type)));

	switch (TYPE_GETTYPE(geom->type))
	{
	case POINTTYPE:
	case MULTIPOINTTYPE:
		return 0;
		break;
	case LINETYPE:
	case CIRCSTRINGTYPE:
	case MULTILINETYPE:
	case COMPOUNDTYPE:
	case MULTICURVETYPE:
		return 1;
		break;
	case POLYGONTYPE:
	case TRIANGLETYPE:
	case CURVEPOLYTYPE:
	case MULTIPOLYGONTYPE:
	case MULTISURFACETYPE:
		return 2;
		break;

	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
		dim = lwgeom_is_solid(geom)?3:2;
		return dim;
		break;

	case COLLECTIONTYPE:
		return lwcollection_dimensionality((LWCOLLECTION *)geom);
		break;
	default:
		lwerror("lwgeom_dimensionality: unsupported input geometry type: %s",
		        lwtype_name(TYPE_GETTYPE(geom->type)));
		break;
	}
	return 0;
}

extern LWGEOM* lwgeom_remove_repeated_points(LWGEOM *in)
{
	LWDEBUGF(4, "lwgeom_remove_repeated_points got type %s",
	         lwtype_name(TYPE_GETTYPE(in->type)));

	switch (TYPE_GETTYPE(in->type))
	{
	case MULTIPOINTTYPE:
		return lwmpoint_remove_repeated_points((LWMPOINT*)in);
		break;
	case LINETYPE:
		return lwline_remove_repeated_points((LWLINE*)in);

	case MULTILINETYPE:
	case COLLECTIONTYPE:
	case MULTIPOLYGONTYPE:
	case POLYHEDRALSURFACETYPE:
		return lwcollection_remove_repeated_points((LWCOLLECTION *)in);

	case POLYGONTYPE:
		return lwpoly_remove_repeated_points((LWPOLY *)in);
		break;

	case POINTTYPE:
	case TRIANGLETYPE:
	case TINTYPE:
		/* No point is repeated for a single point, or for Triangle or TIN */
		return in;

	case CIRCSTRINGTYPE:
	case COMPOUNDTYPE:
	case MULTICURVETYPE:
	case CURVEPOLYTYPE:
	case MULTISURFACETYPE:
		/* Dunno how to handle these, will return untouched */
		return in;

	default:
		lwnotice("lwgeom_remove_repeated_points: unsupported geometry type: %s",
		         lwtype_name(TYPE_GETTYPE(in->type)));
		return in;
		break;
	}
	return 0;
}

LWGEOM* lwgeom_flip_coordinates(LWGEOM *in)
{
	LWCOLLECTION *col;
	LWPOLY *poly;
	int i;

	LWDEBUGF(4, "lwgeom_flip_coordinates, got type: %s",
	         lwtype_name(TYPE_GETTYPE(in->type)));

	switch (TYPE_GETTYPE(in->type))
	{
	case POINTTYPE:
		ptarray_flip_coordinates(lwgeom_as_lwpoint(in)->point);
		return in;

	case LINETYPE:
		ptarray_flip_coordinates(lwgeom_as_lwline(in)->points);
		return in;

	case CIRCSTRINGTYPE:
		ptarray_flip_coordinates(lwgeom_as_lwcircstring(in)->points);
		return in;

	case POLYGONTYPE:
		poly = (LWPOLY *) in;
		for (i=0; i<poly->nrings; i++)
			ptarray_flip_coordinates(poly->rings[i]);
		return in;

	case TRIANGLETYPE:
		ptarray_flip_coordinates(lwgeom_as_lwtriangle(in)->points);
		return in;

	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTISURFACETYPE:
	case MULTICURVETYPE:
	case POLYHEDRALSURFACETYPE:
	case TINTYPE:
		col = (LWCOLLECTION *) in;
		for (i=0; i<col->ngeoms; i++)
			lwgeom_flip_coordinates(col->geoms[i]);
		return in;

	default:
		lwerror("lwgeom_flip_coordinates: unsupported geometry type: %s",
		        lwtype_name(TYPE_GETTYPE(in->type)));
	}
	return NULL;
}

void lwgeom_set_srid(LWGEOM *geom, int srid)
{
	int i;

	geom->SRID = srid;

	LWDEBUG(4,"entered");
	if ( lwgeom_is_collection(geom) )
	{
		/* All the children are set to the unknown SRID value */
		LWCOLLECTION *col = lwgeom_as_lwcollection(geom);
		for ( i = 0; i < col->ngeoms; i++ )
		{
			lwgeom_set_srid(col->geoms[i], SRID_UNKNOWN);
		}
	}
}

LWGEOM* lwgeom_simplify(const LWGEOM *igeom, double dist)
{
	switch (TYPE_GETTYPE(igeom->type))
	{
	case POINTTYPE:
	case MULTIPOINTTYPE:
		return lwgeom_clone(igeom);
	case LINETYPE:
		return (LWGEOM*)lwline_simplify((LWLINE*)igeom, dist);
	case POLYGONTYPE:
		return (LWGEOM*)lwpoly_simplify((LWPOLY*)igeom, dist);
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
		return (LWGEOM*)lwcollection_simplify((LWCOLLECTION *)igeom, dist);
	default:
		lwerror("lwgeom_simplify: unsupported geometry type: %s",lwtype_name(TYPE_GETTYPE(igeom->type)));
	}
	return NULL;
}
