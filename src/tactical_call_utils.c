/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
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

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "hashtable.h"
#include "hashtable_itr.h"

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

#include "xastir.h"
#include "globals.h"
#include "snprintf.h"
#include "main.h"
#include "xa_config.h"

static struct hashtable *tactical_hash = NULL;
#define TACTICAL_HASH_SIZE 1024

// Multiply all the characters in the callsign, truncated to
// TACTICAL_HASH_SIZE
//
unsigned int tactical_hash_from_key(void *key)
{
  unsigned char *jj = key;
  unsigned int tac_hash = 1;

  while (*jj != '\0')
  {
    tac_hash = tac_hash * (unsigned int)*jj++;
  }

  tac_hash = tac_hash % TACTICAL_HASH_SIZE;

//    fprintf(stderr,"hash = %d\n", tac_hash);
  return (tac_hash);
}





int tactical_keys_equal(void *key1, void *key2)
{

//fprintf(stderr,"Comparing %s to %s\n",(char *)key1,(char *)key2);
  if (strlen((char *)key1) == strlen((char *)key2)
      && strncmp((char *)key1,(char *)key2,strlen((char *)key1))==0)
  {
//fprintf(stderr,"    match\n");
    return(1);
  }
  else
  {
//fprintf(stderr,"  no match\n");
    return(0);
  }
}





void init_tactical_hash(int clobber)
{
//    fprintf(stderr," Initializing tactical hash \n");
  // make sure we don't leak
//fprintf(stderr,"init_tactical_hash\n");
  if (tactical_hash)
  {
//fprintf(stderr,"Already have one!\n");
    if (clobber)
    {
//fprintf(stderr,"Clobbering hash table\n");
      hashtable_destroy(tactical_hash, 1);
      tactical_hash=create_hashtable(TACTICAL_HASH_SIZE,
                                     tactical_hash_from_key,
                                     tactical_keys_equal);
    }
  }
  else
  {
//fprintf(stderr,"Creating hash table from scratch\n");
    tactical_hash=create_hashtable(TACTICAL_HASH_SIZE,
                                   tactical_hash_from_key,
                                   tactical_keys_equal);
  }
}





char *get_tactical_from_hash(char *callsign)
{
  char *result;

  if (callsign == NULL || *callsign == '\0')
  {
    fprintf(stderr,"Empty callsign passed to get_tactical_from_hash()\n");
    return(NULL);
  }

  if (!tactical_hash)    // no table to search
  {
//fprintf(stderr,"Creating hash table\n");
    init_tactical_hash(1); // so create one
    return NULL;
  }

//    fprintf(stderr,"   searching for %s...",callsign);

  result=hashtable_search(tactical_hash,callsign);

  if (result)
  {
//            fprintf(stderr,"\t\tFound it, %s, len=%d, %s\n",
//                callsign,
//                strlen(callsign),
//                result);
  }
  else
  {
//            fprintf(stderr,"\t\tNot found, %s, len=%d\n",
//                callsign,
//                strlen(callsign));
  }

  return (result);
}





// This function checks whether there already is something in the
// hashtable that matches.  If a match found, it overwrites the
// tactical call for that entry, else it inserts a new record.
//
void add_tactical_to_hash(char *callsign, char *tactical_call)
{
  char *temp1;    // tac-call
  char *temp2;    // callsign
  char *ptr;


  // Note that tactical_call can be '\0', which means we're
  // getting rid of a previous tactical call.
  //
  if (callsign == NULL || *callsign == '\0'
      || tactical_call == NULL)
  {
    return;
  }

  if (!tactical_hash)    // no table to add to
  {
//fprintf(stderr,"init_tactical_hash\n");
    init_tactical_hash(1); // so create one
  }

  // Remove any matching entry to avoid duplicates
  ptr = hashtable_remove(tactical_hash, callsign);
  if (ptr)    // If value found, free the storage space for it as
  {
    // the hashtable_remove function doesn't.  It does
    // however remove the key (callsign) ok.
    free(ptr);
  }

  temp1 = (char *)malloc(MAX_TACTICAL_CALL+1);
  CHECKMALLOC(temp1);

  temp2 = (char *)malloc(MAX_CALLSIGN+1);
  CHECKMALLOC(temp2);

//fprintf(stderr, "\t\t\tAdding %s = %s...\n", callsign, tactical_call);

  xastir_snprintf(temp2,
                  MAX_CALLSIGN+1,
                  "%s",
                  callsign);

  xastir_snprintf(temp1,
                  MAX_TACTICAL_CALL+1,
                  "%s",
                  tactical_call);

  //                                   (key)  (value)
  //                         hash      call   tac-call
  if (!hashtable_insert(tactical_hash, temp2, temp1))
  {
    fprintf(stderr,"Insert failed on tactical hash --- fatal\n");
    free(temp1);
    free(temp2);
    exit(1);
  }

  // A check to see whether hash insert/update worked properly
  ptr = get_tactical_from_hash(callsign);
  if (!ptr)
  {
    fprintf(stderr,"***Failed hash insert/update***\n");
  }
  else
  {
//fprintf(stderr,"Current: %s -> %s\n",
//    callsign,
//    ptr);
  }
}





void destroy_tactical_hash(void)
{
  struct hashtable_itr *iterator = NULL;
  char *value;

  if (tactical_hash && hashtable_count(tactical_hash) > 0)
  {

    iterator = hashtable_iterator(tactical_hash);

    do
    {
      if (iterator)
      {
        value = hashtable_iterator_value(iterator);
        if (value)
        {
          free(value);
        }
      }
    }
    while (hashtable_iterator_remove(iterator));

    // Destroy the hashtable, freeing what's left of the
    // entries.
    hashtable_destroy(tactical_hash, 1);

    tactical_hash = NULL;

    if (iterator)
    {
      free(iterator);
    }
  }
}





//
// Tactical callsign logging
//
// Logging function called from the Assign Tactical Call right-click
// menu option and also from db.c:fill_in_tactical_call.  Each
// tactical assignment is logged as one line.
//
// We need to check for identical callsigns in the file, deleting
// lines that have the same name and adding new records to the end.
// Actually  BAD IDEA!  We want to keep the history of the tactical
// calls so that we can trace the changes later.
//
// Best method:  Look for lines containing matching callsigns and
// comment them out, adding the new tactical callsign at the end of
// the file.
//
// Do we need to use a special marker to mean NO tactical callsign
// is assigned?  This is for when we had a tactical call but now
// we're removing it.  The reload_tactical_calls() routine below
// would need to match.  Since we're using comma-delimited files, we
// can just check for an empty string instead.
//
void log_tactical_call(char *call_sign, char *tactical_call_sign)
{
  char file[MAX_VALUE];
  FILE *f;


  // Add it to our in-memory hash so that if stations expire and
  // then appear again they get assigned the same tac-call.
  add_tactical_to_hash(call_sign, tactical_call_sign);


  get_user_base_dir("config/tactical_calls.log", file, sizeof(file));

  f=fopen(file,"a");
  if (f!=NULL)
  {

    if (tactical_call_sign == NULL)
    {
      fprintf(f,"%s,\n",call_sign);
    }
    else
    {
      fprintf(f,"%s,%s\n",call_sign,tactical_call_sign);
    }

    (void)fclose(f);

    if (debug_level & 1)
    {
      fprintf(stderr,
              "Saving tactical call to file: %s:%s",
              call_sign,
              tactical_call_sign);
    }
  }
  else
  {
    fprintf(stderr,"Couldn't open file for appending: %s\n", file);
  }
}





//
// Function to load saved tactical calls back into xastir.  This
// is called on startup.  This implements persistent tactical calls
// across xastir restarts.
//
// Here we create a hash lookup and store one record for each valid
// line read from the tactical calls log file.  The key for each
// hash entry is the callsign-SSID.  Here we simply read them in and
// create the hash.  When a new station is heard on the air, it is
// checked against this hash and the tactical call field filled in
// if there is a match.
//
// Note that the length of "line" can be up to max_device_buffer,
// which is currently set to 4096.
//
void reload_tactical_calls(void)
{
  char file[MAX_VALUE];
  FILE *f;
  char line[300+1];


  get_user_base_dir("config/tactical_calls.log", file, sizeof(file));

  f=fopen(file,"r");
  if (f!=NULL)
  {

    while (fgets(line, 300, f) != NULL)
    {

      if (debug_level & 1)
      {
        fprintf(stderr,"loading tactical calls from file: %s",line);
      }

      if (line[0] != '#')     // skip comment lines
      {
        char *ptr;

        // we're dealing with comma-separated files, so
        // break the two pieces at the comma.
        ptr = index(line,',');

        if (ptr != NULL)
        {
          char *ptr2;


          ptr[0] = '\0';  // terminate the callsign
          ptr++;  // point to the tactical callsign

          // check for lf
          ptr2 = index(ptr,'\n');
          if (ptr2 != NULL)
          {
            ptr2[0] = '\0';
          }

          // check for cr
          ptr2 = index(ptr,'\r');
          if (ptr2 != NULL)
          {
            ptr2[0] = '\0';
          }

          if (debug_level & 1)
          {
            fprintf(stderr, "Call=%s,\tTactical=%s\n", line, ptr);
          }

          //                   call tac-call
          add_tactical_to_hash(line, ptr);
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    if (debug_level & 1)
    {
      fprintf(stderr,"couldn't open file for reading: %s\n", file);
    }
  }

  /*
      if (tactical_hash) {
          fprintf(stderr,"Stations w/tactical calls defined: %d\n",
              hashtable_count(tactical_hash));
      }
  */

}





