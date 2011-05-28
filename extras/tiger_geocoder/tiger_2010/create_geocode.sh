#!/bin/bash
# $Id$
PGPORT=5432
PGHOST=localhost
PGUSER=postgres
PGPASSWORD=yourpasswordhere
THEDB=geocoder
PSQL_CMD=/usr/bin/psql
PGCONTRIB=/usr/share/postgresql/contrib
#if you are on 9.1+ use the CREATE EXTENSION syntax instead
${PSQL_CMD} -d "${THEDB}" -f "${PGCONTRIB}/fuzzystrmatch.sql"
#${PSQL_CMD} -d "${THEDB}" -c "CREATE EXTENSION fuzzystrmatch" 
${PSQL_CMD} -d "${THEDB}" -c "CREATE SCHEMA tiger"
${PSQL_CMD} -d "${THEDB}" -f "tables/lookup_tables_2010.sql"
${PSQL_CMD} -d "${THEDB}" -c "CREATE SCHEMA tiger_data"
${PSQL_CMD} -d "${THEDB}" -f "tiger_loader.sql"
${PSQL_CMD} -d "${THEDB}" -f "create_geocode.sql"
${PSQL_CMD} -d "${THEDB}" -c "CREATE INDEX idx_tiger_addr_least_address ON addr USING btree (least_hn(fromhn,tohn));"