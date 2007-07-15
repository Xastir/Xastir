/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2007  The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#include "xastir.h"

extern char *xastirCoordToLatLongWKT(float x, float y, char *wkt);

#ifdef HAVE_DB

// constants to identify database types

#ifdef HAVE_MYSQL
// MySQL version 3.x and higher
#include <mysql.h>
// mysql error message codes
#include <errmsg.h>
#define DB_MYSQL 1
#endif /* HAVE_MYSQ */

#ifdef HAVE_SPATIAL_DB

#ifdef HAVE_POSTGIS
// Postgresql with postgis
#include <libpq-fe.h>
#define DB_POSTGIS 2
#endif /* HAVE_POSTGIS */

#ifdef HAVE_MYSQL_SPATIAL
// MySQL version 4.1 and higher
#define DB_MYSQL_SPATIAL 3
#endif /* HAVE_MYSQL_SPATIAL */

// constants to control database schema versioning

// Version of the mysql/postgresql table structures this version of xastir expects to find.
// Any change or addition of database schema elements should trigger a version change.
// Newer versions of xastir should require an older database to be upgraded to the 
// current version before allowing queries to run against that database.
#define XASTIR_SPATIAL_DB_VERSION 1  
// Allow grouping of forward compatible table structures allowing an older version of xastir to
// interact with a database created by a newer version of xastir of the same compatble series
// addition of new tables and fields shouldn't change comapatable series, but renamed, deleted, 
// or shortened schema elements should change compatible series (changes where a select or 
// or insert query run by an older version of xastir will fail against a newer database).
#define XASTIR_SPATIAL_DB_COMPATABLE_SERIES 1  

// constants to indicate schema to use in a database
#define XASTIR_SCHEMA_SIMPLE 1     // simple station table only
#define XASTIR_SCHEMA_CAD 2        // simple station table and cad objects
#define XASTIR_SCHEMA_COMPLEX 3    // full aprs concept support
#define XASTIR_SCHEMA_APRSWORLD 4  // aprs world implementaiton

// description of a database
typedef struct {
   char name[100];    // name of connection to display to user
   int type;          // type of dbms (posgresql, mysql, etc)
   char host[255];    // hostname for database server
   int port;          // port on which to connect to database server
   char username[20]; // username to use to connect to database
   char password[20]; // password to use to connect to database 
   char schema[20];   // name of database or schema to use
   char makeerrormessage[255]; // most recent error message from attempting to make a 
                      // connection with using this descriptor.
   int schema_type;    // table structures to use in the database
                      // A database schema could contain both APRSWorld 
                      // and XASTIR table structures, but a separate database
                      // descriptor should be defined for each.  
   char unix_socket[255];   // MySQL - unix socket parameter
   //connection_list open_connections // list of open connections to this database
} DbDescriptor;

// a database connection 
typedef struct {
   int type;         // type of dbms (postgresql, mysql, etc, redundant from descriptor->type)
   DbDescriptor descriptor;  // connection parameters used to establish this connnection
#ifdef HAVE_MYSQL
   MYSQL  *mhandle;   // mysql connection
#endif /* HAVE_MYSQL */
#ifdef HAVE_POSTGIS
   PGconn  *phandle;  // postgres connection
#endif /* HAVE_POSTGIS */
   char errormessage[255]; // most recent error message on this connection.
} Connection; 

// linked list of database connections
typedef struct{
   Connection *conn;  // a database connection
   Connection *conn_next; // pointer to next element in list
} ConnectionList;


// connection management
extern Connection openConnection (DbDescriptor *aDbDescriptor);
extern void closeConnection (Connection *aDbConnection);
extern int testConnection(Connection *aDbConnection);


// storing and retrieving data from a database
extern int storeStationToGisDb(Connection *aDbConnection, DataRow *aStation);
extern int storeCadToGisDb(Connection *aDbConnection, CADRow *aCadObject);
extern int storeStationSimpleToGisDb(Connection *aDbConnection, DataRow *aStation);
extern int getAllSimplePositions(Connection *aDbConnection);
extern int getAllSimplePositionsInBoundingBox(Connection *aDbConnection, int east, int west, int north, int south);

#endif /* HAVE_SPATIAL_DB */

#endif /* HAVE_DB */

// structure to hold a latutude and longitude in decimal degrees
typedef struct {
  float latitude;
  float longitude;
} Point;


