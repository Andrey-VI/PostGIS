#ifndef _LWGEOM_PG_H
#define _LWGEOM_PG_H 1

#include "postgres.h"
#include "utils/geo_decls.h"
#include "fmgr.h"
#include "liblwgeom.h"
#include "pgsql_compat.h"

#ifndef PG_NARGS
#define PG_NARGS() (fcinfo->nargs)
#endif

void *pg_alloc(size_t size);
void *pg_realloc(void *ptr, size_t size);
void pg_free(void *ptr);
void pg_error(const char *msg, ...);
void pg_notice(const char *msg, ...);

// Serialize an LWGEOM into a PG_LWGEOM (postgis datatype)
PG_LWGEOM *pglwgeom_serialize(LWGEOM *lwgeom);
extern Oid getGeometryOID(void);

// call this as first thing of any PG function
void init_pg_func(void);

// PG-dependant
extern BOX2DFLOAT4 *box_to_box2df(BOX *box);  // postgresql standard type
extern BOX box2df_to_box(BOX2DFLOAT4 *box);
extern void box2df_to_box_p(BOX2DFLOAT4 *box, BOX *out); // postgresql standard type

// PG-exposed
Datum BOX2D_same(PG_FUNCTION_ARGS);
Datum BOX2D_overlap(PG_FUNCTION_ARGS);
Datum BOX2D_overleft(PG_FUNCTION_ARGS);
Datum BOX2D_left(PG_FUNCTION_ARGS);
Datum BOX2D_right(PG_FUNCTION_ARGS);
Datum BOX2D_overright(PG_FUNCTION_ARGS);
Datum BOX2D_overbelow(PG_FUNCTION_ARGS);
Datum BOX2D_below(PG_FUNCTION_ARGS);
Datum BOX2D_above(PG_FUNCTION_ARGS);
Datum BOX2D_overabove(PG_FUNCTION_ARGS);
Datum BOX2D_contained(PG_FUNCTION_ARGS);
Datum BOX2D_contain(PG_FUNCTION_ARGS);
Datum BOX2D_intersects(PG_FUNCTION_ARGS);
Datum BOX2D_union(PG_FUNCTION_ARGS);

Datum LWGEOM_same(PG_FUNCTION_ARGS);
Datum BOX3D_construct(PG_FUNCTION_ARGS);
Datum BOX2DFLOAT4_ymin(PG_FUNCTION_ARGS);

Datum LWGEOM_force_2d(PG_FUNCTION_ARGS);
Datum LWGEOM_force_3dm(PG_FUNCTION_ARGS);
Datum LWGEOM_force_3dz(PG_FUNCTION_ARGS);
Datum LWGEOM_force_4d(PG_FUNCTION_ARGS);
Datum LWGEOM_force_collection(PG_FUNCTION_ARGS);
Datum LWGEOM_force_multi(PG_FUNCTION_ARGS);

Datum LWGEOMFromWKB(PG_FUNCTION_ARGS);
Datum WKBFromLWGEOM(PG_FUNCTION_ARGS);

Datum LWGEOM_getBBOX(PG_FUNCTION_ARGS);
Datum LWGEOM_addBBOX(PG_FUNCTION_ARGS);
Datum LWGEOM_dropBBOX(PG_FUNCTION_ARGS);

#endif // !defined _LWGEOM_PG_H 1
