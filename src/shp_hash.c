#include "config.h"

#ifdef USE_RTREE
#include <stdlib.h>
#include <stdio.h>
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

#include "hashtable.h"
#include "hashtable_itr.h"
#include "shp_hash.h"

static struct hashtable *shp_hash=NULL;
static crap=0;

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
            if (si->root) {
                RTreeDestroyNode(si->root);
            }
            if (si->filename) {
                free(si->filename);
            }
        } while (hashtable_iterator_advance(iterator));
        hashtable_destroy(shp_hash, 1);
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
                     // in the future we'll add a timestamp, too 

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
#endif // USE_RTREE
