/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2007-2008  The Xastir Group
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

// include postgresql library for postgis support
#ifdef HAVE_POSTGIS
#include <libpq-fe.h>
// pg_type.h contains constants for OID values to use in paramTypes arrays
// in prepared queries.
#include <pg_type.h>   
#endif // HAVE_POSTGIS


// mysql error library for mysql error code constants
#ifdef HAVE_MYSQL
#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <errmsg.h>
#include <time.h>
#endif // HAVE_MYSQL


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

// Some systems don't have strtof
#ifndef HAVE_STRTOF
  #define strtof(a,b) atof(a)
#endif

#include "snprintf.h"

#include <stdio.h>
#include <ctype.h>
#include <Xm/XmAll.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "database.h"
#include "main.h"
#include "util.h"
#include "xastir.h"
#include "db_gis.h"

#ifdef HAVE_DB
/* db_gis.c
 *
 * Functions supporting connections to databases, including GIS enabled
 * databases that hold OpenGIS objects and can apply spatial indicies.
 *
 * XASTIR GIS database code is separated into three layers
 *
 *  1) Supporting XASTIR logic (ui elements, cad integration, 
 *     map drawing, etc).
 *  2a) Generic db storage/retrieval code - wrappers for layer 3
 *  2b) Connection management code
 *  3a) DBMS specific db storage/retrieval code for spatial databases
 *  3b) DBMS specific db storage/retrieval code for non-spatial databases
 * 
 * Data structures in an underlying database can be considered as a fourth 
 * level.
 *  
 *  Code for layers 2 and 3 is in this file.
 *
 *  Layer 2 functions should be extern and called from elsewhere to
 *    perform spatial database operations.  Xastir shouldn't need to care
 *    if an underlying database has spatial support or not for simple data.
 *    Some functionality might require spatial object support and might be 
 *    included only if a spatial database is available.  Thus 3b code may only
 *    support a subset of the 2a functions, while 3a code should support all
 *    2a functions.
 *    Layer 2a wrappers should take and return values in xastir coordinates,
 *    and convert them to decimal degrees to pass on to layer 3.  Likewise
 *    return values from layer 3 to layer 2a should be in decimal degrees, 
 *    limiting the number of different places at which the xastir/decimal 
 *    degree conversion code needs to be invoked.  This would not be true if 
 *    data are fed directly from decimal degree feeds into the database, so 
 *    there may also be a need for layer 2 functions that deal only with 
 *    latitude and longitude in decimal degrees.
 *
 *  Layer 3 functions should not be extern and should only be called 
 *    by layer 2 functions from within this file.  
 *    Layer 3 functions should take and return values in decimal degrees.
 *    Xastir objects should be passed down into layer 3, as doing
 *    so should make code easier to maintain (but harder to extend) than using
 *    generic structures for transport of data between layers 2 and 3.  
 *    Passing a station struct from layer 2 to 3 makes layer 2 a very simple
 *    wrapper, but requires new layer 3 code to write station data to a map
 *    layer rather than to a DataRow (to, for example, prepare a layer of 
 *    temperature data at points for analysis and generation of a temperature
 *    grid.)  [Using generic structures for transport would let the layer 3  
 *    code remain unchanged while layer 2 functions are added or extended, but
 *    requires added maintainance to synchronise xastir structs, the generic
 *    structs, and database structures.]
 *
 *  A spatially enabled database is expected to support OpenGIS spatial 
 *  objects and be able to apply spatial indicies to the data.  A 
 *  non-spatially enabled database is expected to hold coordinates using
 *  separate fields for latitude and longitude.  Layer 3 functions that
 *  interact with a spatial database will need to convert decimal degrees
 *  to well known text (WKT, and perhaps also well known binary, WKB) 
 *  representations.  Layer 3 functions that interact with non-spatially 
 *  enabled databases can just pass raw latitudes and longitudes.
 * 
 *  All spatial data are expected to be in WGS84 projection EPSG:4326
 *
 *  Support for five sets of underlying database schema elements is envisioned
 *   - a very simple station at point and time table
 *   - schema elements to support CAD objects with arbitrary associated data
 *     tables.
 *   - a schema capable of holding the full range of aprs data using spatial
 *     elements (Points, Polygons, etc).
 *   - full support for APRSWorld tables (using latitude and longitude fields
 *     rather than spatial elements).  
 *   - arbitrary tables with schema discovery for arbitrary GIS databases 
 *     such as Tiger data.  
 *   The first three of these will require schema version awareness and will
 *   produce compatability/database lifecycle issues.  
 *
 *  Descriptions of how to make connections to databases are stored in 
 *  connection descriptors.  Connection descriptors describe the DBMS, whether
 *  the database has/lacks spatial support, the schema type (simple, 
 *  simple+cad, xastir full, APRSWorld, etc for the database, and connection 
 *  parameters (server, user, database).  The layer 2/3 separation is intended
 *  to allow functions (layer 2) to be called from within xastir (layer 1) 
 *  without the need to test which function to call for which dbms.  Some 
 *  functions may be schema specific, others may be able to use any of 
 *  several different schemas.  Connections can be opened from a database 
 *  descriptor, and more than one descriptor can point to the same database. 
 *  (Thus a single MySQL database may contain simple xastir tables, xastir 
 *  CAD object tables, and APRSWorld tables, but two different descriptors 
 *  would be used to define connections to talk to the APRSWorld tables and 
 *  the simple+cad tables within what MySQL considers one schema.  A given 
 *  version of xastir will expect a particular version or range of versions 
 *  for database schemas - an older version of xastir may expect fields that 
 *  no longer exist in a database created for a newer version of xastir and 
 *  vice versa.  
 *  
 *  Data selected from a spatial database might be brought into xastir as 
 *   stations just like an internet feed or findu fetch trail query, as 
 *   editable CAD objects, or as map layers.  
 */

/******************* DATABASE SUPPORT IS EXPERIMENTAL ***********************/
/**************** CODE IN THIS FILE MAY CHANGE AT ANY TIME ******************/
// Layer 3 declarations

char xastir_dbms_type[4][XASTIR_DB_DESCRIPTOR_MAX_SIZE+1] = {"","MySQL (lat/long)","Postgresql/Postgis","MySQL Spatial"} ;
char xastir_schema_type[5][XASTIR_SCHEMA_DESCRIPTOR_MAX_SIZE+1] = {"","Xastir Simple","Xastir CAD","Xastir Full","APRSWorld"} ;

/*
// store integer values for picklist items, but use localized strings on picklists
char xastir_dbms_type[3][XASTIR_DB_DESCRIPTOR_MAX_SIZE+1];   // array of xastir database type strings
xastir_snprintf(&xastir_dbms_type[DB_MYSQL][0],
     XASTIR_DB_DESCRIPTOR_MAX_SIZE, 
     "%s",langcode("XADBMST001"));
xastir_snprintf(&xastir_dbms_type[DB_POSTGIS][0], 
     sizeof(&xastir_dbms_type[DB_POSTGIS][0]), 
     "%s", langcode("XADBMST002"));
xastir_snprintf(&xastir_dbms_type[DB_MYSQL_SPATIAL][0], 
     sizeof(&xastir_dbms_type[DB_MYSQL_SPATIAL][0]), 
     "%s",langcode("XADBMST003"));

char xastir_schema_type[4][XASTIR_SCHEMA_DESCRIPTOR_MAX_SIZE+1];  // array of xastir schema type strings
xastir_snprintf(xastir_schema_type[XASTIR_SCHEMA_SIMPLE], 
     sizeof(xastir_schema_type[XASTIR_SCHEMA_SIMPLE][0]),
      "%s",langcode ("XASCHEMA01"));
xastir_snprintf(xastir_schema_type[XASTIR_SCHEMA_CAD][0], 
     sizeof(xastir_schema_type[XASTIR_SCHEMA_CAD][0]),
      "%s", langcode("XASCHEMA02"));
xastir_snprintf(xastir_schema_type[XASTIR_SCHEMA_COMPLEX][0], 
     sizeof(xastir_schema_type[XASTIR_SCHEMA_COMPLEX][0]), 
     "%s", langcode("XASCHEMA03"));
xastir_snprintf(xastir_schema_type[XASTIR_SCHEMA_APRSWORLD], 
     sizeof(xastir_schema_type[XASTIR_SCHEMA_APRSWORLD][0]), 
     "%s", langcode("XASCHEMA04"));
*/

#ifdef HAVE_SPATIAL_DB
#ifdef HAVE_POSTGIS
int storeStationToGisDbPostgis(Connection *aDbConnection, DataRow *aStation);
int storeCadToGisDbPostgis(Connection *aDbConnection, CADRow *aCadObject); 
int storeStationSimplePointToGisDbPostgis(Connection *aDbConnection, DataRow *aStation); 
int testXastirVersionPostgis(Connection *aDbConnection);
int getAllSimplePositionsPostgis(Connection *aDbConnection);
int getAllSimplePositionsPostgisInBoundingBox(Connection *aDbConnection, char* str_e_long, char* str_w_long, char* str_n_lat, char* str_s_lat);
//PGconn postgres_conn_struct[MAX_DB_CONNECTIONS];
#endif /* HAVE_POSTGIS*/
#ifdef HAVE_MYSQL_SPATIAL
int storeStationToGisDbMysql(Connection *aDbConnection, DataRow *aStation); 
int storeCadToGisDbMysql(Connection *aDbConnection, CADRow *aCadObject); 
int storeStationSimplePointToGisDbMysql(Connection *aDbConnection, DataRow *aStation); 
int getAllSimplePositionsMysqlSpatial(Connection *aDbConnection);
int getAllCadFromGisDbMysql(Connection *aDbConnection); 
int getAllSimplePositionsMysqlSpatialInBoundingBox(Connection *aDbConnection, char* str_e_long, char* str_w_long, char* str_n_lat, char* str_s_lat);
#endif /* HAVE_MYSQL_SPATIAL */
#endif /* HAVE_SPATIAL_DB */
Connection connection_struc[MAX_DB_CONNECTIONS];
ConnectionList connections [MAX_IFACE_DEVICES];
int connections_initialized = 0;
#ifdef HAVE_MYSQL
MYSQL mysql_conn_struct, *mysql_connection = &mysql_conn_struct;
MYSQL mcs[MAX_DB_CONNECTIONS];
Connection dbc_struct, *dbc = &dbc_struct;
int testXastirVersionMysql(Connection *aDbConnection);
int storeStationSimplePointToDbMysql(Connection *aDbConnection, DataRow *aStation); 
int getAllSimplePositionsMysql(Connection *aDbConnection);
int getAllSimplePositionsMysqlInBoundingBox(Connection *aDbConnection, char *str_e_long, char *str_w_long, char *str_n_lat, char *str_s_lat);
int storeStationToDbMysql(Connection *aDbConnection, DataRow *aStation);
void mysql_interpret_error(int errorcode, Connection *aDbConnection);
#endif /* HAVE_MYSQL*/

// Layer 2a: Generic GIS db storage code. ************************************
// Wrapper functions for actual DBMS specific actions

#ifdef HAVE_SPATIAL_DB

// ******** Functions that require spatialy enabled database support *********





/* function storeStationToGisDb() 
 * Stores the information about a station and its most recent position
 * to a spatial database.
 * @param aDbConnection generic database connection to the database in
 * which the station information is to be stored.
 * @param aStation the station to store.
 * @returns 0 on failure, 1 on success.  On failure, stores error message
 * in connection.
 */
int storeStationToGisDb(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    // This function is dbms agnostic, and hands the call off to a 
    // function for the relevant database type.  That function picks the 
    // relevant schema and either handles the query or passes it on to 
    // a function to handle that schema.  
    switch (aDbConnection->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS :
            returnvalue = storeStationToGisDbPostgis(aDbConnection, aStation);
        break; 
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL :
            returnvalue = storeStationToGisDbMysql(aDbConnection, aStation);
        break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL
        case DB_MYSQL :
            returnvalue = storeStationToDbMysql(aDbConnection, aStation);
        break;
        #endif /* HAVE_MYSQL*/
    }
    return returnvalue;
}






/* function storeCadToGisDb()
 * Stores current data about objects (including CAD objects) and their 
 * most recent positions to a spatial database.  Objects are treated as
 * points
*/
int storeCadToGisDb(Connection *aDbConnection, CADRow *aCadObject) { 
    int returnvalue = 0;
    // check that connection has cad support in schema
    return returnvalue;
}






/* function storeStationTrackToGisDb() 
 * Stores information about a station and track of all recieved positions from 
 * that station (including weather information if present) to a spatial
 * database.
 * @param aDbConnection generic database connection to the database in
 * which the station information is to be stored.
 * @param aStation the station to store.
 * @returns 0 on failure, 1 on success.  On failure, stores error message
 * in connection.
 */
int storeStationTrackToGisDb(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    return returnvalue;
}






#endif /* HAVE_SPATIAL_DB */





// ***** Functions that do not require spatialy enabled database support ******
// Include "Simple" in these function names.  They should only deal with point
// data, not polygons or complex spatial objects.  Station positions and times
// demarking implicit tracks should be ok.  






/* function storeStationSimpleToGisDb() 
 * Stores basic information about a station and its most recent position
 * to a spatial database. Stores only callsign, most recent position, 
 * and time.  Intended for testing and simple logging uses.
 * Underlying table should have structure:
 * create table simpleStation (
 *    simpleStationId int primary key not null auto_increment
 *    station varchar(9) not null,  // max_callsign
 *    time date not null default now(),
 *    position POINT  // or latitude float, longitude float for simple db.
 * ); 
 ****  or perhaps it should be an APRSWorld table??  ****
 ****  or perhaps it should be an APRSWorld table, but with POINT when supported??  ****
 *
 * ********* generalize to lat/lon fields or position POINT. ******
 * @param aDbConnection generic database connection to the database in
 * which the station information is to be stored.
 * @param aStation the station to store.
 * @returns 0 on failure, 1 on success.  On failure, stores error message
 * in connection.
 */
int storeStationSimpleToGisDb(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    int triedDatabase = 0;

    if (debug_level & 1) {
        fprintf(stderr,"in storeStationSimpleToGisDb() "); 
        fprintf(stderr,"with connection->type: %d\n",aDbConnection->type);
    }

    switch (aDbConnection->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS :
            returnvalue = storeStationSimplePointToGisDbPostgis(aDbConnection, aStation);
            triedDatabase++;
        break; 
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL :
            returnvalue = storeStationSimplePointToGisDbMysql(aDbConnection, aStation);
            triedDatabase++;
        break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL
        case DB_MYSQL :
            returnvalue = storeStationSimplePointToDbMysql(aDbConnection, aStation);
            triedDatabase++;
        break;
        #endif /* HAVE_MYSQL*/
    }
    if (triedDatabase==0) { 
    }
    return returnvalue;
}






/* function getAllSimplePositions()
 * Given a database connection, return all simple station positions stored in
 * that database.  
 */
int getAllSimplePositions(Connection *aDbConnection) {
    int returnvalue = 0;
    int triedDatabase = 0;
    if (debug_level & 1) { 
       fprintf(stderr,"in getAllSimplePositions ");    
       fprintf(stderr,"with aDbConnection->type %d\n",aDbConnection->type);    
    }

    switch (aDbConnection->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS :
            returnvalue = getAllSimplePositionsPostgis(aDbConnection);
            triedDatabase++;
        break; 
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL :
            returnvalue = getAllSimplePositionsMysqlSpatial(aDbConnection);
            triedDatabase++;
        break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL
        case DB_MYSQL :
            returnvalue = getAllSimplePositionsMysql(aDbConnection);
            triedDatabase++;
        break;
        #endif /* HAVE_MYSQL*/
    }
    if (triedDatabase==0) { 
    }
    return returnvalue;
}





/* function getAllSimplePositionsInBoundingBox()
 * Given a database connection and a bounding box, return all simple station 
 * positions stored in that database that fall within the bounds of the box.  
 * Takes eastern, western, northern, and southern bounds of box in xastir 
 * coordinates.  
 */
int getAllSimplePositionsInBoundingBox(Connection *aDbConnection, int east, int west, int north, int south) {
    int returnvalue = 0;
    int triedDatabase = 0;
    char str_e_long[11];
    char str_n_lat[10];
    char str_w_long[11];
    char str_s_lat[10];
    // convert from xastir coordinates to decimal degrees
    convert_lon_l2s(east, str_e_long, sizeof(str_e_long), CONVERT_DEC_DEG);
    convert_lat_l2s(north, str_n_lat, sizeof(str_n_lat), CONVERT_DEC_DEG);
    convert_lon_l2s(west, str_w_long, sizeof(str_w_long), CONVERT_DEC_DEG);
    convert_lat_l2s(south, str_s_lat, sizeof(str_s_lat), CONVERT_DEC_DEG);
    switch (aDbConnection->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS :
            returnvalue = getAllSimplePositionsPostgisInBoundingBox(aDbConnection,str_e_long,str_w_long,str_n_lat,str_s_lat);
            triedDatabase++;
        break; 
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL :
            returnvalue = getAllSimplePositionsMysqlSpatialInBoundingBox(aDbConnection,str_e_long,str_w_long,str_n_lat,str_s_lat);
            triedDatabase++;
        break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL
        case DB_MYSQL :
            returnvalue = getAllSimplePositionsMysqlInBoundingBox(aDbConnection,str_e_long,str_w_long,str_n_lat,str_s_lat);
            triedDatabase++;
        break;
        #endif /* HAVE_MYSQL*/
    }
    if (triedDatabase==0) { 
    }
    return returnvalue;
}


// Layer 2b: Connection managment. *******************************************
/* It should be possible to maintain a list of an arbitrary number of defined
 * data sources of different types, and to have an arbitrary number of 
 * connections to these data sources open at the same time.  
 *
 * Some issues: How to handle login credentials for databases?  Request on
 * connection?  How to perform multiple operations with the same datasource 
 * (e.g. logging to the database from feeds while querying CAD objects).
 * Probably want to be able to store password, request password on connect, 
 * or use configuration file (e.g. my.ini) for password) - let user tune
 * choices to environment.
 *
 * The existing interface code seems better suited to having a fixed number 
 * of interfaces with zero or one database connection associated with each
 * interface than handing an arbitrary number of connections per interface.  
 */



// simple testing hardcoded database connection testing function
// remove this function and call in main.c  when integration with 
// interfaces is working.
// fill in password, uncomment, and uncomment code in main.c for
// simple database write test - writes station in n_first to simple mysql db
/*
ioparam simpleDbTest(void) {
   ioparam test;
   Connection conn;
   int ok;
   xastir_snprintf(test.device_name, sizeof(test.device_name), "Test Connection");
   test.database_type = DB_MYSQL;
   xastir_snprintf(test.device_host_name, sizeof(test.device_host_name), "localhost");
   test.sp = 3306;
   xastir_snprintf(test.database_username, sizeof(test.database_username), "xastir_test");
   // hardcode a test password here for simple test
   xastir_snprintf(test.device_host_pswd, sizeof(test.device_host_pswd), "hardcoded test password");
   xastir_snprintf(test.database_schema, sizeof(test.database_schema), "xastir");
   test.database_schema_type = XASTIR_SCHEMA_SIMPLE;
   xastir_snprintf(test.database_unix_socket, sizeof(test.database_unix_socket), "/var/lib/mysql/mysql.sock");
   
   got_conn=openConnection(&test, conn);
   ok = storeStationSimpleToGisDb(&conn, n_first);

   return test;
}
*/

/* Function openConnection()
 * Opens the specified database connection and returns a
 * generic handle to that connection.
 * On connection failure, returns null and sets error message in 
 * the connection descriptor.
 * @param anIface a database connection description (host username etc).
 */
int openConnection(ioparam *anIface, Connection *connection) {
    int returnvalue = 0;
    char connection_string[900];
    int connecting;  // connection polling loop test
    int connected;   // status of connection polling loop
    unsigned long client_flag = 0; // parameter used for mysql connection, is normally 0.
    unsigned int port;  // port to make connection on
    int i;  // loop counter for connection list
    int connection_made = 0;
    #ifdef HAVE_POSTGIS
    PGconn *postgres_connection;
    PostgresPollingStatusType poll;
    #endif /* HAVE_POSTGIS */
    #ifdef HAVE_MYSQL
    if (anIface->database_type!=DB_POSTGIS) { 
        // instantiate the MYSQL structure for the connection
        mysql_init(&connection->mhandle);
    }
    #endif /* HAVE_MYSQL */
    if (debug_level & 1) 
          fprintf(stderr,"Entering openConnection with anIface [%p] and conn [%p]\n",anIface,connection);

    connection->type = anIface->database_type;
    connection->descriptor = *anIface;

    if (connections_initialized == 0) { 
        connections_initialized = 1;
    }
  
    // need some sort of connection listener to handle reconnection attempts when a connection fails...
    
    // try to open connection
    if (!(anIface==NULL)) { 
        switch (anIface->database_type) {
            #ifdef HAVE_POSTGIS
            case DB_POSTGIS : 
                if (debug_level & 1)
                    fprintf(stderr,"Opening Connection to a Postgresql/Postgis database.\n");
                // If type is postgis, connect to postgis database.
                // build connection string from parameters
                xastir_snprintf(connection_string, sizeof(connection_string), \
                   "host=%s user=%s password=%s dbname=%s port=%d", \
                   anIface->device_host_name, anIface->database_username, anIface->device_host_pswd, anIface->database_schema, anIface->sp);
                // Use nonblocking connection
                connection->phandle = PQconnectStart(connection_string);
                if (connection->phandle == NULL) {  
                    xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), "Insufficient memory to open connection.");
                } else {
                    connected = 0;
                    // can connect, run PQ_connect_poll loop
                    // Note: xastir needs to decide when to time out
                    while (connected==0) { 
                       // need to add a timer to polling loop
                       poll = PQconnectPoll(connection->phandle);
                       if (poll == PGRES_POLLING_FAILED || poll == PGRES_POLLING_OK) { 
                          connected = 1;
                       } 
                       // add connection status feedback here if desired
                    }
                    if (PQstatus(connection->phandle)==CONNECTION_OK) {
                        //if (debug_level & 1)
                            fprintf(stderr,"Connected to Postgresql database on %s\n",anIface->device_host_name);
                        // connection successfull
                        connection->type=DB_POSTGIS;
                        connection->descriptor = *anIface;
                        xastir_snprintf(connection->errormessage, sizeof(connection->errormessage), " ");
                        connection_made = 1;
                    } else {
                        // connection attempt failed
                        fprintf(stderr,"Failed to connect to Postgresql database on %s\n",anIface->device_host_name);
                        fprintf(stderr,"Postgres Error: %s\n", PQerrorMessage(connection->phandle));
                        xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), "Unable to make Postgresql connection %s. %s", PQerrorMessage(postgres_connection), connection_string); 
                    }
                }
                break;
            #endif /* HAVE_POSTGIS */
            #ifdef HAVE_MYSQL_SPATIAL
            case DB_MYSQL_SPATIAL : 
                // if type is mysql (=>4.1), connect to mysql database
                if (debug_level & 1) 
                     fprintf(stderr,"Opening connection to a MySQL (spatial) database.\n");
                if (&connection->mhandle == NULL) { 
                    // insufficient memory to initalize a new database handle 
                    xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), "Insufficient memory to open connection.");
                } else { 
                    port = anIface->sp;
                    if (debug_level & 1) 
                        fprintf(stderr,"Opening connection to %s.\n",anIface->device_host_name);
                    mysql_real_connect(&connection->mhandle, anIface->device_host_name, anIface->database_username, anIface->device_host_pswd, anIface->database_schema, port, anIface->database_unix_socket, client_flag); 

//MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long client_flag)

                    if (&connection->mhandle == NULL) { 
                        // unable to establish connection
                        xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), "Unable to establish connection: %s", mysql_error(&connection->mhandle));
                        fprintf(stderr,"Failed to connect to MySQL database on %s\n",anIface->device_host_name);
                        fprintf(stderr, "MySQL Error: %s", mysql_error(&connection->mhandle));
                    } else { 
                        // connected to database
                        fprintf(stderr,"Connected to MySQL database on %s\n",anIface->device_host_name);
                        // make sure error message for making connection is empty.
                        xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), " ");
                        // store connection information
                        connection->type = DB_MYSQL_SPATIAL;
                        connection->descriptor = *anIface;
                        xastir_snprintf(connection->errormessage, sizeof(connection->errormessage), " ");
                        connection_made = 1;

                        // ping the server
                        fprintf(stderr,"mysql ping %d\n",mysql_ping(&connection->mhandle));
                    }
                }
                break;
            #endif /* HAVE_MYSQL_SPATIAL */
            #ifdef HAVE_MYSQL
            case DB_MYSQL : 
                // if type is mysql (<4.1), connect to mysql database
                if (debug_level & 1) 
                    fprintf(stderr,"Opening connection to a MySQL database.\n");
                if (&connection->mhandle == NULL) { 
                    // insufficient memory to initalize a new database handle 
                    xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), "Insufficient memory to open connection.");
                    fprintf(stderr,"Insufficient memory to open mysql connection [mysql_init(*MYSQL) returned null].\n");
                } else { 
                    client_flag = CLIENT_COMPRESS;
                    port = anIface->sp;
// **** fails if database_unix_socket doesn't exist                    
                    mysql_real_connect(&connection->mhandle, anIface->device_host_name, anIface->database_username, anIface->device_host_pswd, anIface->database_schema, port, anIface->database_unix_socket, client_flag); 
                    if (&connection->mhandle == NULL) { 
                        fprintf(stderr,"Unable to establish connection to MySQL database\nHost: %s Schema: %s Username: %s\n",anIface->device_host_name, anIface->database_schema, anIface->database_username);
                        // unable to establish connection
                        xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), "Unable to establish MySQL connection. Host: %s Username: %s Password: %s Schema %s Port: %d", anIface->device_host_name, anIface->database_username, anIface->device_host_pswd, anIface->database_schema, port); 
                        fprintf(stderr,"Failed to connect to MySQL database on %s\n",anIface->device_host_name);
                        fprintf(stderr, "MySQL Error: %s", mysql_error(&connection->mhandle));
                    } else { 
                        fprintf(stderr,"Connected to MySQL database on %s\n",anIface->device_host_name);
                        // connected to database
                        // make sure error message for making connection is empty.
                        xastir_snprintf(anIface->database_errormessage, sizeof(anIface->database_errormessage), " ");
                        // store connection information
                        connection->type = DB_MYSQL;
                        connection->descriptor = *anIface;
                        xastir_snprintf(connection->errormessage, sizeof(connection->errormessage), " ");
                        connection_made = 1;
                        if (debug_level & 1) 
                            fprintf(stderr,"Connected to MySQL database, connection stored\n");

                        // ping the server
                        fprintf(stderr,"mysql ping %d\n",mysql_ping(&connection->mhandle));
                        
                    }
                }
                break;
            #endif /* HAVE_MYSQL*/
        }  /* end switch */
    }  /* end test for null interface */

    if (connection_made==1) { 
        if (debug_level & 1) {
            fprintf(stderr,"Connection made: ");       
            fprintf(stderr,"connection->type [%d]\n",connection->type);
        }
        returnvalue = 1;
    } else { 
        // Detailed error message should have been returned above, but make sure
        // there is at least a minimal failure message regardless of the problem.
        fprintf(stderr,"Failed to make database connection.\n");   
        free(connection);
        //connection = NULL;
    }
    return returnvalue;
}





/* Function closeConnection()
 * Closes the specified database connection.
 * @param aDbConnection a generic database connection handle.
 */
int closeConnection(Connection *aDbConnection, int port_number) {
    //ioparam db =  aDbConnection->descriptor;
    // free up connection resources
   
    switch (aDbConnection->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS : 
            // if type is postgis, close connection to postgis database
            if (aDbConnection->phandle!=NULL) { 
                PQfinish(aDbConnection->phandle);
                //free(aDbConnection->phandle);
            }
            break;
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL : 
             // if type is mysql, close connection to mysql database
            if (aDbConnection->mhandle!=NULL) { 
                mysql_close(&aDbConnection->mhandle);
                //free(aDbConnection->mhandle);
            }
            break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL : 
            // if type is mysql, close connection to mysql database
            if (aDbConnection->mhandle!=NULL) { 
                mysql_close(&aDbConnection->mhandle);
                //free(aDbConnection->mhandle);
            }
            break;
        #endif /* HAVE_MYSQL*/
    }
    // clean up the aDbConnection object
    //free(aDbConnection);

    return 1;
}





/* Tests a database connection and the underlying schema to see
 * if the connection is open, the schema version is supported by
 * this version of the code, and to see what permissions are 
 * available */ 
int testConnection(Connection *aDbConnection){
    int returnvalue = 0;
    int dbreturn;
    switch (aDbConnection->type) {
       #ifdef HAVE_POSTGIS
       case DB_POSTGIS: 
           // is the connection open  [required]
           if (aDbConnection->phandle!=NULL) { 
               // is the database spatially enabled [required]
               // are the needed tables present [required]
                    // check schema type (simple, simple+cad, full, aprsworld)
                    // check version of database schema for compatability
               testXastirVersionPostgis(aDbConnection);
               // does the user have select privileges [required]
               // does the user have update privileges [optional] 
               // does the user have inesrt privileges [optional]
               // does the user have delete privileges [optional]
           }
           break;
       #endif /* HAVE_POSTGIS */
       #ifdef HAVE_MYSQL_SPATIAL
       case DB_MYSQL_SPATIAL: 
           // is the connection open  [required]
           if (aDbConnection->mhandle!=NULL) { 
               dbreturn = mysql_ping(aDbConnection->mhandle);
               if (dbreturn>0) { 
                    mysql_interpret_error(dbreturn, aDbConnection);
               } else { 
                   // is the database spatially enabled [required]
                   // determine from db version >= 4.1
                   // mysql_server_version is new to mysql 4.1
                   dbreturn = mysql_get_server_version(aDbConnection->mhandle);
                   // are the needed tables present [required]
                     // check schema type (simple, simple+cad, full, aprsworld)
                     // check version of database schema for compatability
                     dbreturn = testXastirVersionMysql(aDbConnection);
                   // does the user have select privileges [required]
                   // does the user have update privileges [optional] 
                   // does the user have insert privileges [optional]
                   // does the user have delete privileges [optional]
               }
           }
           break;
       #endif /* HAVE_MYSQL_SPATIAL */
       #ifdef HAVE_MYSQL
       case DB_MYSQL: 
           // is the connection open  [required]
           if (aDbConnection->mhandle != NULL) { 
                dbreturn = mysql_ping(aDbConnection->mhandle);
                if (dbreturn>0) { 
                     mysql_interpret_error(dbreturn, aDbConnection);
                } else { 
                    // is the database spatially enabled [optional]
                    // determine from db version >= 4.1
                    #ifdef HAVE_MYSQL_SPATIAL
                    // mysql_server_version is new to mysql 4.1
                    dbreturn = mysql_get_server_version(aDbConnection->mhandle);
                    #endif /* HAVE_MYSQL_SPATIAL */
                    // are the needed tables present [required]
                      // check schema type (simple, simple+cad, aprsworld)
                      // full requires objects, not supported here.
                      // check version of database schema for compatability
                    dbreturn = testXastirVersionMysql(aDbConnection);
                    // does the user have select privileges [required]
                    // does the user have update privileges [optional] 
                    // does the user have insert privileges [optional]
                    // does the user have delete privileges [optional]
                }
           }
           break;
       #endif /* HAVE_MYSQL*/
    }
    return returnvalue;
}




// Layer 3: DBMS specific db storage code *************************************
// Functions in this section should be local to this file and not exported
// Export functions in section 2a above.  
//
// Layer 3a: DBMS specific GIS db storage code ********************************
// Functions supporting queries to specific types of GIS enabled databasesa
// 

#ifdef HAVE_SPATIAL_DB

#ifdef HAVE_POSTGIS
// Postgis implementation of spatial database functions





/* postgresql+postgis implementation of storeStationToGisDb().  */
int storeStationToGisDbPostgis(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    PGconn *conn = aDbConnection->phandle;
    // check type of schema to use (XASTIR simple, full or APRSWorld) 
    switch (aDbConnection->descriptor.database_schema_type) {
        case XASTIR_SCHEMA_SIMPLE : 
            returnvalue = storeStationSimplePointToGisDbPostgis(aDbConnection,aStation);
            break;
        case XASTIR_SCHEMA_APRSWORLD : 
            break;
        case XASTIR_SCHEMA_COMPLEX :
            break;
        // otherwise error message
    }
    return returnvalue;
}





/* postgresql+postgis implementation of storeCadToGisDb().  */
int storeCadToGisDbPostgis(Connection *aDbConnection, CADRow *aCadObject) { 
    int returnvalue = 0;
    PGconn *conn = aDbConnection->phandle;

    return returnvalue;
}





/* function storeStationSimplePointToGisDbPostgis()
 * Postgresql/Postgis implementation of wrapper storeStationSimplePointToGisDb().
 * Should only be called through wrapper function.  Do not call directly.
 */
int storeStationSimplePointToGisDbPostgis(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;  // Default return value is failure.
    int ok;  // Holds results of tests when building query.
    char wkt[MAX_WKT];  // well know text representation of latitude and longitude of point
    char timestring[101];  // string representation of the time heard or the current time
    char call_sign[(MAX_CALLSIGN*2)+1];  // temporary holding for escaped callsign
    char aprs_symbol[2];  // temporary holding for escaped aprs symbol
    char aprs_type[2];    // temporary holding for escaped aprs type
    char special_overlay[2];  // temporary holding for escaped overlay
    char origin[(MAX_CALLSIGN*2)+1]; // temporary holding for escaped origin
    char node_path[(NODE_PATH_SIZE*2)+1];  // temporary holding for escaped node_path
    char record_type[2];  // temporary holding for escaped record_type 
    //PGconn *conn = aDbConnection->phandle;
    PGresult *prepared;
    PGresult *result;
    int count;  // returned value from count query
    const int PARAMETERS = 9;
    // parameter arrays for prepared query
    const char *paramValues[PARAMETERS];  
    int paramLengths[PARAMETERS];  // ? don't need, just null for prepared insert ?
    int paramFormats[PARAMETERS];  // ? don't need, just null for prepared insert ?
    // To use native Postgres POINT for position instead of postgis geometry point.
    //const Oid paramTypes[6] = { VARCHAROID, TIMESTAMPTZOID, POINTOID, VARCHAROID, VARCHAROID, VARCHAROID };
    //const Oid paramTypes[6] = { 1043, 1184, 600, 1043, 1043, 1043 };
    // Native postgres (8.2) geometries don't have spatial support as rich as Postgis extensions.
    // use postgis geometry Point instead:
    // lookup OID for geometry:  select OID from pg_type where typname = 'geometry'; returns 19480
    //const Oid paramTypes[6] = { VARCHAROID, TIMESTAMPTZOID, 19480, VARCHAROID, VARCHAROID, VARCHAROID };
    //const Oid paramTypes[6] = { 1043, 1184, 19480, 1043, 1043, 1043 };
    // Value 18480 is probably installation specific, use unknownOID instead:
    //const Oid paramTypes[9] = { VARCHAROID, TIMESTAMPTZOID, UNKNOWNOID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID, VARCHAROID };
    const Oid paramTypes[9] = { 1043, 1184, 705, 1043, 1043, 1043, 1043, 1043, 1043 };
    const char *sql = "insert into simpleStation (station, transmit_time, position, symbol, overlay, aprstype, origin, record_type, node_path) values ($1, $2, $3, $4, $5, $6, $7, $8, $9)";
    const char *StatementName = "InsertSimpleStation";
    const char *StatementExists = "select count(*) from pg_prepared_statements where name = 'InsertSimpleStation'";

   
    if (debug_level & 1) 
        fprintf(stderr,"In postgres simple station insert\n");

    // Check to see if this prepared statement exists in the current session
    // and create it if it does not.  
    // Query adds connection overhead - should probably track with a global variable, 
    // and query/recreate statment only on failure.  
    ok = 0;
    // pg_prepared_statements system view added in postgresql 8.2  
    if (PQserverVersion(aDbConnection->phandle)>80199) { 
        result = PQexec(aDbConnection->phandle, "select count(*) from pg_prepared_statements where name = 'InsertSimpleStation'");
        result = PQexec(aDbConnection->phandle, StatementExists);
        if (result==NULL) { 
           fprintf(stderr,"Postgres Check for Prepared Query exec Failed: %s\n", PQerrorMessage(aDbConnection->phandle));
           xastir_snprintf(aDbConnection->errormessage,sizeof(aDbConnection->errormessage),PQerrorMessage(aDbConnection->phandle));
        } else {
            count = 0;
            if (PQresultStatus(result) == PGRES_TUPLES_OK) { 
                count = atoi(PQgetvalue(result,0,0));
            }
            if (count==0) { 
                // Statement doesn't exist, so prepare it, let PQprepare report on any error that got us a NULL result.
                prepared = PQprepare(aDbConnection->phandle, StatementName, sql, PARAMETERS, paramTypes);
                if (PQresultStatus(prepared)==PGRES_COMMAND_OK) { 
                    ok = 1;
                } else {
                   // error condition - can't prepare statement
                   fprintf(stderr,"Postgres Prepare Query Failed: %s\n", PQerrorMessage(aDbConnection->phandle));
                   xastir_snprintf(aDbConnection->errormessage,sizeof(aDbConnection->errormessage),PQerrorMessage(aDbConnection->phandle));
                   exit(1);
    
                }
            } else if (count==1) { 
                // prepared statement exists, we can go ahead with query.
                ok = 1;
            } else {
                fprintf(stderr,"Postgres Check for Prepared Query getvalue (count=%d) failed: %s\n",count, PQresultErrorMessage(result));
                xastir_snprintf(aDbConnection->errormessage,sizeof(aDbConnection->errormessage),PQresultErrorMessage(result));
            }
        }
    } else { 
        prepared = PQprepare(aDbConnection->phandle, StatementName, sql, PARAMETERS, paramTypes);
        ok = 1;
    }
    if (ok==1) {
        // native postgis POINT is (99.999 099.999) instead of POINT (99.999 099.999)
        // ok = xastirCoordToLatLongPosgresPoint(aStation->coord_lon, aStation->coord_lat, wkt);
        //
        // Prepared query is ready, get and fill in the parameter values
        // from the station provided, then fire the query.
        ok = xastirCoordToLatLongWKT(aStation->coord_lon, aStation->coord_lat, wkt);
        if (ok==1) { 
            // Postgresql 8 documentation indicates that escape string should not be performed
            // when calling PQexecParams or its sibling routines, not explicit, but implication
            // is that PQexecPrepared with passed parameters is a sibling routine and we 
            // shouldn't be running PQescapeStringConn() on the parameters.
            // If used, form would be:
            // PQescapeStringConn(conn,call_sign,aStation->call_sign,(MAX_CALLSIGN*2)+1,escape_error);
            xastir_snprintf(call_sign,MAX_CALLSIGN+1,"%s",aStation->call_sign);
            if (strlen(aStation->origin) > 0) { 
                xastir_snprintf(origin,strlen(aStation->origin)+1,"%s",aStation->origin);
            } else { 
                xastir_snprintf(origin,1,"%c",'\0');
            } 
            xastir_snprintf(record_type,2,"%c",aStation->record_type);
            if (aStation->node_path_ptr==NULL) { 
                xastir_snprintf(node_path,2," ");
            } else {  
                xastir_snprintf(node_path,strlen(aStation->node_path_ptr)+1,"%s",aStation->node_path_ptr);
            }   
 
            // Get time in seconds, adjust to datetime
            // If aStation is my station or another unset sec_heard is 
            // encountered, use current time instead. Conversely, use time
            // provided in sec_heard if sec_heard is an invalid time.
            get_iso_datetime(aStation->sec_heard,timestring,True,False);

            // set parameter values to call, transmit_time, and position
            paramValues[0]=call_sign;
            paramValues[1]=timestring;
            paramValues[2]=wkt;
            if (aStation->aprs_symbol.aprs_symbol==NULL) { 
                xastir_snprintf(aprs_symbol,2," ");
                paramValues[3]=&aprs_symbol;
            } else { 
                xastir_snprintf(aprs_symbol,2,"%c",aStation->aprs_symbol.aprs_symbol);
                paramValues[3]=aprs_symbol;
            }
            if (aStation->aprs_symbol.special_overlay==NULL) { 
                xastir_snprintf(special_overlay,2," ");
                paramValues[4]=&special_overlay;
            } else { 
                xastir_snprintf(special_overlay,2,"%c",aStation->aprs_symbol.special_overlay);
                paramValues[4]=&special_overlay;
            }
            if (aStation->aprs_symbol.aprs_type==NULL) { 
                xastir_snprintf(aprs_type,2," ");
                paramValues[5]=&aprs_type;
            } else { 
                xastir_snprintf(aprs_type,2,"%c",aStation->aprs_symbol.aprs_type);
                paramValues[5]=aprs_type;
            }
            paramValues[6]=origin;
            paramValues[7]=record_type;
            paramValues[8]=node_path;

            if (debug_level & 1)  {
                 fprintf(stderr,"Inserting: Call: %s, Time: %s, Position: %s, Symbol:%s,%s,%s Origin:%s, Node_path:%s, Record type:%s\n",call_sign,timestring,wkt,paramValues[3],paramValues[4],paramValues[5],paramValues[6],paramValues[8],paramValues[7]);
            }
       
            // send query
            result = PQexecPrepared(aDbConnection->phandle,StatementName,PARAMETERS,paramValues,NULL,NULL,POSTGRES_RESULTFORMAT_TEXT);
            if (PQresultStatus(result)!=PGRES_COMMAND_OK) { 
                   fprintf(stderr,"Insert query failed:%s\n",PQresultErrorMessage(result));
                   // error, get error message.
                   xastir_snprintf(aDbConnection->errormessage,sizeof(aDbConnection->errormessage),PQresultErrorMessage(result));
            } else { 
                 // query was successfull
                returnvalue=1;
            }    
        } else { 
            // problem with coordinates of station 
            xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Error converting latitude or longitude from xastir coordinates: %ld,%ld",aStation->coord_lat,aStation->coord_lon);
        }
    }
    PQclear(result);
    PQclear(prepared);
    return returnvalue;
}





/* function add_simple_station() 
 * adds an xastir DataRow using station and additional data from a simpleStation 
 * record in a SQL database.  
 * @param p_new_station Pointer to a DataRow for the new station, probably initalized as DataRow p_new_station = NULL
 * @param station
 * @param origin
 * @param symbol
 * @param overlay
 * @param aprs_type
 * @param latitude
 * @param longitude
 * @param record_type
 * @param node_path
 * @param transmit_time
 *
 * @returns 0 if unable to add new station (p_new_station should be null)
 * otherwise returns 1 (and p_new_station should be a pointer to the DataRow 
 * for the new station record.
 */
/*
int add_simple_station(DataRow *p_new_station,char *station, char *origin, char *symbol, char *overlay, char *aprs_type, char *latitude, char *longitude, char *record_type, char *node_path, char *transmit_time) { 
    int returnvalue = 0;
    unsigned long x;  // xastir coordinate for longitude
    unsigned long y;  // xastir coordinate for latitide
    float lat;  // latitude converted from retrieved string
    float lon;  // longitude converted from retrieved string
    DataRow *p_time;  // pointer to new station record  
    struct tm time;

    // Add a datarow using the retrieved station record from the postgis database.
    p_time = NULL;
    p_new_station = add_new_station(p_new_station,p_time,station);
    if (!(p_new_station==NULL)) { 
        // set values for new station based on the database row
        xastir_snprintf(p_new_station->origin,58,"%s",origin);
        p_new_station->aprs_symbol.aprs_symbol = symbol[0];
        p_new_station->aprs_symbol.special_overlay = overlay[0];
        p_new_station->aprs_symbol.aprs_type = aprs_type[0];
        lat = strtof(latitude,NULL);
        lon = strtof(longitude,NULL);
        if (convert_to_xastir_coordinates (&x, &y, lon, lat)) {
           p_new_station->coord_lon = x;
           p_new_station->coord_lat = y;
        }
        p_new_station->record_type = record_type[0];
        p_new_station->node_path_ptr= node_path;
        // also set flags for the station 
        p_new_station->flag |= ST_ACTIVE;
        if (position_on_extd_screen(p_new_station->coord_lat,p_new_station->coord_lon)) {
            p_new_station->flag |= (ST_INVIEW);   // set   "In View" flag
        }
        else {
            p_new_station->flag &= (~ST_INVIEW);  // clear "In View" flag
        }
        p_new_station->data_via = 'F';  // treat as data from a file.
        if (strlen(transmit_time) > 0) { 
            strptime(transmit_time,"%Y-%m-%d %H:%M:%S",&time);
            p_new_station->sec_heard = mktime(&time);
        }
        returnvalue = 1;
    }
    return returnvalue;
}
*/




/* function testXastirVersionPostgis()
 * Postgresql/Postgis implementation of wrapper testXastirVersionPostgis().
 * Should only be called through wrapper function.  Do not call directly.
 */
int testXastirVersionPostgis(Connection *aDbConnection) {
    int returnvalue = 0;
    char sql[100] = "select version_number from version order by version_number desc limit 1";  
    PGconn *conn = aDbConnection->phandle;

    return returnvalue;
}





/* function getAllSimplePositionsPostgis()
 * Postgresql/Postgis implementation of wrapper getAllSimplePositions().
 * Should only be called through wrapper function.  Do not call directly.
 */
int getAllSimplePositionsPostgis(Connection *aDbConnection) {
    int returnvalue = 0;  // value to return from function, 1 for success, 0 for failure
    int row;  // row counter for result set loop
    int station_count = 0;  // number of new stations retrieved
    unsigned long x;  // xastir coordinate for longitude
    unsigned long y;  // xastir coordinate for latitide
    float lat;  // latitude converted from retrieved string
    float lon;  // longitude converted from retrieved string
    const char *sql = "select station, symbol, overlay, aprstype, transmit_time, AsText(position), origin, record_type, node_path, X(position), Y(position) from simpleStation order by station, transmit_time";
    // station is column 0, symbol is column 1, etc.
    PGconn *conn = aDbConnection->phandle;
    char *feedback[100];
    DataRow *p_new_station;  // pointer to new station record  
    DataRow *p_time;  // pointer to new station record  

    // run query and retrieve result set
    PGresult *result = PQexec(conn,sql);

    if (result==NULL) { 
       // PQexec probably couldn't allocate memory for the result set.
       xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Null result: %s\n",PQerrorMessage(conn));
       fprintf(stderr, "getAllSimplePositionsPostgis() Null result\nPostgresql Error : %s\n",PQerrorMessage(conn));
    } else { 
       // PQexec returned a result, but it may not be valid, check to see.
       if (PQresultStatus(result)==PGRES_COMMAND_OK || PQresultStatus(result)==PGRES_TUPLES_OK) { 
          // PQexec returned a valid result set.
          xastir_snprintf(feedback,100,"Retreiving %i Postgis records\n",PQntuples(result));
          stderr_and_statusline(feedback);
          for (row=0; row<PQntuples(result); row++) {
              // step through rows in result set and add each to xastir db as a minimal DataRow
              if (PQgetisnull(result,row,0)) {
                  // station name is null, skip.
              } else {
                  // check if station exists
                  p_new_station = NULL;
                  if (search_station_name(&p_new_station,PQgetvalue(result,row,0),1)) {  
                       // This station allready exists as a DataRow in the xastir db.
                       // Don't create a duplicate record, but add to the DataRow's track.

                       // ? check if it is a mobile station ?       
                       
                       // if station doesn't have a track, allocate memory for one

                       // add position and time to track

                  } else { 
                       // This station isn't in the xastir db. 
                       add_simple_station(p_new_station,PQgetvalue(result,row,0), PQgetvalue(result,row,6), PQgetvalue(result,row,1), PQgetvalue(result,row,2), PQgetvalue(result,row,3), PQgetvalue(result,row,10), PQgetvalue(result,row,9), PQgetvalue(result,row,7), PQgetvalue(result,row,8), PQgetvalue(result,row,4));
                    
/*
                       // Add a datarow using the retrieved station record from the postgis database.
                       p_time = NULL;
                       p_new_station = add_new_station(p_new_station,p_time,PQgetvalue(result,row,0));
                       // set values for new station based on the database row
                       xastir_snprintf(p_new_station->origin,58,"%s",PQgetvalue(result,row,6));
                       p_new_station->aprs_symbol.aprs_symbol = PQgetvalue(result,row,1)[0];
                       p_new_station->aprs_symbol.special_overlay = PQgetvalue(result,row,2)[0];
                       p_new_station->aprs_symbol.aprs_type = PQgetvalue(result,row,3)[0];
                       lat = strtof(PQgetvalue(result,row,9),NULL);
                       lon = strtof(PQgetvalue(result,row,10),NULL);
                       if (convert_to_xastir_coordinates (&x, &y, lon, lat)) {
                          p_new_station->coord_lon = x;
                          p_new_station->coord_lat = y;
                       }
                       p_new_station->record_type = PQgetvalue(result,row,7)[0];
                       // also set flags for the station 
                       p_new_station->flag |= ST_ACTIVE;
                       if (position_on_extd_screen(p_new_station->coord_lat,p_new_station->coord_lon)) {
                           p_new_station->flag |= (ST_INVIEW);   // set   "In View" flag
                       }
                       else {
                           p_new_station->flag &= (~ST_INVIEW);  // clear "In View" flag
                       }
*/
                       station_count ++;
                  }  // end else, new station
              } // end else, station is not null 
          } // end for loop stepping through rows
          redo_list = (int)TRUE;      // update active station lists
          xastir_snprintf(feedback,100,"Added %d stations from Postgis\n",station_count);
          stderr_and_statusline(feedback);
       } else {       
           // sql query had a problem retrieving result set.
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "%s %s\n",PQresStatus(PQresultStatus(result)),PQerrorMessage(conn));
           fprintf(stderr, "getAllSimplePositionsPostgis() %s\nPostgresql Error : %s\n",PQresStatus(PQresultStatus(result)),PQerrorMessage(conn));
       }
       // done with result set, so free the resource.
       PQclear(result);
    } 
    return returnvalue;
}





/* function getAllSimplePositionsPostgisInBoundingBox()
 * Postgresql/Postgis implementation of wrapper getAllSimplePositionsInBoundingBox().
 * Should only be called through wrapper function.  Do not call directly.
 */
int getAllSimplePositionsPostgisInBoundingBox(Connection *aDbConnection, char* str_e_long, char* str_w_long, char* str_n_lat, char* str_s_lat) {
    int returnvalue = 0;
    // set up prepared query with bounding box 
    // postgis simple table uses POINT
    char sql[100] = "select call, transmit_time, position from simpleStation where ";  
    PGconn *conn = aDbConnection->phandle;
   

    return returnvalue;
}




#endif /* HAVE_POSTGIS */

#ifdef HAVE_MYSQL_SPATIAL
// Mysql 5 implementation of spatial database functions





/* function storeStationToGisDbMysql
 * MySQL implemenation of storeStationToGisDb
 * Should be private to db_gis.c 
 * Should only be called through wrapper function.  Do not call directly.
 * @param aDbConnection an exastir database connection struct describing
 * the connection.
 * @param aStation
 * Returns 0 for failure, 1 for success.  
 * If failure, stores error message in aDbConnection->errormessage.
 */
int storeStationToGisDbMysql(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    int mysqlreturn;
    // check type of schema to use (XASTIR simple, full or APRSWorld) 
    switch (aDbConnection->descriptor.database_schema_type) {
        case XASTIR_SCHEMA_SIMPLE : 
            returnvalue = storeStationSimplePointToGisDbMysql(aDbConnection,aStation);
            break;
        case XASTIR_SCHEMA_APRSWORLD : 
            break;
        case XASTIR_SCHEMA_COMPLEX :
            break;
        // otherwise error message
    }
    return returnvalue;
}






/* function storeCadToGisDbMysql
 * MySQL implementation of storeCadToGisDbMysql
 * Should be private to db_gis.c 
 * Should only be called through wrapper function.  Do not call directly.
 * @param aDbConnection an exastir database connection struct describing
 * the connection.
 * @param aCadObject
 * Returns 0 for failure, 1 for success.  
 * If failure, stores error message in aDbConnection->errormessage.
 */
int storeCadToGisDbMysql(Connection *aDbConnection, CADRow *aCadObject) { 
    int returnvalue = 0;
    int mysqlreturn;
   
    return returnvalue;
}






/* support function for prepared statements
int bind_mysql_parameter(MYSQL_BIND *bind, buffer, int length, int buffer_length, my_bool is_null) {
       bind[0]->buffer =  &station->call_sign;
       bind[0]->length = strlen(station->call_sign);
       bind[0]->buffer_length = MAX_CALL;
       bind[0]->buffer_type = MYSQL_TYPE_STRING
       bind[0]->is_null = false;
}
*/





/* function storeStationSimplePointToGisDbMysql()
 * MySQL implementation of wrapper storeStationSimplePointToGisDb().
 * Should be private to db_gis.c 
 * Should only be called through wrapper function.  Do not call directly.
 * @param aDbConnection an xastir database connection struct describing
 * the connection.
 * @param aStation
 * Returns 0 for failure, 1 for success.  
 * On failure sets error message in aDbConnection->errormessage.
 */
int storeStationSimplePointToGisDbMysql(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    int mysqlreturn;  // hold return value of mysql query
    int param_count;  // check on the number of parameters present in the prepared statement
    int ok;    // variable to store results of tests preparatory to firing query
    // temporary holding variables for bind buffers
    char wkt[MAX_WKT];  // well know text representation of latitude and longitude of point
    char aprs_symbol[2];  // temporary holding for escaped aprs symbol
    char aprs_type[2];    // temporary holding for escaped aprs type
    char special_overlay[2];  // temporary holding for escaped overlay
    char record_type[2];              // temporary holding for escaped record type
    char origin[MAX_CALLSIGN+1];  // temporary holding for escaped origin
    char node_path[NODE_PATH_SIZE];         // temporary holding for escaped node_path_ptr
    MYSQL_STMT *statement;
    // bind string lengths
    int call_sign_length;
    int wkt_length;
    int aprs_symbol_length;
    int aprs_type_length;
    int special_overlay_length;
    int origin_length;
    int record_type_length;
    int node_path_length;
    // time
    MYSQL_TIME timestamp;
    char timestring[100+1];
    time_t secs_now;
    struct tm *ts;  // to convert time to component parts for bind.buffer_type MYSQL_TYPE_DATETIME
    // define prepared statement and matching bind array
    #define SQL "INSERT INTO simpleStationSpatial (station, transmit_time, position, symbol, overlay, aprstype, origin, record_type, node_path) VALUES (?,?,PointFromText(?),?,?,?,?,?,?)"
    MYSQL_BIND bind[9];   // bind array for prepared query.
    int parameters = 9;
    // Note:
    // bind[9], SQL "?????????", and param_count must all match value of parameters
    // nine bound parameters, nine question marks in the statement, and param_count returned as nine.

    if (debug_level & 1) 
        fprintf(stderr,"in storeStationSimplePointToGisDbMysql\n");    
        
    statement = mysql_stmt_init(&aDbConnection->mhandle);
    if (!statement) { 
       fprintf(stderr,"Unable to create mysql prepared statement.  May be out of memmory.\n");    
    }
    mysql_stmt_prepare(statement, SQL, strlen(SQL));
    if (!statement) { 
        mysql_interpret_error(*mysql_error(&aDbConnection->mhandle),aDbConnection);
    } else { 
       // test to make sure that statement has the correctg number of parameters
       param_count= mysql_stmt_param_count(statement);
       if (param_count!=parameters) { 
           fprintf(stderr,"Number of bound parameters %d does not match expected value %d\n",param_count,parameters);
       } else {
           // set up the buffers 
           memset(bind, 0, sizeof(bind));

           bind[0].buffer =  (char *)&aStation->call_sign;
           bind[0].length = &call_sign_length;
           bind[0].buffer_length = MAX_CALLSIGN;
           bind[0].buffer_type = MYSQL_TYPE_STRING;
           bind[0].is_null = 0;

           bind[1].buffer = (char *)&timestamp;
           bind[1].length = 0;
           bind[1].buffer_type = MYSQL_TYPE_DATETIME;
           bind[1].is_null = 0;

           bind[2].buffer = (char *)&wkt;
           bind[2].length = &wkt_length;
           bind[2].buffer_length = MAX_WKT;
           bind[2].buffer_type = MYSQL_TYPE_STRING;
           bind[2].is_null = 0;

           bind[3].buffer = (char *)&aprs_symbol;
           bind[3].length = &aprs_symbol_length;
           bind[3].buffer_length = 2;
           bind[3].buffer_type = MYSQL_TYPE_STRING;
           bind[3].is_null = 0;

           bind[4].buffer = (char *)&special_overlay;
           bind[4].length = &special_overlay_length;
           bind[4].buffer_length = 2;
           bind[4].buffer_type = MYSQL_TYPE_STRING;
           bind[4].is_null = 0;

           bind[5].buffer = (char *)&aprs_type;
           bind[5].length = &aprs_type_length;
           bind[5].buffer_length = 2;
           bind[5].buffer_type = MYSQL_TYPE_STRING;
           bind[5].is_null = 0;

           bind[6].buffer = (char *)&origin;  // segfaults with origin of zero length, otherwise writes bad data
           bind[6].length = &origin_length;
           bind[6].buffer_length = MAX_CALLSIGN;
           bind[6].buffer_type = MYSQL_TYPE_STRING;
           bind[6].is_null = 0;

           bind[7].buffer = (char *)&record_type;
           bind[7].length = &record_type_length;
           bind[7].buffer_length = 2;
           bind[7].buffer_type = MYSQL_TYPE_STRING;
           bind[7].is_null = 0;

           bind[8].buffer = (char *)&node_path; 
           bind[8].length = &node_path_length;
           bind[8].buffer_length = NODE_PATH_SIZE;
           bind[8].buffer_type = MYSQL_TYPE_STRING;
           bind[8].is_null = 0;

           ok = mysql_stmt_bind_param(statement,bind);
           if (ok!=0) { 
               fprintf(stderr,"Error binding parameters to mysql prepared statement.\n");                   
               mysql_interpret_error(mysql_errno(&aDbConnection->mhandle),aDbConnection);
               fprintf(stderr,mysql_stmt_error(statement));
           } else { 

               // get call, time, and position
               // call is required
               if (aStation->call_sign!=NULL && strlen(aStation->call_sign)>0) { 
                   call_sign_length = strlen(aStation->call_sign);
        
                   // get time in seconds, adjust to datetime
                   // If my station or another unset sec_heard is 
                   // encountered, use current time instead, use time
                   // provided if it was invalid.
                   get_iso_datetime(aStation->sec_heard,timestring,True,False);
                   if ((int)aStation->sec_heard==0 ) { 
                       secs_now = sec_now();
                       ts = localtime(&secs_now);
                   } else { 
                       ts = localtime(&aStation->sec_heard);
                   }
                   timestamp.year = ts->tm_year + 1900;  // tm_year is from 1900
                   timestamp.month = ts->tm_mon + 1;     // tm_mon is from 0
                   timestamp.day = ts->tm_mday;          // tm_mday is from 1
                   timestamp.hour = ts->tm_hour;
                   timestamp.minute = ts->tm_min;
                   timestamp.second = ts->tm_sec;
                   ok = xastirCoordToLatLongWKT(aStation->coord_lon, aStation->coord_lat, wkt);
                   if (ok==1) { 
                       wkt_length = strlen(wkt);


                       if (aStation->aprs_symbol.aprs_symbol==NULL) { 
                           xastir_snprintf(aprs_symbol,2,"%c",'\0');
                       } else { 
                           xastir_snprintf(aprs_symbol,2,"%c",aStation->aprs_symbol.aprs_symbol);
                       }
                       aprs_symbol_length = strlen(aprs_symbol);

                       if (aStation->aprs_symbol.aprs_type==NULL) { 
                           xastir_snprintf(aprs_type,2,"%c",'\0');
                       } else { 
                           xastir_snprintf(aprs_type,2,"%c",aStation->aprs_symbol.aprs_type);
                       }
                       aprs_type_length = strlen(aprs_type);

                       if (aStation->aprs_symbol.special_overlay==NULL) { 
                           xastir_snprintf(special_overlay,2,"%c",'\0');
                       } else { 
                           xastir_snprintf(special_overlay,2,"%c",aStation->aprs_symbol.special_overlay);
                       }
                       special_overlay_length = strlen(special_overlay);

                       if (aStation->origin==NULL) { 
                           //xastir_snprintf(origin,2,"%c",'\0');
                           origin[0]='\0';
                       } else { 
                           xastir_snprintf(origin,MAX_CALLSIGN+1,"%s",aStation->origin);
                       }
                       origin_length = strlen(origin);

                       if (aStation->record_type==NULL) { 
                           //xastir_snprintf(record_type,2,"%c",'\0');
                           record_type[0]='\0';
                       } else { 
                           xastir_snprintf(record_type,2,"%c",aStation->record_type);
                       }
                       record_type_length = strlen(record_type);
                       
                       if (aStation->node_path_ptr==NULL) { 
                           //xastir_snprintf(node_path,2,"%c",'\0');
                           node_path[0]='\0';
                       } else { 
                           xastir_snprintf(node_path,strlen(aStation->node_path_ptr)+1,"%s",aStation->node_path_ptr);
                       }
                       node_path_length = strlen(node_path);

                       // all the bound parameters should be available and correct
                       if (debug_level & 1) 
                          fprintf(stderr,"saving station %s  %d %d %d %d:%d:%d wkt=%s \n",aStation->call_sign,ts->tm_year,ts->tm_mon,ts->tm_mday,ts->tm_hour,ts->tm_min,ts->tm_sec,wkt);
                       // send query
                       mysqlreturn = mysql_stmt_execute(statement);
                       if (mysqlreturn!=0) { 
                           returnvalue=0;
                           fprintf(stderr,"%s\n",mysql_stmt_error(statement));
                           mysql_interpret_error(mysqlreturn,aDbConnection);
                       } else {
                           returnvalue=1;
                       }
                   } else { 
                        fprintf(stderr,"Unable to save station to mysql db, Error converting latitude or longitude form xastir coordinates\n");                   
                        xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Error converting latitude or longitude from xastir coordinates: %d,%d",aStation->coord_lat,aStation->coord_lon);
                   }
               } else { 
                   // set call not null error message
                   fprintf(stderr,"Unable to save station to mysql db, Station call sign was blank or null.\n");                   
                   xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Station callsign is required and was blank or null.");
               }
           } // end of bind check
       }  // end of parameter count check
    }
  
    mysql_stmt_close(statement);

    return returnvalue;
}






int getAllSimplePositionsMysqlSpatial(Connection *aDbConnection) {
    int returnvalue = 0;
    int mysqlreturn;
    DataRow *p_new_station;
    int station_count = 0;  // number of new stations retrieved
    char *s_lat[13];  // string latitude
    char *s_lon[13];  // string longitude
    float lat;  // latitude converted from retrieved string
    float lon;  // longitude converted from retrieved string
    char *feedback[100];
    struct tm time;
    int skip; // used in identifying mobile stations
    char sql[] = "select station, transmit_time, AsText(position), symbol, overlay, aprstype, origin, record_type, node_path from simpleStationSpatial order by station, transmit_time";
    MYSQL_RES *result;
    MYSQL_ROW *row;
    int ok;   // to hold mysql_query return value
    ok = mysql_query(&aDbConnection->mhandle,sql);
    if (ok==0) { 
        result = mysql_use_result(&aDbConnection->mhandle);
        if (result!=NULL) { 
            xastir_snprintf(feedback,100,"Retrieving MySQL records\n");
            stderr_and_statusline(feedback);
            // with mysql_use_result each call to mysql_fetch_row retrieves
            // a row of data from the server.  Mysql_store_result might use
            // too much memory in retrieving a large result set all at once.
            row = mysql_fetch_row(result);
            while (row != NULL) { 
               // retrieve data from the row
               // test to see if this is a valid station
               if (row[0]==NULL) { 
                  // station is null, skip
               } else { 
                  p_new_station = NULL;
                  if (search_station_name(&p_new_station,row[0],1)) {  
                       // This station is allready in present as a DataRow in the xastir db.
                       // check to see if this is likely to be a mobile station 

                       // can't easily work off of position becaue of rounding errors, exclude stations that are likely to be fixed
                       // _/ = wx
                       skip = 0;
                       if ((row[3]=='_') && (row[5]=='/')) { 
                           skip = 1;   // wx
                       }


                       if (skip==0) { 
                           // add to track
                       }

                       // Add data to the station's track.
                  } else { 
                       // This station isn't in the xastir db. 
                       // Add a datarow using the retrieved station record from the postgis database.
                       lat = xastirWKTPointToLatitude(row[2]); 
                       lon = xastirWKTPointToLongitude(row[2]); 
                       xastir_snprintf(s_lat,13,"%3.6f",lat);
                       xastir_snprintf(s_lon,13,"%3.6f",lon);
                       add_simple_station(p_new_station, row[0], row[6], row[3], row[4], row[5], s_lat, s_lon, row[7], row[8], row[1]);

                       station_count++;
                  }
               }
               // try to fetch the next row
               row = mysql_fetch_row(result);
            }
        } else { 
            // error fetching the result set
            fprintf(stderr,"mysql error: %s\n",mysql_error(&aDbConnection->mhandle));
            mysql_interpret_error(mysql_errno(&aDbConnection->mhandle),aDbConnection);
        }
        xastir_snprintf(feedback,100,"Retreived %d new stations from MySQL\n",station_count);
        stderr_and_statusline(feedback);
        mysql_free_result(result);
    } else { 
       // query didn't execute correctly
       mysql_interpret_error(ok,aDbConnection);
    }

    return returnvalue;
}






int getAllCadFromGisDbMysql(Connection *aDbConnection) { 
    int returnvalue = 0;
    int mysqlreturn;
    MYSQL *conn = aDbConnection->mhandle;
     
    return returnvalue;
}





int getAllSimplePositionsMysqlSpatialInBoundingBox(Connection *aDbConnection, char* str_e_long, char* str_w_long, char* str_n_lat, char* str_s_lat) {
    int returnvalue = 0;
    int mysqlreturn;
    MYSQL *conn = aDbConnection->mhandle;
     
    return returnvalue;

}




/* 

  // some thoughts on database schema elements

  create database xastir;
  grant select on xastir to user xastir_user@localhost identified by encrypted password '<password>';
  
  create table version (
     version_number int,
     compatable_series int
  );
  grant select on version to xastir_user@localhost
  insert into version (version_number) values (XASTIR_SPATIAL_DB_VERSION);
  insert into version (version_number) values (XASTIR_SPATIAL_DB_COMPATIBLE_SERIES);

  # should be minimum fields needed to populate a DataRow and a related 
  # APRS_Symbol in xastir 
  create table simpleStation (
     simpleStationId int primary key not null auto_increment
     station varchar(MAX_CALLSIGN) not null,  # callsign of station, length up to max_callsign
     symbol varchar(1),     # aprs symbol character
     overlay varchar(1),    # aprs overlay table character
     aprstype varchar(1),    # aprs type, required???
     transmit_time datetime not null default now(),  # transmission time, if available, otherwise storage time
     position POINT   # position of station or null if latitude and longitude are not available
  );

                                                                          

  grant select, insert on simpleStation to xastir_user@localhost;


  create table datarow (
      datarow_id int not null primary key auto_increment,
      call_sign varchar(10) not null,
      tactical_call_sign varchar() not null default '',
      c_aprs_symbol_id int 
      location POINT,
      time_sn int,
      sec_heard long,
      heard_via_tnc_last_time long,
      direct_heard long,
      packet_time varchar,
      pos_time varchar,
      flag int,
      pos_amb varchar(1),
      error_ellipse_radius int,
      lon_precision int,
      lat_precision int,
      trail_color int,
      record_type varchar(1),
      data_via varchar(1),
      heard_via_tnc_port int, 
      last_port_heard int,
      num_packets int,
      altitude varchar([MAX_ALTITUDE]),
      speed varchar([MAX_SPEED+1]),
      course varchar([MAX_COURSE+1]),
      bearing varchar([MAX_COURSE+1]),
      NRQ varchar([MAX_COURSE+1]),
      power_gain varchar([MAX_POWERGAIN+1]),
      signal_gain varchar([MAX_POWERGAIN+1])
  );

  

*/

#endif /* HAVE_MYSQL_SPATIAL */

#endif /* HAVE_SPATIAL_DB */






// Layer 3b: DBMS specific db storage code for non spatial databases **********
// Functions supporting queries to specific types of databases that lack 
// spatial extensions.  Limited to storing points using latitude and longitude
// fields without spatial objects or spatial indexing.  
// 

#ifdef HAVE_MYSQL
// functions for MySQL database version < 4.1, or MySQL schema objects that don't 
// include spatial indicies.  
//
//********* Support for MySQL < 4.1 is depreciated  *****************************
//********* Expect MySQL support to be limited to MySQL 5+ **********************
//





/* function storeStationSimplePointToDbMysql()
 * MySQL implementation of wrapper storeStationSimplePointToGisDb().
 * Should be private to db_gis.c
 * Should only be called through wrapper function.  Do not call directly.
 * Returns 0 for failure, 1 for success.  
 * If failure, stores error message in aDbConnection->errormessage.
 */
int storeStationSimplePointToDbMysql(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;  // default return value is failure.
    int mysqlreturn = 1;  // result of sending mysql query.
    char sql[400];
    // Next three variables are one character in two bytes plus one character for
    // filling by mysql_real_escape_string().  
    char aprs_symbol[3];  // temporary holding for escaped aprs symbol
    char aprs_type[3];    // temporary holding for escaped aprs type
    char special_overlay[3];  // temporary holding for escaped overlay
    char call_sign[(MAX_CALLSIGN)*2+1];   // temporary holding for escaped callsign
    char origin[(MAX_CALLSIGN)*2+1];  // temporary holding for escaped origin
    char record_type[3];              // temporary holding for escaped record type
    char node_path[(NODE_PATH_SIZE*2)+1];         // temporary holding for escaped node_path_ptr
    float longitude;
    float latitude;
    int ok;
    char timestring[100+1];
    char *to[3];  // write to value for mysql_real_escape_string 

    if (debug_level & 1) 
        fprintf(stderr,"In storestationsimpletodbmysql()\n");

    // prepared statements not present below MySQL version 4.1
    // details of prepared statement support changed between versions 4.1 and 5.0.  
    // char [] sql = "insert into simpleStation (call, transmit_time, latitude, longitude) values ('%1','%2','%3','%4'))";
    // call is a required element for a simple station
    if (aStation!=NULL && aStation->call_sign!=NULL && strlen(aStation->call_sign)>0) {
        // get time in seconds, adjust to datetime
        // If my station or another unset sec_heard is 
        // encountered, use current time instead, use time
        // provided if it was invalid.
        get_iso_datetime(aStation->sec_heard,timestring,True,False);
        // get coord_lat, coord_long in xastir coordinates and convert to decimal degrees
        ok = convert_from_xastir_coordinates (&longitude, &latitude, aStation->coord_lon, aStation->coord_lat);
        // latitude and longitude are required elements for a simple station record.
        if (ok==1) { 
            // build insert query with call, time, and position
            // handle special cases of null, \ and ' characters in type, symbol, and overlay.
            if (aStation->aprs_symbol.aprs_symbol==NULL) { 
                xastir_snprintf(aprs_symbol,2,"%c",'\0');
            } else { 
                xastir_snprintf(aprs_symbol,2,"%c",aStation->aprs_symbol.aprs_symbol);
                mysql_real_escape_string(&aDbConnection->mhandle,&aprs_symbol,aprs_symbol,1);
            }
            if (aStation->aprs_symbol.aprs_type==NULL) { 
                xastir_snprintf(aprs_type,2,"%c",'\0');
            } else { 
                xastir_snprintf(aprs_type,2,"%c",aStation->aprs_symbol.aprs_type);
                mysql_real_escape_string(&aDbConnection->mhandle,&aprs_type,aprs_type,1);
            }
            if (aStation->aprs_symbol.special_overlay==NULL) { 
                xastir_snprintf(special_overlay,2,"%c",'\0');
            } else { 
                xastir_snprintf(special_overlay,2,"%c",aStation->aprs_symbol.special_overlay);
                mysql_real_escape_string(&aDbConnection->mhandle,&special_overlay,special_overlay,1);
            } 
  
            // Need to escape call sign - may contain special characters: 
            // insert into simpleStation (station, symbol, overlay, aprstype, transmit_time, latitude, longitude) 
            // values ('Fry's','/\0\0',' ','//\0','2007-08-07 21:55:43 -0400','47.496834','-122.198166')
            mysql_real_escape_string(&aDbConnection->mhandle,&call_sign,&(aStation->call_sign),strlen(aStation->call_sign));

            if (strlen(aStation->origin) > 0) { 
                mysql_real_escape_string(&aDbConnection->mhandle,&origin,&(aStation->origin),strlen(aStation->origin));
            } else { 
                xastir_snprintf(origin,2,"%c",'\0');
            } 
            if (aStation->node_path_ptr==NULL) { 
                xastir_snprintf(node_path,2,"%c",'\0');
            } else { 
                //mysql_real_escape_string(conn,&node_path,aStation->node_path_ptr,((strlen(aStation->node_path_ptr)*2)+1));
                xastir_snprintf(node_path,strlen(aStation->node_path_ptr)+1,"%s",aStation->node_path_ptr);
            } 
            
            xastir_snprintf(sql,sizeof(sql),"insert into simpleStation (station, symbol, overlay, aprstype, transmit_time, latitude, longitude, origin, record_type, node_path) values ('%s','%s','%s','%s','%s','%3.6f','%3.6f','%s','%c','%s')", aStation->call_sign, aprs_symbol, special_overlay, aprs_type,timestring,latitude,longitude,origin,aStation->record_type,node_path);

            if (debug_level & 1) 
                fprintf(stderr,"MySQL Query:\n%s\n",sql);

            // send query 
            mysql_ping(&aDbConnection->mhandle);
            mysqlreturn = mysql_real_query(&aDbConnection->mhandle, sql, strlen(sql)+1);
            if (mysqlreturn!=0) { 
                // get the mysql error message
                fprintf(stderr,mysql_error(&aDbConnection->mhandle));
                fprintf(stderr,"\n");
                mysql_interpret_error(mysqlreturn,aDbConnection);
            } else {
                // insert query was successfull, return value is ok.
                returnvalue=1;
            }
        } else { 
            xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Error converting latitude or longitude from xastir coordinates: %ld,%ld",aStation->coord_lat,aStation->coord_lon);
        } 
    } else { 
        // set call not null error message
        xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Station callsign is required and was blank or null.");
    }
    return returnvalue;
}





/* function testXastirVersionMysql() 
 * checks the xastir database version number of a connected MySQL database against the 
 * version range supported by the running copy of xastir.
 * @param aDbConnection pointer to a Connection struct describing the connection
 * @returns 0 if incompatable, 1 if compatable, -1 on connection failure.
 *
 * db       program
 * v  cs    v   cs    compatable
 * 1  1     1   1     1    identical 
 * 2  1     1   1     1    database newer than program (added fields, not queried)
 * 1  1     2   1     0    program newer than database (added fields, queries fail).
 * 3  2     2   1     0    different series
 * 2  1     3   2     0    different series
 *
 * TODO: Needs to include schema 
 */
int testXastirVersionMysql(Connection *aDbConnection) {
    int returnvalue = -1;
    MYSQL_RES *result;
    MYSQL_ROW *row;
    char sql[] = "select version_number from version order by version_number desc limit 1";  
    int ok;   // to hold mysql_query return value
    ok = mysql_query(&aDbConnection->mhandle,sql);
    if (ok==0) { 
        result = mysql_use_result(&aDbConnection->mhandle);
        if (result!=NULL) { 
            row = mysql_fetch_row(result);
            if (row != NULL) { 
                (int)row[0];
                if (row[0] == XASTIR_SPATIAL_DB_VERSION) { 
                   returnvalue = 1;
                } else { 
                    if (row[1] < XASTIR_SPATIAL_DB_VERSION && row[1] == XASTIR_SPATIAL_DB_COMPATABLE_SERIES) {
                        returnvalue = 1;
                    } else { 
                        returnvalue = 0;
                    }
                }
            } else {
                // result returned, but no rows = incompatable
                returnvalue = 0;
            }
        }
    } 
    return returnvalue;
}





/* function storeStationToDbMysql() 
 */
int storeStationToDbMysql(Connection *aDbConnection, DataRow *aStation){
    int returnvalue = 0;
    // check type of schema to use (XASTIR simple, full or APRSWorld) 
    switch (aDbConnection->descriptor.database_schema_type) {
        case XASTIR_SCHEMA_SIMPLE : 
            returnvalue = storeStationSimplePointToDbMysql(aDbConnection,aStation);
            break;
        case XASTIR_SCHEMA_APRSWORLD : 
            break;
        case XASTIR_SCHEMA_COMPLEX :
            break;
        // otherwise error message
    }
    return returnvalue;
}





/* function getAllSimplePositionsMysql()
 * MySQL implementation of getAllSimplePositions for a MySQL database that
 * does not include spatial support.
 * @param aDbConnection an exastir database connection struct describing
 * the connection.
 * Returns 0 for failure, 1 for success.  
 * If failure, stores error message in aDbConnection->errormessage.
 */
int getAllSimplePositionsMysql(Connection *aDbConnection) {
    int returnvalue = 0;
    DataRow *p_new_station;
    DataRow *p_time;
    int station_count = 0;  // number of new stations retrieved
    unsigned long x;  // xastir coordinate for longitude
    unsigned long y;  // xastir coordinate for latitide
    float lat;  // latitude converted from retrieved string
    float lon;  // longitude converted from retrieved string
    char *feedback[100];
    struct tm time;
    char sql[] = "select station, transmit_time, latitude, longitude, symbol, overlay, aprstype, origin, record_type, node_path from simpleStation order by station, transmit_time";
    MYSQL_RES *result;
    MYSQL_ROW *row;
    int ok;   // to hold mysql_query return value
    ok = mysql_query(&aDbConnection->mhandle,sql);
    if (ok==0) { 
        result = mysql_use_result(&aDbConnection->mhandle);
        if (result!=NULL) { 
            xastir_snprintf(feedback,100,"Retrieving MySQL records\n");
            stderr_and_statusline(feedback);
            // with mysql_use_result each call to mysql_fetch_row retrieves
            // a row of data from the server.  Mysql_store_result might use
            // too much memory in retrieving a large result set all at once.
            row = mysql_fetch_row(result);
            while (row != NULL) { 
               // retrieve data from the row
               // test to see if this is a valid station
               if (row[0]==NULL) { 
                  // station is null, skip
               } else { 
                  p_new_station = NULL;
                  if (search_station_name(&p_new_station,row[0],1)) {  
                       // This station is allready in present as a DataRow in the xastir db.
                       // Add data to the station's track.
                  } else { 
                       // This station isn't in the xastir db. 
                       // Add a datarow using the retrieved station record from the postgis database.
                       add_simple_station(p_new_station, row[0], row[7], row[4], row[5], row[6], row[2], row[3], row[8], row[9], row[1]);

                       station_count++;
                  }
               }
               // try to fetch the next row
               row = mysql_fetch_row(result);
            }
        } else { 
            // error fetching the result set
            fprintf(stderr,"mysql error: %s\n",mysql_error(&aDbConnection->mhandle));
            mysql_interpret_error(mysql_errno(&aDbConnection->mhandle),aDbConnection);
        }
        xastir_snprintf(feedback,100,"Retreived %d new stations from MySQL\n",station_count);
        stderr_and_statusline(feedback);
        mysql_free_result(result);
    } else { 
       // query didn't execute correctly
       mysql_interpret_error(ok,aDbConnection);
    }
    return returnvalue;
}


int getAllSimplePositionsMysqlInBoundingBox(Connection *aDbConnection, char *str_e_long, char *str_w_long, char *str_n_lat, char *str_s_lat) {
    int returnvalue = 0;

    return returnvalue;
}



/* function mysql_interpret_error() 
 * given a mysql error code and an xastir connection, sets an appropriate
 * error message in the errormessage field of the connection.  Interprets
 * numeric error codes returned by mysql functions.
 * @param errorcode A result returned by a mysql function that can be 
 * interpreted as an error code.  
 * @param aDbConnection an xastir database connection struct describing the
 * connection and its current state.
 * Note - it is possible to give this function a connection on which an
 * error has not occured along with an error code.  This function does
 * not check the connection or assess whether an error actually occured
 * on it or not, it simply interprets an error code and writes the 
 * interpretation into the connection that was passed to it.  
 */
void mysql_interpret_error(int errorcode, Connection *aDbConnection) { 
    fprintf(stderr,"Error communicating with MySQL database. Error code=%d\n",errorcode);
    switch (errorcode) {
       case CR_OUT_OF_MEMORY :
           // insufficient memory for query
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "MySQL: Out of Memory");
           // notify the connection status listener
           break;
       // mysql_query errors
       case CR_COMMANDS_OUT_OF_SYNC :
           // commands in improper order
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "MySQL: Commands out of sync");
           break;
       case CR_SERVER_GONE_ERROR :
           // mysql server has gone away
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "MySQL: Connection to server lost");
           // notify the connection status listener
           break;
       case CR_SERVER_LOST :
           // server connection was lost during query
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "MySQL: Connection to server lost during query");
           // notify the connection status listener
           break;
       case CR_UNKNOWN_ERROR :
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "MySQL: Unknown Error");
           break;
       default:
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "MySQL: Unrecognized error Code [%d]", errorcode);
    }
    fprintf(stderr,"%s\n",aDbConnection->errormessage);
}

#endif /* HAVE_MYSQL*/

// add code for a lightweight database here







#endif /* HAVE_DB*/

// Functions related to GIS, but not database specific ************************




/* Function  xastirCoordToLatLongPostgresPoint
 * converts a point in xastir coordinates to a native postgres representation 
 * of a point using latitude and longitude in decimal degrees in the WGS84 
 * projection EPSG:4326. Format is similar to WKT, but without leading POINT.
 * @param x longitude in xastir coordinates = decimal 100ths of a second.  
 * @param y latitude in xastir coordinates = decimal 100ths of a second.  
 * @param pointer to a char[ at least 24] string to hold point representation.
 * returns 1 on success, 0 on failure.
 */
int xastirCoordToLatLongPostgresPoint(long x, long y, char *wkt) { 
    // 1 xastir coordinate = 1/100 of a second
    // 100*60*60 xastir coordinates (=360000 xastir coordinates) = 1 degree
    // 360000   xastir coordinates = 1 degree
    // conversion to string decimal degrees handled by utility fuctions
    int returnvalue = 0;  // defaults to failure
    float latitude;
    float longitude;
    int ok;
    ok = convert_from_xastir_coordinates (&longitude,&latitude, x, y);
    if (ok>0) { 
       xastir_snprintf(wkt, MAX_WKT, "(%3.6f, %3.6f)", latitude, longitude);
       returnvalue = 1;
    } 
    return returnvalue;
}






/* Function  xastirCoordToLatLongWKT
 * converts a point in xastir coordinates to a well known text string (WKT)
 * representation of a point using latitude and longitude in decimal degrees
 * in the WGS84 projection EPSG:4326
 * @param x longitude in xastir coordinates = decimal 100ths of a second.  
 * @param y latitude in xastir coordinates = decimal 100ths of a second.  
 * @param pointer to a char[29] string to hold well known text representation.
 * returns 1 on success, 0 on failure.
 */
int xastirCoordToLatLongWKT(long x, long y, char *wkt) { 
    // 1 xastir coordinate = 1/100 of a second
    // 100*60*60 xastir coordinates (=360000 xastir coordinates) = 1 degree
    // 360000   xastir coordinates = 1 degree
    // conversion to string decimal degrees handled by utility fuctions
    int returnvalue = 0;  // defaults to failure
    float latitude;
    float longitude;
    int ok;
    ok = convert_from_xastir_coordinates (&longitude,&latitude, x, y);
    if (ok>0) { 
       xastir_snprintf(wkt, MAX_WKT, "POINT(%3.6f %3.6f)", longitude, latitude);
       returnvalue = 1;
    } 
    return returnvalue;
}





float xastirWKTPointToLongitude(char *wkt) { 
    float returnvalue = 0.0;
    char temp[MAX_WKT];
    char *space = NULL;
    int x;
    if (wkt[0]=='P' && wkt[1]=='O' && wkt[2]=='I' && wkt[3]=='N' && wkt[4]=='T' && wkt[5]=='(') { 
        // this is a point
        xastir_snprintf(temp, MAX_WKT, "%s", wkt);
        // truncate at the space
        space = strchr(temp,' ');
        if (space != NULL) { 
            *space = '\0';
        }
        // remove the leading "POINT("
        for (x=0;x<6;x++) {  
            temp[x]=' ';
        }
        returnvalue = atof(temp);
    }
    return returnvalue;
}





float xastirWKTPointToLatitude(char *wkt) { 
    float returnvalue = 0.0;
    char temp[MAX_WKT];
    char *paren = NULL;
    int x;
    if (wkt[0]=='P' && wkt[1]=='O' && wkt[2]=='I' && wkt[3]=='N' && wkt[4]=='T' && wkt[5]=='(') { 
        // this is a point
        xastir_snprintf(temp, MAX_WKT, "%s", wkt);
        // truncate at the trailing parenthesis
        paren = strchr(temp,')');
        if (paren != NULL) { 
            *paren = '\0';
        }
        // convert all leading characters up to the space to spaces.
        for (x=0;x<(int)(strlen(temp));x++)  {
            if (temp[x]==' ') { 
               x = (int)(strlen(temp));
           } else { 
               temp[x] = ' ';
           }
        }
        returnvalue = atof(temp);
    }
    return returnvalue;

}

