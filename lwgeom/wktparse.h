/*
 * Written by Ralph Mason ralph.mason<at>telogis.com
 *
 * Copyright Telogis 2004
 * www.telogis.com
 *
 */

#include <stdlib.h>

typedef unsigned char byte;
typedef void* (*allocator)(size_t size);
typedef void  (*freeor)(void* mem);
typedef void  (*report_error)(const char* string);

//typedef unsigned long int4;


/* How much memory is allocated at a time(bytes) for tuples */
#define ALLOC_CHUNKS 8192

/* to shrink ints less than 0x7f to 1 byte */
//#define SHRINK_INTS

#define	POINTTYPE	1
#define	LINETYPE	2
#define	POLYGONTYPE	3
#define	MULTIPOINTTYPE	4
#define	MULTILINETYPE	5
#define	MULTIPOLYGONTYPE	6
#define	COLLECTIONTYPE	7

/* Extended lwgeom integer types */
#define POINTTYPEI    10
#define LINETYPEI     11
#define POLYGONTYPEI  12

extern int srid;

void set_srid(double srid);
void alloc_lwgeom(int srid);

void alloc_point_2d(double x,double y);
void alloc_point_3d(double x,double y,double z);
void alloc_point_4d(double x,double y,double z,double m);

void alloc_point(void);
void alloc_linestring(void);
void alloc_polygon(void);
void alloc_multipoint(void);
void alloc_multilinestring(void);
void alloc_multipolygon(void);
void alloc_geomertycollection(void);
void alloc_empty();

void alloc_counter(void);


void pop(void);
void popc(void);

void alloc_wkb(const char* parser);
/*
	Use these functions to parse and unparse lwgeoms
	You are responsible for freeing the returned memory.
*/

byte* parse_lwg(const char* wkt,allocator allocfunc,report_error errfunc);
byte* parse_lwgi(const char* wkt,allocator allocfunc,report_error errfunc);
char* unparse_WKT(byte* lw_geom,allocator alloc,freeor free);
char* unparse_WKB(byte* lw_geom,allocator alloc,freeor free);


