/*
 * Written by Ralph Mason ralph.mason<at>telogis.com
 *
 * Copyright Telogis 2004
 * www.telogis.com
 *
 */

%x vals_ok
%{
#include "wktparse.tab.h"

static YY_BUFFER_STATE buf_state;
   void init_parser(const char *src) { BEGIN(0);buf_state = lwg_parse_yy_scan_string(src); }
   void close_parser() { lwg_parse_yy_delete_buffer(buf_state); }
   int lwg_parse_yywrap(void){ return 1; }

%}

%%

<vals_ok>[-|\+]?[0-9]+(\.[0-9]+)?([Ee](\+|-)?[0-9]+)? { lwg_parse_yylval.value=atof(lwg_parse_yytext); return VALUE; }
<vals_ok>[-|\+]?(\.[0-9]+)([Ee](\+|-)?[0-9]+)? { lwg_parse_yylval.value=atof(lwg_parse_yytext); return VALUE; }

<INITIAL>00[0-9A-F]* {  lwg_parse_yylval.wkb=lwg_parse_yytext; return WKB;}
<INITIAL>01[0-9A-F]* {  lwg_parse_yylval.wkb=lwg_parse_yytext; return WKB;}

<*>POINT 	{ return POINT; }
<*>LINESTRING { return LINESTRING; }
<*>POLYGON { return POLYGON; }
<*>MULTIPOINT { return MULTIPOINT; }
<*>MULTILINESTRING { return MULTILINESTRING; }
<*>MULTIPOLYGON { return MULTIPOLYGON; }
<*>GEOMETRYCOLLECTION { return GEOMETRYCOLLECTION; }
<*>SRID { BEGIN(vals_ok); return SRID; }
<*>EMPTY { return EMPTY; }

<*>\(	{ BEGIN(vals_ok); return LPAREN; }
<*>\)	{ return RPAREN; }
<*>,	{ return COMMA ; }
<*>=	{ return EQUALS ; }
<*>;	{ BEGIN(0); return SEMICOLON; }
<*>[ \t\n\r]+ /*eat whitespace*/
<*>.	{ return lwg_parse_yytext[0]; }

%%

