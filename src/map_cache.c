/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2005  The Xastir Group
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */


/*
 *   The code in this file is used to cache maps downloaded
 *   from tiger.census.gov, and to manage that cache. It was written
 *   to use Berkeley DB version 4 or better. 
 *
 *   Dan Brown N8YSZ. 
 *
 */


// Need this one before the #ifdef in order to get the definition of
// USE_MAP_CACHE, if defined.
#include "config.h"

#ifdef  USE_MAP_CACHE
//#warning USE_MAP_CACHE Defined

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "snprintf.h" 
#include "xastir.h"
#include "xa_config.h" 
#include "main.h"
#include "maps.h" 
#include "map_cache.h" 
#include <db.h>



int map_cache_put( char * map_cache_url, char * map_cache_file ){

// Puts an entry into the url->filename database
// Tracks space used in "CACHE_SPACE_USED" 

    char mc_database_filename[MAX_FILENAME]; 
    int mc_ret, mc_t_ret, mc_file_stat, mc_space_used ;
    DBT mc_key, mc_data ; 
    DB *dbp; 
    struct stat file_status;
    char mc_buf[128];


    mc_space_used=0; 

    xastir_snprintf(mc_database_filename,
    sizeof(mc_database_filename),
    "%s/map_cache.db", 
    get_user_base_dir("map_cache"));

    // check for reasonable filename
    // expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif
    //                                   1234567890123456789012345678901234567

    mc_ret=strlen(map_cache_file);
    
    if ( mc_ret < 37 ) { 
        if (debug_level & 512 ) {
            fprintf(stderr,
            	"map_cache_put: Unusable filename: %s. Skipping encaching\n",
            	map_cache_file);
        }
        return (-1 * mc_ret); 
    } 
            
// stat the file to see if we even need to bother 

    if ((mc_file_stat=stat(map_cache_file, &file_status)) !=0) {
        if (debug_level & 512 ) {
            fprintf(stderr,
            	"map_cache_put: File Error: %s. Skipping encaching\n",
            	map_cache_file);
        } 
       
        return (mc_file_stat); 
    }

    if ( debug_level & 512) {
        printf ("map_cache_put: file_status.st_size %d\n",
                        (int) file_status.st_size);        
    }

// Create handle to db

    if ((mc_ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr, "map_cache_put db_create:%s\n", db_strerror(mc_ret)); 
        return(1);
    }

// open the db 

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4 

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

    if ((mc_ret = dbp->open(dbp,
            mc_database_filename , NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {

        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret); 
    }
#elif	 (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=1 )

    if ((mc_ret = dbp->open(dbp,
            NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {

        dbp->err(dbp, mc_ret, "%s", mc_database_filename);
        db_strerror(mc_ret); 
    }

#endif  /** DB_VERSION Check **/

// Before we put something in we need to see if we got space

// if mc_cache_size_limit

// Setup for get 

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));

    mc_key.data = "CACHE_SPACE_USED";
    mc_key.size = sizeof("CACHE_SPACE_USED"); 

// check "CACHE_SPACE_USED" record in db
	
    if (((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0) &&  ( mc_data.data != NULL ) ) {
        ( debug_level & 512 ) && printf("map_cache_put: %s: key retrieved: data was %s.\n",
                            (char *)mc_key.data,
                            (char *)mc_data.data);

// this pukes if mc_data.data is null

        mc_space_used=atoi( (char *)mc_data.data);

        if ( debug_level & 512 ) {
            fprintf (stderr, "map_cache_put: CACHE_SPACE_USED = %.2f mb\n",
                            (mc_space_used/1024/1024.0));
        }

    } else {

        if (mc_data.data == NULL) {
            if ( debug_level & 512 ) {
                printf ("map_cache_put: CACHE_SPACE_USED get returned null \n"); 
            }
        } else { 
            if ( debug_level & 512 ) {
                printf ("map_cache_put: Unable to check CACHE_SPACE_USED: %s\n",
                            db_strerror(mc_ret)); 
            }
	}

        // for now let us assume this is the first map entry and  we
        // just flag an error. Better procedure for later might be to
        // return(mc_ret) indicating a database error of some sort
         
    }
      
//      xastir_snprintf(map_cache_file, MAX_FILENAME, "%s",(char *)mc_data.data);

    if ( debug_level & 512 ) {
            fprintf (stderr, "map_cache_put: mc_space_used before = %d bytes file_status.st_size %d\n",
                        mc_space_used,
                        (int) file_status.st_size);
    }

    mc_space_used += (int) file_status.st_size;

    if ( debug_level & 512 ) {
            fprintf (stderr, "map_cache_put: mc_space_used after = %d bytes \n",
                        mc_space_used);
    }

    if ( mc_space_used > MAP_CACHE_SPACE_LIMIT) { 
        if ( debug_level & 512 ) {
            fprintf (stderr, "map_cache_put: Warning cache space used: %.2f mb NOW OVER LIMIT: %.2f mb\n",
                            (mc_space_used/1024/1024.0),
                            (MAP_CACHE_SPACE_LIMIT/1024/1024.0));
        }

// Cache Cleanup
// The warning is nice, but we should do something here
// Needs LRU and or FIFO db structures

    } else {

// else put cache_space_used

// setup 

        memset(&mc_key, 0, sizeof(mc_key));
        memset(&mc_data, 0, sizeof(mc_data));

// data

        mc_key.data = "CACHE_SPACE_USED";
        mc_key.size = sizeof("CACHE_SPACE_USED"); 

        xastir_snprintf(mc_buf, sizeof(mc_buf), "%d", mc_space_used);

        if ( debug_level & 512 ) { 
                fprintf (stderr, "map_cache_put: mc_buf: %s len %d\n",
                        mc_buf,(int) sizeof(mc_buf));
        }

        mc_data.data = mc_buf ; 
        mc_data.size = sizeof(mc_buf);

// put 

        if ((mc_ret = dbp->put(dbp, NULL, &mc_key, &mc_data, 0)) == 0) {

            if ( debug_level & 512 ) {
                fprintf(stderr, "map_cache_put: %s: key stored.\n", (char *)mc_key.data);
            }

        } else {

            if ( debug_level & 512 ) {
                 dbp->err(dbp, mc_ret, "DB->put");
            }
//            db_strerror(mc_ret); 

            return(mc_ret);

        } 

    } 

// Setup for put of data

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));

// Real data at last 

    mc_key.data = map_cache_url;
    mc_key.size = strlen(map_cache_url); 
    mc_data.data = map_cache_file;
    mc_data.size = strlen(map_cache_file)+1; /* +1 includes  \0 */ 

// do the actual put 

    if ((mc_ret = dbp->put(dbp, NULL, &mc_key, &mc_data, 0)) == 0) {
        if ( debug_level & 512 ) {
            fprintf(stderr, "map_cache_put: %s: key stored.\n", (char *)mc_key.data);
        }
    } else {
        if ( debug_level & 512 ) {
            dbp->err(dbp, mc_ret, "DB->put") ;
        }
        // db_strerror(mc_ret); 
        return(mc_ret); 
    } 
    statusline("Map now cached",1);
// close the db 

    if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0) {
        mc_ret = mc_t_ret;
        db_strerror(mc_ret); 
    }

/* end map_cache_put */

    return (0) ; 
}





int map_cache_get( char * map_cache_url, char * map_cache_file ){

// Queries URL->Filename db 
// Calls map_cache_del to cleanup expired maps 

    DBT mc_key, mc_data ; 
    DB *dbp;
    int mc_ret, mc_t_ret, mc_file_stat ;
    char mc_database_filename[MAX_FILENAME]; 
    struct stat file_status;


    xastir_snprintf(mc_database_filename,
        sizeof(mc_database_filename),   // change to max_filename?
        "%s/map_cache.db",
        get_user_base_dir("map_cache"));
    
    if ((mc_ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr, "map_cache_get db_create:%s\n", db_strerror(mc_ret)); 
        return(1);
    }

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4 

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

    if ((mc_ret = dbp->open(dbp,
        mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        ( debug_level & 512 ) ?  dbp->err(dbp, mc_ret, "%s", mc_database_filename):0;
        // db_strerror(mc_ret);
    }

#elif	 (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=1 )
	
    if ((mc_ret = dbp->open(dbp,
        NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        ( debug_level & 512 ) ? dbp->err(dbp, mc_ret, "%s", mc_database_filename) : 0 ;
        // db_strerror(mc_ret);
    }

#endif  /** DB_VERSION Check **/

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));

    mc_key.data=map_cache_url ; 
    mc_key.size=strlen(map_cache_url); 

    statusline("Checking Map Cache",1);
    if (debug_level & 512 ) {
        fprintf(stderr, "map_cache_get: Checking Map Cache\n");
    }

	
    if ((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0) {
        if ( debug_level & 512 ) {
             fprintf(stderr, "map_cache_get: %s: key retrieved: data was %s.\n",
                            (char *)mc_key.data, (char *)mc_data.data);
        }
        xastir_snprintf(map_cache_file, MAX_FILENAME, "%s",(char *)mc_data.data);

    // check for reasonable filename
    // expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif
    //                                   1234567890123456789012345678901234567

	mc_ret=strlen(map_cache_file);
	
        if ( mc_ret < 37 ) { 
 
            if (debug_level & 512 ) {
                fprintf(stderr,
                    "map_cache_get: Unusable filename: %s. Deleting key %s from cache\n",
                    map_cache_file,map_cache_url);
            }
            map_cache_del(map_cache_url);
  
            return (-1 * mc_ret); 
        } 
            

    // check age of file - based on name - delete if too old
     
        if (debug_level & 512 ) {
            fprintf(stderr, "map_cache_get: Checking age\n");
        }
   
        if ( (mc_ret=map_cache_expired(map_cache_file, (MC_MAX_FILE_AGE)))){
            if ( debug_level & 512 ) {
                fprintf(stderr, "map_cache_get: deleting expired key: %s.\n",
                               (char *)mc_key.data); 
            }
            map_cache_del(map_cache_url);
            return (mc_ret);
        } 

    // check if the file exists 
        if ( debug_level & 512 ) {
                fprintf(stderr,"map_cache_get: checking for existance of map_cache_file:  %s.\n",
                            map_cache_file);
        }

        mc_file_stat=stat(map_cache_file, &file_status);

        if ( debug_level & 512 ) {
                fprintf(stderr,"map_cache_get: map_cache_file %s stat returned:%d.\n",
                            map_cache_file,
                            mc_file_stat);
        }

        if ( mc_file_stat == -1 ) {
	// 		
            if ( debug_level & 512 ) {
                fprintf(stderr,"map_cache_get: attempting delete map_cache_file %s \n",
                                map_cache_file);
            }
            if ((mc_ret = dbp->del(dbp, NULL, &mc_key, 0)) == 0) {
                if ( debug_level & 512 ) {
                    fprintf(stderr, "map_cache_get: File stat failed %s: key was deleted.\n",
                                    (char *)mc_key.data);
                }
            }
            else {
                if ( debug_level & 512 ) {
                    dbp->err(dbp, mc_ret, "DB->del");
                }
                // db_strerror(mc_ret);
            }		
            if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0){
                mc_ret = mc_t_ret;
               // db_strerror(mc_ret);
            }
  
            // db_strerror(mc_ret);
            // Return the file stat if there was a file problem
            return (mc_file_stat);
        } else {
            if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0){
                mc_ret = mc_t_ret;
                // db_strerror(mc_ret);
            }
            // If we made it here all is good
            statusline("Loading Cached Map",1);
            return (0); 
        }
		
    } else {
        if ( debug_level & 512 ) {
            fprintf(stderr, "map_cache_get: Get failed. Key was: %s.\n",
                (char *)mc_key.data);
        }

        ( debug_level & 512 ) ? dbp->err(dbp, mc_ret, "DB->get"):0;
        // db_strerror(mc_ret);

        // there was some problem getting things from the db
        // return the return from the get 

        statusline("Map not found in cache...",1);        
        return (mc_ret); 
    }
/** end map_cache_get **/  
}





int map_cache_del( char * map_cache_url ){

    // Delete entry from the cache 
    // and unlink associated file from disk

    DBT mc_key, mc_data, mc_size_key, mc_size_data ; 
    DB *dbp;
    int mc_ret, mc_t_ret, mc_file_stat, mc_space_used;
    char mc_database_filename[MAX_FILENAME]; 
    char mc_delete_file[MAX_FILENAME]; 
    struct stat file_status;

    char mc_buf[128];
    
    mc_space_used = 0 ; 

    xastir_snprintf(mc_database_filename,
        MAX_FILENAME,
        "%s/map_cache.db",
        get_user_base_dir("map_cache"));

    if ((mc_ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr, "map_cache_del db_create:%s\n", db_strerror(mc_ret)); 
        return(1);
    }

#if  (DB_VERSION_MAJOR<4)   /** DB_VERSION Check **/
#error DB_VERSION_MAJOR < 4 

#elif (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<=0 )

    if ((mc_ret = dbp->open(dbp,
        mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        ( debug_level & 512 ) ?  dbp->err(dbp, mc_ret, "%s", mc_database_filename):0;
        // db_strerror(mc_ret);
    }
#elif	 (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=1 )
	
    if ((mc_ret = dbp->open(dbp,
        NULL,mc_database_filename, NULL, DB_CREATE, DB_BTREE, 0664)) != 0) {
        ( debug_level & 512 ) ? dbp->err(dbp, mc_ret, "%s", mc_database_filename):0;
        // db_strerror(mc_ret);
    }

#endif  /** DB_VERSION Check **/

    memset(&mc_key, 0, sizeof(mc_key));
    memset(&mc_data, 0, sizeof(mc_data));
	
    mc_key.data=map_cache_url ; 
    mc_key.size=strlen(map_cache_url); 

    // Try to get the key from the cache
	
    if ((mc_ret = dbp->get(dbp, NULL, &mc_key, &mc_data, 0)) == 0) {
        if ( debug_level & 512 ) {
            fprintf(stderr, "map_cache_del: %s: key retrieved: data was %s.\n",
                            (char *)mc_key.data,
                            (char *)mc_data.data);
        }

    // stat the file 

    xastir_snprintf(mc_delete_file,
        MAX_FILENAME,
        "%s",
        (char *) mc_data.data);

        mc_file_stat=stat(mc_delete_file, &file_status);
        
        if ( debug_level & 512 ) {
            fprintf(stderr,"map_cache_del: file %s stat returned:%d.\n",
                            (char *) mc_data.data,
                            mc_file_stat);
        }
        if (mc_file_stat == 0 ){


// Setup for get CACHE_SPACE_USED

            memset(&mc_size_key, 0, sizeof(mc_size_key));
            memset(&mc_size_data, 0, sizeof(mc_size_data));

            mc_size_key.data = "CACHE_SPACE_USED";
            mc_size_key.size = sizeof("CACHE_SPACE_USED"); 

// check "CACHE_SPACE_USED" record in db
	
            if (((mc_ret = dbp->get(dbp, NULL, &mc_size_key, &mc_size_data, 0)) == 0) &&  ( mc_size_data.data != NULL ) ) {
                    if ( debug_level & 512 ) {
                        fprintf(stderr, "map_cache_del: %s: key retrieved: data was %s.\n",
                                (char *)mc_size_key.data,
                                (char *)mc_size_data.data);
                    }

// this pukes if mc_size_data.data is null

                mc_space_used=atoi( (char *)mc_size_data.data);

                if ( debug_level & 512 ) {
                    fprintf (stderr, "map_cache_del: CACHE_SPACE_USED = %.2f mb\n",
                                (mc_space_used/1024/1024.0));
                }
            } else {

                if (mc_size_data.data == NULL) {
                    if ( debug_level & 512 ) {
                        fprintf (stderr, "map_cache_del: CACHE_SPACE_USED get returned null \n"); 
                    }
                } else { 
                    if ( debug_level & 512 ) {
                        fprintf (stderr, "map_cache_del: Unable to check CACHE_SPACE_USED: %s\n",
                            db_strerror(mc_ret)); 
                    }
                }
            }

            if ( debug_level & 512 ) {
                fprintf (stderr, "map_cache_del: mc_space_used before = %d bytes file_status.st_size %d\n",
                                mc_space_used,
                                (int) file_status.st_size);
            }

             mc_ret = unlink( mc_delete_file );
        
            if ( debug_level & 512 ) {
                fprintf(stderr,"map_cache_del: file %s unlink returned:%d.\n",
                            mc_delete_file,
                            mc_ret);
            }

            if (mc_ret == 0 ){

// Update cache_space_used

// setup 

                mc_space_used -= (int) file_status.st_size;

                if (mc_space_used < 0) {
                    mc_space_used = 0;
                }

                if ( debug_level & 512 ) {
                    fprintf (stderr, "map_cache_del: unlink succeeded mc_space_used = %d bytes \n",
                            mc_space_used);        
                }
                memset(&mc_size_key, 0, sizeof(mc_size_key));
                memset(&mc_size_data, 0, sizeof(mc_size_data));

// data
                mc_size_key.data = "CACHE_SPACE_USED";
                mc_size_key.size = sizeof("CACHE_SPACE_USED"); 

                xastir_snprintf(mc_buf, sizeof(mc_buf), "%d", mc_space_used);

                if ( debug_level & 512 ) {
                    fprintf (stderr, "map_cache_del: mc_buf: %s len %d\n",
                                mc_buf, (int)sizeof(mc_buf));
                }

                mc_size_data.data = mc_buf ; 
                mc_size_data.size = sizeof(mc_buf);

// put 
                
                if ((mc_ret = dbp->put(dbp, NULL, &mc_size_key, &mc_size_data, 0)) == 0) {
                    if ( debug_level & 512 ) {
                        printf("map_cache_del: %s: key stored.\n",
                                    (char *)mc_size_key.data);
                    }
                } else {
                    if ( debug_level & 512 ) {
                        dbp->err(dbp, mc_ret, "DB->put");
                    }

                    // db_strerror(mc_ret); 
                } 

    } else  {

        if ( debug_level & 512 ) { 
            fprintf (stderr, "map_cache_del: unlink failed mc_space_used = %d bytes \n",
                       mc_space_used);
        }        
        return(mc_ret) ; 
    }

        } else {

            // file stat was not good - do something

        }

	
    // remove entry from cache url->filename database 
	
        if ((mc_ret = dbp->del(dbp, NULL, &mc_key, 0)) == 0){
            if ( debug_level & 512 ) {
                fprintf(stderr, "map_cache_del: %s: key was deleted.\n",
                                (char *)mc_key.data);
            }
        } 
        else {
            if ( debug_level & 512 ) {
                dbp->err(dbp, mc_ret, "DB->del");
            }
            // db_strerror(mc_ret);
        }							

    // close  the db. 
	
        if ((mc_t_ret = dbp->close(dbp, 0)) != 0 && mc_ret == 0){
            mc_ret = mc_t_ret;
            // db_strerror(mc_ret);
        }

    return (0); 
		
    } else {
        if ( debug_level & 512 ) {
            dbp->err(dbp, mc_ret, "DB->del");
        }
        // db_strerror(mc_ret);
        return (mc_ret); 
    }

/** end map_cache_del **/  
}





char * map_cache_fileid(){

    // returns a unique identifier 
    // used for generating filenames for cached files

    time_t t; 
    char * mc_time_buf;


    mc_time_buf = malloc (16); 
    sprintf( mc_time_buf, "%d",(int) time(&t)); 
    return (mc_time_buf);
}





int map_cache_expired( char * mc_filename, time_t mc_max_age ){

    // check for files old enough to expire 
    // this is lame but it should work for now. 
    // expects file name like /home/brown/.xastir/map_cache/map_1100052372.gif

    // writing this proved a good example of why pointer arithmetic is tricky
    // and a good example of why I should avoid it - n8ysz 20041110
    
    time_t mc_t,mc_file_age; 
    char *mc_filename_tmp, *mc_time_buf_tmp, *mc_time_buf;
    
    
    mc_time_buf=malloc(MAX_FILENAME);
    mc_time_buf_tmp=mc_time_buf; 

    // grab filename
    mc_filename_tmp=strrchr(mc_filename,'/'); 

    // clean up map_
    mc_filename_tmp=strchr(mc_filename_tmp,'_'); 
    ++mc_filename_tmp; 
    
    // save up to .gif 
    while ((*mc_time_buf_tmp++ = *mc_filename_tmp++) != '.' )
        ;
 
   *(--mc_time_buf_tmp) ='\0';

//    if ( debug_level & 512 ) {
//          fprintf(stderr, "map_cache_expired: mc_filename is: %s mc_time_buf is: %s.\n", mc_filename, mc_time_buf);
//      }

    mc_file_age=(time(&mc_t) - ((time_t) atoi(mc_time_buf)) ); 
    
    if ( mc_file_age <  mc_max_age ) {
        if ( debug_level & 512 ) {
            fprintf(stderr, "map_cache_expired: mc_filename %s is NOT expired. mc_time_buf is: %s.\n",
                            mc_filename,
                            mc_time_buf);
        }
        free(mc_time_buf); 
        return (0); 
    } else {
        if ( debug_level & 512 ) {
            fprintf(stderr, "map_cache_expired: mc_filename %s IS expired. mc_time_buf is: %s.\n",
                            mc_filename,
                            mc_time_buf);
        }
        free(mc_time_buf); 
        return ((int)mc_file_age); 
    }


   // sprintf( mc_time_buf, "%d",(int)

    return (0);
}



// Functions that need writing 

int mc_check_space_used () {
    
   return(0); 

}

int mc_update_space_used () {
    
   return(0); 

}


#endif // USE_MAP_CACHE
