#include "config.h"

#ifdef USE_RTREE
#include <stdlib.h>
#include <stdio.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#ifdef HAVE_SHAPEFIL_H
#include <shapefil.h>
#else
#ifdef HAVE_LIBSHP_SHAPEFIL_H
#include <libshp/shapefil.h>
#else
#error HAVE_LIBSHP defined but no corresponding include defined
#endif  // HAVE_LIBSHP_SHAPEFIL_H
#endif  // HAVE_SHAPEFIL_H


#include <rtree/index.h>

#include "xastir.h"
#include "util.h"
#include "hashtable.h"
#include "hashtable_itr.h"
#include "shp_hash.h"

#define PURGE_PERIOD 3600     // One hour, hard coded for now.
                              // This should be in a slider in the timing
                              // configuration instead.


static struct hashtable *shp_hash=NULL;
static time_t purge_time;

#define SHP_HASH_SIZE 65535
#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }

unsigned int shape_hash_from_key(void *key) {
    char *str=(char *)key;
    unsigned int shphash=5381;
    int c;
    int i=0;
    while (str[i]!='\0') {
        c=str[i++];
        shphash = ((shphash << 5) + shphash)^c;
    }

    return (shphash);
}

int shape_keys_equal(void *key1, void *key2) {

    //    fprintf(stderr,"Comparing %s to %s\n",(char *)key1,(char *)key2);
    if (strncmp((char *)key1,(char *)key2,strlen((char *)key1))==0) {
        //        fprintf(stderr,"    match\n");
        return(1);
    } else {
        //        fprintf(stderr,"  no  match\n");
        return(0);
    }
}

void init_shp_hash(int clobber) {
    // make sure we don't leak
    if (shp_hash) {
        if (clobber) {
            hashtable_destroy(shp_hash, 1);
            shp_hash=create_hashtable(SHP_HASH_SIZE,
                                      shape_hash_from_key,
                                      shape_keys_equal);
        }
    } else {
        shp_hash=create_hashtable(SHP_HASH_SIZE,
                                  shape_hash_from_key,
                                  shape_keys_equal);
    }

    // Now set the static timer value to the next time we need to run the purge
    // routine
    purge_time = sec_now() + PURGE_PERIOD;
}

// destructor for a single shapeinfo structure
void destroy_shpinfo(shpinfo *si) {
    if (si) {
        empty_shpinfo(si);
        //        fprintf(stderr,
        //                "       Freeing shpinfo %lx\n",
        //        (unsigned long int) si);
        free(si);
    }
}

// free the pointers in a shapinfo object
void empty_shpinfo(shpinfo *si) {
    if (si) {
        if (si->root) {
            //            fprintf(stderr,"        Freeing root\n");
            RTreeDestroyNode(si->root);
            si->root=NULL;
        }

        // This is a little annoying --- the hashtable functions free the
        // key, which is in our case the filename.  So since we're only going
        // to empty the shpinfo when we're removing from the hashtable, we
        // must not free the filename ourselves.
        //        if (si->filename) {
        //            fprintf(stderr,"        Freeing filename\n");
        //            free(si->filename);
        //            si->filename=NULL;
        //        }
    }
}
    
void destroy_shp_hash() {
    struct hashtable_itr *iterator=NULL;
    shpinfo *si;
    if (shp_hash) {
        // walk through the hashtable, free any pointers in the values
        // that aren't null, or we'll leak like a sieve.
        iterator=hashtable_iterator(shp_hash);
        do {
            si = hashtable_iterator_value(iterator);
            empty_shpinfo(si);
        } while (hashtable_iterator_advance(iterator));
        hashtable_destroy(shp_hash, 1);  // destroy the hashtable, freeing
                                         // what's left of the entries
        shp_hash=NULL;
    }
}

void add_shp_to_hash(char *filename, SHPHandle sHP) {

    // This function does NOT check whether there already is something in 
    // the hashtable that matches.
    // Check that before calling this routine.

    shpinfo *temp;
    int filenm_len;
    filenm_len=strlen(filename);
    if (!shp_hash) {  // no table to add to
        init_shp_hash(1); // so create one
    }
    temp = (shpinfo *)malloc(sizeof(shpinfo));
    CHECKMALLOC(temp);
    // leave room for terminator
    temp->filename = (char *) malloc(sizeof(char)*(filenm_len+1));
    CHECKMALLOC(temp->filename);
    strncpy(temp->filename,filename,filenm_len+1);
    temp->filename[filenm_len]='\0';  // just to be safe
    temp->root = RTreeNewIndex();  
    temp->creation = sec_now();
    temp->last_access = temp->creation;

    build_rtree(&(temp->root),sHP);

    //    fprintf(stderr, "  adding %s...",temp->filename);
    if (!hashtable_insert(shp_hash,temp->filename,temp)) {
        fprintf(stderr,"Insert failed on shapefile hash --- fatal\n");
        exit(1);
    }
}

shpinfo *get_shp_from_hash(char *filename) {
    shpinfo *result;
    if (!shp_hash) {  // no table to search
        init_shp_hash(1); // so create one
        return NULL;
    }
    //    fprintf(stderr,"   searching for %s...",filename);
    result=hashtable_search(shp_hash,filename);
    //    if (result) {
    //        fprintf(stderr,"      found it\n");
    //    } else {
    //        fprintf(stderr,"      it is not there\n");
    //    }

    // If there is one, we have now accessed it, so bump the last access time
    if (result) {
        result->last_access = sec_now();
    }

    return (result);

}
//CAREFUL:  note how adding things to the tree can change the root
// Must not ever use cache a value of the root pointer if there's any
// chance that the tree needs to be expanded!
void build_rtree (struct Node **root, SHPHandle sHP) {
    int nEntities,i;
    SHPObject	*psCShape;
    struct Rect bbox_shape;
    SHPGetInfo(sHP, &nEntities, NULL, NULL, NULL);
    for( i = 0; i < nEntities; i++ ) {
        psCShape = SHPReadObject ( sHP, i );
        bbox_shape.boundary[0]=(RectReal) psCShape->dfXMin;
        bbox_shape.boundary[1]=(RectReal) psCShape->dfYMin;
        bbox_shape.boundary[2]=(RectReal) psCShape->dfXMax;
        bbox_shape.boundary[3]=(RectReal) psCShape->dfYMax;
        SHPDestroyObject ( psCShape );
        RTreeInsertRect(&bbox_shape,i+1,root,0);
    }
}

void purge_shp_hash() {
    struct hashtable_itr *iterator=NULL;
    shpinfo *si;
    time_t time_now;
    int ret;

    time_now = sec_now();
    if (time_now > purge_time) {  // Time to purge
        //   fprintf(stderr,"Purging...\n");
        purge_time += PURGE_PERIOD;

        if (shp_hash) {
            // walk through the hash table and kill entries that are old

            iterator=hashtable_iterator(shp_hash);
            if (iterator) { // could be null if we've already purged everything
                do {
                    si=hashtable_iterator_value(iterator);
                    if (time_now > si->last_access+PURGE_PERIOD) {
                        // this is stale, hasn't been accessed in a long time
                        //                        fprintf(stderr,
                        // "    found stale entry for %s, deleting it.\n",
                        //     si->filename);
                        //fprintf(stderr,"    Destroying si=%lx\n",
                        //        (unsigned long int) si);
                        destroy_shpinfo(si);
                        // fprintf(stderr,"    removing from hashtable\n");
                        ret=hashtable_iterator_remove(iterator);
                    } else {
                        ret=hashtable_iterator_advance(iterator);
                    }
                } while (ret);
            }
        }
    }
    
}
#endif // USE_RTREE
