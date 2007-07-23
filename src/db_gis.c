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

// include postgresql library for postgis support
#ifdef HAVE_POSTGIS
#include <libpq-fe.h>
#endif // HAVE_POSTGIS
// mysql error library for mysql error code constants
#ifdef HAVE_MYSQL
#include <my_global.h>
#include <mysql.h>
#include <errmsg.h>
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
#ifdef HAVE_SPATIAL_DB
#ifdef HAVE_POSTGIS
int storeStationToGisDbPostgis(Connection *aDbConnection, DataRow *aStation);
int storeCadToGisDbPostgis(Connection *aDbConnection, CADRow *aCadObject); 
int storeStationSimplePointToGisDbPostgis(Connection *aDbConnection, DataRow *aStation); 
int testXastirVersionPostgis(Connection *aDbConnection);
int getAllSimplePositionsPostgis(Connection *aDbConnection);
int getAllSimplePositionsPostgisInBoundingBox(Connection *aDbConnection, char* str_e_long, char* str_w_long, char* str_n_lat, char* str_s_lat);
#endif /* HAVE_POSTGIS*/
#ifdef HAVE_MYSQL_SPATIAL
int storeStationToGisDbMysql(Connection *aDbConnection, DataRow *aStation); 
int storeCadToGisDbMysql(Connection *aDbConnection, CADRow *aCadObject); 
int storeStationSimplePointToGisDbMysql(Connection *aDbConnection, DataRow *aStation); 
int getAllSimplePositionsMysqlSpatial(Connection *aDbConnection);
int getAllCadFromGisDbMysql(Connection *aDbConnection); 
int getAllSimplePositionsMysqlSpatialInBoundingBox(Connection *aDbConnection, char* str_e_long, char* str_w_long, char* str_n_lat, char* str_s_lat);
#endif /* HAVE_MYSQL_SPATIAL */
#ifdef HAVE_MYSQL
int testXastirVersionMysql(Connection *aDbConnection);
int storeStationSimplePointToDbMysql(Connection *aDbConnection, DataRow *aStation); 
int getAllSimplePositionsMysql(Connection *aDbConnection);
int getAllSimplePositionsMysqlInBoundingBox(Connection *aDbConnection, char *str_e_long, char *str_w_long, char *str_n_lat, char *str_s_lat);
int storeStationToDbMysql(Connection *aDbConnection, DataRow *aStation);
void mysql_interpret_error(int errorcode, Connection *aDbConnection);
#endif /* HAVE_MYSQL*/
#endif /* HAVE_SPATIAL_DB */

// Layer 2a: Generic GIS db storage code. ************************************
// Wrapper functions for actual DBMS specific actions

#ifdef HAVE_SPATIAL_DB

// ******** Functions that require spatialy enabled database support *********





/* function storeStationToGisDb() 
 * stores the information about a station and its most recent position
 * to a spatial database.
 * @param aDbConnection generic database connection to the database in
 * which the station information is to be stored.
 * @param aStation the station to store.
 * @returns 0 on failure, 1 on success.  On failure, stores error message
 * in connection.
 */
int storeStationToGisDb(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
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
 */





/* Function openConnection()
 * Opens the specified database connection and returns a
 * generic handle to that connection.
 * On connection failure, returns null and sets error message in 
 * the connection descriptor.
 * @param aDbDescriptor a database connection description (host username etc).
 */
Connection openConnection(DbDescriptor *aDbDescriptor) {
    char connection_string[900];
    unsigned long client_flag = 0; // parameter used for mysql connection, is normally 0.
    Connection conn;
    #ifdef HAVE_POSTGIS
    PGconn *postgres_connection;
    #endif /* HAVE_POSTGIS */
    #ifdef HAVE_MYSQL
    MYSQL *mysql_connection;
    #endif /* HAVE_MYSQL */
  
    // initialize a new generic connection 
  
    // need some sort of connection listener to handle reconnection attempts when a connection fails...
    
    // try to open connection
    switch (aDbDescriptor->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS : 
            // If type is postgis, connect to postgis database.
            // build connection string from parameters
            xastir_snprintf(connection_string, sizeof(connection_string), \
               "host=%s, user=%s, password=%s, db=%s, port=%d", \
               aDbDescriptor->host, aDbDescriptor->username, aDbDescriptor->password, aDbDescriptor->schema, aDbDescriptor->port);
            // Use nonblocking connection
            postgres_connection = PQconnectStart(connection_string);
            if (postgres_connection != NULL) {  
                // can connect, run PQ_connect_poll loop
                // Note: xastir needs to decide when to time out
            }
            break;
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL : 
            mysql_connection = mysql_init(mysql_connection);
            // if type is mysql (=>4.1), connect to mysql database
            if (mysql_connection == NULL) { 
                // insufficient memory to initalize a new database handle 
                xastir_snprintf(aDbDescriptor->makeerrormessage, sizeof(aDbDescriptor->makeerrormessage), "Insufficient memory to open connection.");
            } else { 
                mysql_connection = mysql_real_connect(mysql_connection, aDbDescriptor->host, aDbDescriptor->username, aDbDescriptor->password, aDbDescriptor->schema, aDbDescriptor->port, aDbDescriptor->unix_socket, client_flag); 
                if (mysql_connection == NULL) { 
                    // unable to establish connection
                    xastir_snprintf(aDbDescriptor->makeerrormessage, sizeof(aDbDescriptor->makeerrormessage), "Unable to establish connection. %s", mysql_error(mysql_connection));
                } else { 
                    // connected to database
                    // make sure error message for making connection is empty.
                    xastir_snprintf(aDbDescriptor->makeerrormessage, sizeof(aDbDescriptor->makeerrormessage), " ");
                    // store connection information
                    conn.mhandle = mysql_connection;
                    conn.type = DB_MYSQL_SPATIAL;
                    conn.descriptor = *aDbDescriptor;
                    xastir_snprintf(conn.errormessage, sizeof(conn.errormessage), " ");
                }
            }
            break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL
        case DB_MYSQL : 
            mysql_connection = mysql_init(mysql_connection);
            // if type is mysql, connect to mysql database
            if (mysql_connection == NULL) { 
                // insufficient memory to initalize a new database handle 
                xastir_snprintf(aDbDescriptor->makeerrormessage, sizeof(aDbDescriptor->makeerrormessage), "Insufficient memory to open connection.");
            } else { 
                mysql_connection = mysql_real_connect(mysql_connection, aDbDescriptor->host, aDbDescriptor->username, aDbDescriptor->password, aDbDescriptor->schema, aDbDescriptor->port, aDbDescriptor->unix_socket, client_flag); 
                if (mysql_connection == NULL) { 
                    // unable to establish connection
                    xastir_snprintf(aDbDescriptor->makeerrormessage, sizeof(aDbDescriptor->makeerrormessage), "Unable to establish connection. %s", mysql_error(mysql_connection));
                } else { 
                    // connected to database
                    // make sure error message for making connection is empty.
                    xastir_snprintf(aDbDescriptor->makeerrormessage, sizeof(aDbDescriptor->makeerrormessage), " ");
                    // store connection information
                    conn.mhandle = mysql_connection;
                    conn.type = DB_MYSQL;
                    conn.descriptor = *aDbDescriptor;
                    xastir_snprintf(conn.errormessage, sizeof(conn.errormessage), " ");
                }
            }
            break;
        #endif /* HAVE_MYSQL*/
    }
    // return generic connection handle
    return conn;
}





/* Function closeConnection()
 * Closes the specified database connection.
 * @param aDbConnection a generic database connection handle.
 */
void closeConnection(Connection *aDbConnection) {
    //DbDescriptor db =  aDbConnection->descriptor;
    // free up connection resources
    switch (aDbConnection->type) {
        #ifdef HAVE_POSTGIS
        case DB_POSTGIS : 
            // if type is postgis, close connection to postgis database
            if (aDbConnection->phandle!=NULL) { 
                PQfinish(aDbConnection->phandle);
                aDbConnection->phandle = NULL;
            }
            break;
        #endif /* HAVE_POSTGIS */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL_SPATIAL : 
             // if type is mysql, close connection to mysql database
            if (aDbConnection->mhandle!=NULL) { 
                mysql_close(aDbConnection->mhandle);
            }
            break;
        #endif /* HAVE_MYSQL_SPATIAL */
        #ifdef HAVE_MYSQL_SPATIAL
        case DB_MYSQL : 
            // if type is mysql, close connection to mysql database
            if (aDbConnection->mhandle!=NULL) { 
                mysql_close(aDbConnection->mhandle);
            }
            break;
        #endif /* HAVE_MYSQL*/
    }
    // clean up the aDbConnection object
    aDbConnection=NULL;
    // remove the connection from the list of open connections
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
    // check type of schema to use (XASTIR full or APRSWorld) 
    switch (aDbConnection->descriptor.schema_type) {
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
    int returnvalue = 0;
    PGconn *conn = aDbConnection->phandle;
    // prepare statement
    char sql[100] = "insert into simpleStation (call, time, position) values ('%1','%2','%3')";
    // get call, position, and time
    // send query
    return returnvalue;
}





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
    int returnvalue = 0;
    char sql[100] = "select call, time, position from simpleStation";
    PGconn *conn = aDbConnection->phandle;

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
    char sql[100] = "select call, time, position from simpleStation where ";  
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
    MYSQL *conn = aDbConnection->mhandle;
   
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
    MYSQL *conn = aDbConnection->mhandle;
   
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
    int mysqlreturn;
    int test;
    char wkt[100];
    char timestring[100+1];
    MYSQL *conn = aDbConnection->mhandle;
    // prepare statement
    MYSQL_BIND bind[3];
    #define SQL "insert into simpleStation (call, time, position) values ('%1','%2',PointFromText('%3'))"
    MYSQL_STMT *statement = mysql_stmt_prepare(conn, SQL, strlen(SQL));
    if (!statement) { 
        mysql_interpret_error(*mysql_error(conn),aDbConnection);
    } else { 
       // get call, time, and position
       // call is required
       if (aStation->call_sign!=NULL && strlen(aStation->call_sign)>0) { 
           // get call, time, and position
           bind[0].buffer =  &aStation->call_sign;
           bind[0].length = strlen(aStation->call_sign);
           bind[0].buffer_length = MAX_CALLSIGN;
           bind[0].buffer_type = MYSQL_TYPE_STRING;
           bind[0].is_null = -1;
           // *** wrap setting of bind values into a function
           (void)strftime(timestring,100,"%a %b %d %H:%M:%S %Z %Y",localtime(&aStation->sec_heard));
           bind[1].buffer = &timestring;
           //bind[1]->length
           //bind[1]->buffer_length
           //bind[1]->buffer_type
           //bind[1]->isnull
           xastirCoordToLatLongWKT(aStation->coord_lon, aStation->coord_lat, wkt);
           bind[2].buffer = wkt;
           //bind[2]->length
           //bind[2]->buffer_length
           //bind[2]->buffer_type
           //bind[2]->isnull
           test = mysql_stmt_bind_param(statement, bind);
           if (test==0) { 
               mysql_interpret_error(test,aDbConnection);
           } else { 
               // send query
               mysqlreturn = mysql_stmt_execute(statement);
               if (mysqlreturn!=0) { 
                   returnvalue=0;
                   mysql_interpret_error(mysqlreturn,aDbConnection);
               } else {
                   returnvalue=1;
               }
           }
       } else { 
           // set call not null error message
           xastir_snprintf(aDbConnection->errormessage, sizeof(aDbConnection->errormessage), "Station callsign is required and was blank or null.");
       }
       mysql_stmt_close(statement);
    }
    // send query
  
  
    return returnvalue;
}






int getAllSimplePositionsMysqlSpatial(Connection *aDbConnection) {
    int returnvalue = 0;
    int mysqlreturn;
    char sql[] = "select call, time, AsText(position) from simpleStation";
    MYSQL *conn = aDbConnection->mhandle;
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
     station varchar(MAX_STATION) not null,  # callsign of station, length up to max_callsign
     symbol varchar(1),     # aprs symbol character
     overlay varchar(1),    # aprs overlay table character
     aprstype varchar(1)    # aprs type, required???
     time date not null default now(),  # transmission time, if available, otherwise storage time
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





/* function storeStationSimplePointToDbMysql()
 * MySQL implementation of wrapper storeStationSimplePointToGisDb().
 * Should be private to db_gis.c
 * Should only be called through wrapper function.  Do not call directly.
 * Returns 0 for failure, 1 for success.  
 * If failure, stores error message in aDbConnection->errormessage.
 */
int storeStationSimplePointToDbMysql(Connection *aDbConnection, DataRow *aStation) { 
    int returnvalue = 0;
    int mysqlreturn;
    char sql[200];
    char str_long[11];
    char str_lat[10];
    char timestring[100+1];
    MYSQL *conn = aDbConnection->mhandle;
    // prepared statements not present below MySQL version 4.1
    // char [] sql = "insert into simpleStation (call, time, latitude, longitude) values ('%1','%2','%3','%4'))";
    // call is required
    if (aStation->call_sign!=NULL && strlen(aStation->call_sign)>0) {
        // get time in seconds, adjust to datetime
        (void)strftime(timestring,100,"%a %b %d %H:%M:%S %Z %Y",localtime(&aStation->sec_heard));
        // get coord_lat, coord_long in xastir coordinates and convert to decimal degrees
        convert_lon_l2s(aStation->coord_lon, str_long, sizeof(str_long), CONVERT_DEC_DEG);
        convert_lat_l2s(aStation->coord_lat, str_lat, sizeof(str_lat), CONVERT_DEC_DEG);
        // build insert query with call, time, and position
        xastir_snprintf(sql,sizeof(sql),"insert into simpleStation (call, time, latitude, longitude) values ('%s','%s','%s','%s'))", aStation->call_sign,timestring,str_lat,str_long);
        // send query
        mysqlreturn = mysql_query(conn, sql);
        if (mysqlreturn!=0) { 
            returnvalue=0;
            mysql_interpret_error(mysqlreturn,aDbConnection);
        } else {
            returnvalue=1;
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
 */
int testXastirVersionMysql(Connection *aDbConnection) {
    int returnvalue = 0;
    int mysqlreturn;
    char sql[] = "select version_number from version order by version_number desc limit 1";  
    MYSQL *conn = aDbConnection->mhandle;
    return returnvalue;
}





/* function storeStationToDbMysql() 
 */
int storeStationToDbMysql(Connection *aDbConnection, DataRow *aStation){
    int returnvalue = 0;
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
    char sql[] = "select call, time, latitude, longitude from simpleStation";
    MYSQL *conn = aDbConnection->mhandle;

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
}

#endif /* HAVE_MYSQL*/

// add code for a lightweight database here







#endif /* HAVE_DB*/

// Functions related to GIS, but not database specific ************************





/* Function  xastirCoordToLatLongWKT
 * converts a point in xastir coordinates to a well known text string (WKT)
 * representation of a point using latitude and longitude in decimal degrees
 * in the WGS84 projection EPSG:4326
 * @param x longitude in xastir coordinates = decimal 100ths of a second.  
 * @param y latitude in xastir coordinates = decimal 100ths of a second.  
 * @param pointer to a char[29] string to hold well known text representation.
 */
char *xastirCoordToLatLongWKT(float x, float y, char *wkt) { 
    // 1 xastir coordinate = 1/100 of a second
    // 100*60*60 xastir coordinates (=360000 xastir coordinates) = 1 degree
    // 360000   xastir coordinates = 1 degree
    // conversion to string decimal degrees handled by utility fuctions
    char str_long[11];
    char str_lat[10];
    convert_lon_l2s(x, str_long, sizeof(str_long), CONVERT_DEC_DEG);
    convert_lat_l2s(y, str_lat, sizeof(str_lat), CONVERT_DEC_DEG);
    // convert strings to floating point numbers and truncate
    xastir_snprintf(wkt, sizeof(wkt), "POINT(%3.6f %3.6f)", strtof(str_lat,NULL), strtof(str_long,NULL));
    return wkt;
}


