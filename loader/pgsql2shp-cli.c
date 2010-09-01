/**********************************************************************
 * $Id: pgsql2shp-cli.c 5450 2010-03-22 19:38:14Z pramsey $
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2001-2003 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************
 *
 * PostGIS to Shapefile converter
 *
 * Original Author: Jeff Lounsbury <jeffloun@refractions.net>
 * Maintainer: Sandro Santilli <strk@keybit.bet>
 *
 **********************************************************************/

#include "pgsql2shp-core.h"


static void
usage()
{
	printf("RCSID: %s RELEASE: %s\n", RCSID, POSTGIS_VERSION);
	printf("USAGE: pgsql2shp [<options>] <database> [<schema>.]<table>\n");
	printf("       pgsql2shp [<options>] <database> <query>\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("  -f <filename>  Use this option to specify the name of the file\n");
	printf("     to create.\n");
	printf("  -h <host>  Allows you to specify connection to a database on a\n");
	printf("     machine other than the default.\n");
	printf("  -p <port>  Allows you to specify a database port other than the default.\n");
	printf("  -P <password>  Connect to the database with the specified password.\n");
	printf("  -u <user>  Connect to the database as the specified user.\n");
	printf("  -g <geometry_column> Specify the geometry column to be exported.\n");
	printf("  -b Use a binary cursor.\n");
	printf("  -r Raw mode. Do not assume table has been created by \n");
	printf("     the loader. This would not unescape attribute names\n");
	printf("     and will not skip the 'gid' attribute.\n");
	printf("  -k Keep postgresql identifiers case.\n");
	printf("  -? Display this help screen.\n");
	printf("\n");
	exit (0);
}

int
main(int argc, char **argv)
{
	SHPDUMPERCONFIG *config;
	SHPDUMPERSTATE *state;

	int ret, c, i, exit_status = 0;

	/* If no options are specified, display usage */
	if (argc == 1)
	{
		usage();
		exit(0);
	}

	/* Parse command line options and set configuration */
	config = malloc(sizeof(SHPDUMPERCONFIG));
	set_config_defaults(config);

	while ((c = pgis_getopt(argc, argv, "bf:h:du:p:P:g:rk")) != EOF)
	{
		switch (c)
		{
		case 'b':
			config->binary = 1;
			break;
		case 'f':
			config->shp_file = pgis_optarg;
			break;
		case 'h':
			config->conn->host = pgis_optarg;
			break;
		case 'd':
			config->dswitchprovided = 1;
			break;
		case 'r':
			config->includegid = 1;
			config->unescapedattrs = 1;
			break;
		case 'u':
			config->conn->username = pgis_optarg;
			break;
		case 'p':
			config->conn->port = pgis_optarg;
			break;
		case 'P':
			config->conn->password = pgis_optarg;
			break;
		case 'g':
			config->geo_col_name = pgis_optarg;
			break;
		case 'k':
			config->keep_fieldname_case = 1;
			break;
		case '?':
			usage();
			exit(0);
		default:
			usage();
			exit(0);
		}
	}


	/* Determine the database name from the next argument, if no database, exit. */
	if (pgis_optind < argc)
	{
		config->conn->database = argv[pgis_optind];
		pgis_optind++;
	}
	else
	{
		usage();
		exit(0);
	}


	/* Determine the table and schema names from the next argument if supplied, otherwise if
	   it's a user-defined query then set that instead */
	if (pgis_optind < argc)
	{
		char *ptr;

		/* User-defined queries begin with SELECT */
		if (!strncmp(argv[pgis_optind], "SELECT ", 7) ||
			!strncmp(argv[pgis_optind], "select ", 7))
		{
			config->usrquery = argv[pgis_optind];
		}
		else
		{
			/* Schema qualified table name */
			ptr = strchr(argv[pgis_optind], '.');
	
			if (ptr)
			{
				config->schema = malloc(strlen(argv[pgis_optind]) + 1);
				snprintf(config->schema, ptr - argv[pgis_optind] + 1, "%s", argv[pgis_optind]);
	
				config->table = malloc(strlen(argv[pgis_optind]));
				snprintf(config->table, strlen(argv[pgis_optind]) - strlen(config->schema), "%s", ptr + 1);
			}
			else
			{
				config->table = malloc(strlen(argv[pgis_optind]) + 1);
				strcpy(config->table, argv[pgis_optind]);
			}
		}
	}

	state = ShpDumperCreate(config);

	ret = ShpDumperConnectDatabase(state);
	if (ret != SHPDUMPEROK)
	{
		fprintf(stderr, "%s\n", state->message);
		fflush(stderr);
		exit(1);
	}

	/* Display a warning if the -d switch is used with PostGIS >= 1.0 */
	if (state->pgis_major_version > 0 && state->config->dswitchprovided)
	{
		fprintf(stderr, "WARNING: -d switch is useless when dumping from postgis-1.0.0+\n");
		fflush(stderr);
	}

	/* Open the table ready to return rows */
	fprintf(stdout, "Initializing... \n");
	fflush(stdout);

	ret = ShpDumperOpenTable(state);
	if (ret != SHPDUMPEROK)
	{
		fprintf(stderr, "%s\n", state->message);
		fflush(stderr);

		if (ret == SHPDUMPERERR)
			exit(1);
	}

	fprintf(stdout, "Done (postgis major version: %d).\n", state->pgis_major_version);
	fprintf(stdout, "Output shape: %s\n", shapetypename(state->outshptype));
	fprintf(stdout, "Dumping: ");
	fflush(stdout);

	for (i = 0; i < ShpDumperGetRecordCount(state); i++)
	{
		/* Mimic existing behaviour */
		if (!(state->currow % state->config->fetchsize))
		{
			fprintf(stdout, "X");
			fflush(stdout);
		}

		ret = ShpLoaderGenerateShapeRow(state);
		if (ret != SHPDUMPEROK)
		{
			fprintf(stderr, "%s\n", state->message);
			fflush(stderr);
	
			if (ret == SHPDUMPERERR)
				exit(1);
		}
	}

	fprintf(stdout, " [%d rows].\n", ShpDumperGetRecordCount(state));
	fflush(stdout);

	/* Return an error code of 2 to indicate "success, but 0 records were processed"
	   (maybe because of an empty table or a user-defined query not returning any results).
	   This is useful for scripts that need to determine whether or not the output shapefile
	   contains any useful data. */

	if (ShpDumperGetRecordCount(state) == 0)
		exit_status = 2;

	ret = ShpDumperCloseTable(state);
	if (ret != SHPDUMPEROK)
	{
		fprintf(stderr, "%s\n", state->message);
		fflush(stderr);

		if (ret == SHPDUMPERERR)
			exit(1);
	}

	ShpDumperDestroy(state);

	return exit_status;
}
