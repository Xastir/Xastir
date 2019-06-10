/*
 *
 * $Id: dlm.c,v 1.10 2018/07/14 21:32:43 MikeNix Exp $
 *
 * Copyright (C) 2018 The Xastir Group
 *
 * This file was contributed by Mike Nix.
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
 *
 */


#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>

// Needed for Solaris
#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif  // HAVE_STRINGS_H

#ifdef HAVE_LIBCURL
  #include <curl/curl.h>
#endif

#include "xastir.h"
#include "maps.h"    // only for MAX_FILENAME
#include "main.h"

#include "dlm.h"

// Must be last include file
#include "leak_detection.h"

/**********************************************************
 * DLM - DownLoad Manager - manage a list of items to download
 **********************************************************/

/* Notes on curl_multi and the tile queue:

   We could have used a thread for each transfer, but that's messy and wasteful.

   We could have used the curl_multi list as the queue, but that would end up
   with us either waiting until the last few tiles were downloading before returning,
   or just adding all the tiles and letting curl go fot it - which could end up
   with hundreds of simultaneous transfers, although curl is supposed to have a
   limiter of some kind, it delivers no control over what order to process the queue.

   Our approach is to queue all the tiles in our own queue, and return immediately
   while letting a download thread feed curl the most recently added tiles first.

   This way, if you scroll past an area, the tiles will all be queued for checking
   but we will always start downloading tiles in the current view (ie most
   recently added) first - without cancelling any tiles already started.


   Mini HowTo:

   in your drawing function, before reading tiles, do this:

      DLM_queue_tile(...) each map tile you want

      call DLM_do_transfers() - this only does anything if the DLM was compiled
                                with threading disabled

      optionally call DLM_wait_done() if you want to wait for all downloads to finish

      Draw your maps using the tiles available in your cache.

   When each download completes, the global request_new_image is incremented
   which triggers a redraw from the top level.  It is safe to repeat this whole
   sequence at each redraw as tiles are only added to the queue if they are
   not in the queue already and the file does not exist, or the file is older
   than 7 days.

*/

// Enable use of a separate download thread in the background
#define DLM_QUEUE_THREADED

// Enable using curl_multi_* to download more than one tile at a time
// Define this to use the code, set the number to limit the
// simultaneous transfers
// WARNING: defining this less than 1 will allow starting an unlimited number
// of transfers. You probably shouldn't do that!
// Also, don't set this too high or slow internet connections will have to
// wait too long before starting to download tiles in the current view
// when the user scrolls around a bit
#define USE_CURL_MULTI 8


#define DLM_Q_STOP    0
#define DLM_Q_STARTING    1
#define DLM_Q_RUN    5
#define DLM_Q_IDLE    8
#define DLM_Q_QUIT    9

#define MAX_DESCLEN    40

struct DLM_queue_entry
{
  struct DLM_queue_entry *next;
  struct DLM_queue_entry *prev;

  // These are only used by DLM_queue_tile for
  // checking if a tile is already queued
  // osm_zl is also used as a type flag (>=0 for tile, <0 for file)
  unsigned long      x;
  unsigned long      y;
  int              osm_zl;

  int              state;
  xastir_mutex      lock;
  char          fileName[MAX_FILENAME];
  char          desc[MAX_DESCLEN];
  char          *tempName;
  char          *url;

#ifdef HAVE_LIBCURL
  FILE          *stream;
#ifdef USE_CURL_MULTI
  char          *curlErrBuf;
  CURL          *curlSession;
#endif
#endif
};


/* DLM_queue_lock is held whenever reading or changing DLM_queue_entry or
 * the linkages of the items in the queue.  Each item also has a lock
 * for changing data in that item.
 */
xastir_mutex DLM_queue_lock = { .threadID=0, .lock=PTHREAD_MUTEX_INITIALIZER };
pthread_t DLM_queue_thread;

volatile int DLM_queue_progress_flag=0;

/* DLM_queue_state is protected by a lock to ensure memory consistency
 * between threads. It is held for every read or write of DLM_queue_state
 */
xastir_mutex DLM_state_lock = { .threadID=0, .lock=PTHREAD_MUTEX_INITIALIZER };
volatile int DLM_queue_state=DLM_Q_STOP;
struct DLM_queue_entry *DLM_queue=NULL;

void (*DLM_progress_callback)(void)=NULL;

/**********************************************************
 * DLM_get_queue_state - Read DLM_queue_state while
 * ensuring it is locked
 **********************************************************/
static int DLM_get_queue_state(void)
{
  int state;
  begin_critical_section(&DLM_state_lock, "DLM_get_queue_state");
  state = DLM_queue_state;
  end_critical_section(&DLM_state_lock, "DLM_get_queue_state");
  return state;
}

/**********************************************************
 * DLM_wait_done - wait until the transfers are all complete
 * returns the number of items in the queue if it timed out
 **********************************************************/
int DLM_wait_done(time_t timeout)
{
#ifdef DLM_QUEUE_THREADED
  while ((timeout > 0) && (DLM_get_queue_state() == DLM_Q_RUN))
  {
    sleep(1);
    timeout--;
  }
#else
  if (DLM_queue_len()>0)
  {
    DLM_do_transfers();
  }
#endif
  return DLM_queue_len();
}

/**********************************************************
 * DLM_check_progress - check if we have more tiles since last call
 **********************************************************/
int DLM_check_progress(void)
{
  int p=DLM_queue_progress_flag;
  DLM_queue_progress_flag=0;
  return p;
}

/**********************************************************
 * DLM_queue_progress - called when new tiles are available
 **********************************************************/
static void DLM_queue_progress(int prog)
{
  if (prog)
  {
    DLM_queue_progress_flag=1;

    // trigger a redraw of the screen
    request_new_image++;

    if (DLM_progress_callback!=NULL)
    {
      DLM_progress_callback();
    }
  }
}

/**********************************************************
 * DLM_queue_len - return number of tiles queued for download/check
 **********************************************************/
int DLM_queue_len(void)
{
  struct DLM_queue_entry *q=DLM_queue;
  int count=0;

  begin_critical_section(&DLM_queue_lock, "DLM_queue_len");
  while (q)
  {
    count++;
    q=q->next;
  }
  end_critical_section(&DLM_queue_lock, "DLM_queue_len");
  //fprintf(stderr,"DLM_queue_len is %d\n", count);
  return count;
}

/**********************************************************
 * abort_DLM_queue_abort() - stop the transfer thread
 **********************************************************/
void DLM_queue_abort(void)
{
  begin_critical_section(&DLM_state_lock, "DLM_queue_abort");
  DLM_queue_state = DLM_Q_QUIT;
  end_critical_section(&DLM_state_lock, "DLM_queue_abort");
  //fprintf(stderr, "DLM_queue aborting\n");
}

/**********************************************************
 * DLM_queue_entry_alloc() - allocate a queue entry
 **********************************************************/
static struct DLM_queue_entry *DLM_queue_entry_alloc(void)
{
  return malloc(sizeof(struct DLM_queue_entry));
}

/**********************************************************
 * DLM_queue_entry_free() - free memory used by a tile queue entry
 **********************************************************/
static void DLM_queue_entry_free(struct DLM_queue_entry *q)
{
  if (q)
  {
    begin_critical_section(&DLM_queue_lock, "DLM_queue_entry_free:Queue Lock");
    begin_critical_section(&(q->lock), "DLM_queue_entry_free:tile lock");

    //fprintf(stderr, "DLM_queue_free %s, ishead=%d\n", q->desc, DLM_queue_entry == q);

    if ((q->state==DLM_Q_IDLE) || (q->state==DLM_Q_STOP))
    {
      // mark as being freed, just in case
      q->state=-1;

      // if we are the head of the queue, it needs updating
      if (DLM_queue == q)
      {
        DLM_queue = q->next;
      }

      // remove this entry from whatever list it's in
      if (q->next)
      {
        q->next->prev = q->prev;
      }
      if (q->prev)
      {
        q->prev->next = q->next;
      }

      // free anything we point to
      if (q->url)
      {
        free(q->url);
      }
      if (q->tempName)
      {
        free(q->tempName);
      }

      //        if (q->serverURL) free(q->serverURL);
      //        if (q->baseDir)   free(q->baseDir);
      //        if (q->ext)       free(q->ext);

#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
      if (q->curlErrBuf)
      {
        free(q->curlErrBuf);
      }
      if (q->curlSession)
      {
        curl_easy_cleanup(q->curlSession);
      }
      if (q->stream)
      {
        fclose(q->stream);
      }
#endif
#endif

      // free us
      end_critical_section(&(q->lock), "DLM_queue_entry_free:tile unlock");
      free(q);
    }
    else
    {
      end_critical_section(&(q->lock), "DLM_queue_entry_free:tile_unlock");
    }
    end_critical_section(&DLM_queue_lock, "DLM_queue_entry_free:Queue_unlock");
  }
}

/**********************************************************
 * DLM_queue_destroy() - free all entries in the queue
 **********************************************************/
static void DLM_queue_destroy(void)
{
  struct DLM_queue_entry *next, *q;
  int count=0;

  q = DLM_queue;
  while (q)
  {
    next=q->next;
    if (q->state==DLM_Q_IDLE)
    {
      DLM_queue_entry_free(q);
      count++;
    }
    q=next;
  }
  //fprintf(stderr, "DLM_queue_destroy: %d entries dropped\n", count);
}

/**********************************************************
 * DLM_queue_abort_tiles() - free all tiles in the queue
 **********************************************************/
void DLM_queue_abort_tiles(void)
{
  struct DLM_queue_entry *next, *q;
  int count=0;

  q = DLM_queue;
  while (q)
  {
    next=q->next;
    if ((q->state==DLM_Q_IDLE) && (q->osm_zl>=0))
    {
      DLM_queue_entry_free(q);
      count++;
    }
    q=next;
  }
  //fprintf(stderr, "DLM_queue_abort_tiles: %d entries dropped\n", count);
}

/**********************************************************
 * DLM_queue_abort_files() - free all non-tiles in the queue
 **********************************************************/
void DLM_queue_abort_files(void)
{
  struct DLM_queue_entry *next, *q;
  int count=0;

  q = DLM_queue;
  while (q)
  {
    next=q->next;
    if ((q->state==DLM_Q_IDLE) && (q->osm_zl<0))
    {
      DLM_queue_entry_free(q);
      count++;
    }
    q=next;
  }
  //fprintf(stderr, "DLM_queue_abort_files: %d entries dropped\n", count);
}

/**********************************************************
 * DLM_store_file() - move the temp file into place if we used one
 **********************************************************/
static int DLM_store_file(struct DLM_queue_entry *q)
{
  int rc;
  if (!q->tempName)
  {
    return 0;
  }

  //unlink(t->fileName); // not needed on Linux (the other OS?)
  if ((rc=rename(q->tempName, q->fileName)))
  {
    fprintf(stderr, "DLM_transfer_thread: unable to rename %s->%s: errno=%d\n",
            q->tempName, q->fileName, errno);
  }
  else
  {
    free(q->tempName);
    q->tempName=NULL;
  }
  return rc;
}

/**********************************************************
 * DLM_get_next_tile() - find the next idle tile ready for download
 * also locks that tile
 **********************************************************/
static struct DLM_queue_entry *DLM_get_next_tile(int state)
{
  struct DLM_queue_entry *q;
  begin_critical_section(&DLM_queue_lock, "DLM_transfer_thread: queue lock");
  q = DLM_queue;
  while (q && (q->state!=state))
  {
    q=q->next;
  }
  if (q)
  {
    begin_critical_section(&(q->lock), "DLM_transfer_thread: tile lock");
  }
  end_critical_section(&DLM_queue_lock, "DLM_transfer_thread: queue unlock");
  return q;
}


#ifdef HAVE_LIBCURL
/**********************************************************
 * DLM_curl_fwrite_callback() - callback for curl_multi
 **********************************************************/
static size_t DLM_curl_fwrite_callback(void *buffer, size_t size, size_t nmemb, void *stream)
{
  struct DLM_queue_entry *out = (struct DLM_queue_entry *)stream;
  if (out && !out->stream)
  {
    out->stream=fopen((out->tempName ? out->tempName : out->fileName), "wb");
    //fprintf(stderr,"DLM_curl_fwrite_callback: Opening %s\n", out->fileName);
    if (!out->stream)
    {
      return -1;
    }
  }
  return fwrite(buffer, size, nmemb, out->stream);
}

/**********************************************************
 * DLM_curl_progress_callback() - callback for curl_multi
 **********************************************************/
static int DLM_curl_progress_callback(void *p,
                                      double UNUSED(dltotal),
                                      double UNUSED(dlnow),
                                      double UNUSED(ultotal),
                                      double UNUSED(ulnow) )
{
  struct DLM_queue_entry *tile = (struct DLM_queue_entry *)p;
  if (!tile)
  {
    return 0;
  }

  // possibly update some display somewhere?

  return 0;
}

/**********************************************************
 * DLM_curl_set_queue_entry() - curl session options we can't set
 * at initialization in some cases.
 **********************************************************/
static void DLM_curl_set_queue_entry(CURL *mySession, struct DLM_queue_entry *qentry)
{
  curl_easy_setopt(mySession, CURLOPT_FILE, qentry);

  curl_easy_setopt(mySession, CURLOPT_PROGRESSDATA, qentry);
  curl_easy_setopt(mySession, CURLOPT_PRIVATE, qentry);

#ifdef USE_CURL_MULTI
  qentry->curlSession = mySession;
#endif
}

/**********************************************************
 * DLM_curl_init() - prep a curl handle our way
 *
 **********************************************************/
static CURL *DLM_curl_init(char *errBuf)
{
  CURL *mySession;
  char agent_string[15];

  mySession = curl_easy_init();
  if (mySession != NULL)
  {
    if (debug_level & 8192)
    {
      curl_easy_setopt(mySession, CURLOPT_VERBOSE, 1);
    }
    else
    {
      curl_easy_setopt(mySession, CURLOPT_VERBOSE, 0);
    }

    curl_easy_setopt(mySession, CURLOPT_ERRORBUFFER, errBuf);

    xastir_snprintf(agent_string, sizeof(agent_string),"Xastir");
    curl_easy_setopt(mySession, CURLOPT_USERAGENT, agent_string);

    // write and progress functions
    curl_easy_setopt(mySession, CURLOPT_WRITEFUNCTION, DLM_curl_fwrite_callback);
    curl_easy_setopt(mySession, CURLOPT_PROGRESSFUNCTION, DLM_curl_progress_callback);

    curl_easy_setopt(mySession, CURLOPT_TIMEOUT, (long)net_map_timeout);
    curl_easy_setopt(mySession, CURLOPT_CONNECTTIMEOUT, (long)(net_map_timeout/2));

    // Added in libcurl 7.9.8
#if (LIBCURL_VERSION_NUM >= 0x070908)
    curl_easy_setopt(mySession, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
#endif  // LIBCURL_VERSION_NUM

    // Added in libcurl 7.10.6
#if (LIBCURL_VERSION_NUM >= 0x071006)
    curl_easy_setopt(mySession, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
#endif  // LIBCURL_VERSION_NUM

    // Added in libcurl 7.10.7
#if (LIBCURL_VERSION_NUM >= 0x071007)
    curl_easy_setopt(mySession, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
#endif  // LIBCURL_VERSION_NUM

    // Added in libcurl 7.10
#if (LIBCURL_VERSION_NUM >= 0x070a00)
    // This prevents a segfault for the case where we get a timeout on
    // domain name lookup.  It has to do with the ALARM signal
    // and siglongjmp(), which we use in hostname.c already.
    // This URL talks about it a bit more, plus see the libcurl
    // docs:
    //
    //     http://curl.haxx.se/mail/lib-2002-12/0103.html
    //
    curl_easy_setopt(mySession, CURLOPT_NOSIGNAL, 1);
#endif // LIBCURL_VERSION_NUM
  }

  return(mySession);
} // DLM_curl_init()
#endif // HAVE_LIBCURL

/**********************************************************
 * DLM_transfer_thread() - retrieve item queued for download
 **********************************************************/
static void *DLM_transfer_thread(void * UNUSED(arg) )
{
  struct DLM_queue_entry *tile;
#ifdef DLM_QUEUE_THREADED
  int idleCnt;
#endif

#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
  CURLM *multiSession;
  CURLMsg *msg;
  int    msgsLeft;
  int    runningTransfers;
#else
  CURL *mySession;
  char errBuf[CURL_ERROR_SIZE];
  int  curl_result;
#endif
#endif // HAVE_LIBCURL

  // detach - we don't care about the result, and won't be calling pthread_join()
  pthread_detach(pthread_self());

  begin_critical_section(&DLM_state_lock, "DLM_transfer_thread set to run");
  DLM_queue_state = DLM_Q_RUN;
  end_critical_section(&DLM_state_lock, "DLM_transfer_thread set to run");
#ifdef DLM_QUEUE_THREADED
  idleCnt=0;
#endif

  if (debug_level & 1)
  {
    fprintf(stderr, "DLM_transfer_thread started\n");
  }

#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
  multiSession = curl_multi_init();
  runningTransfers=0;
#else
  mySession = DLM_curl_init(errBuf);
#endif
#endif // HAVE_LIBCURL

  // get the tiles
  while (DLM_get_queue_state() != DLM_Q_QUIT)
  {

#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
    curl_multi_perform(multiSession, &runningTransfers);

    // handle any "download complete" messages from curl
    while ((msg=curl_multi_info_read(multiSession, &msgsLeft)))
    {
      if (msg->msg==CURLMSG_DONE)
      {
        if (msg->easy_handle)
        {
          struct DLM_queue_entry *t;
          curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, (void *)&t);
          t->state=DLM_Q_STOP;

          if (t->stream)
          {
            fclose(t->stream);
            t->stream=NULL;
          }

          if (msg->data.result != 0)
          {
            fprintf(stderr, "CURL error downloading %s: %d\nURL:%s\n%s\n",
                    t->desc, msg->data.result, t->url, t->curlErrBuf);
          }

          if ((msg->data.result==0) && (!DLM_store_file(t)))
          {
            DLM_queue_progress(1);
          }
          else
          {
            unlink(t->tempName);
          }
          //fprintf(stderr,"DLM_transfer_queue: completed item %s\n",t->desc);

          curl_multi_remove_handle(multiSession, msg->easy_handle);
          curl_easy_cleanup(msg->easy_handle);
          t->curlSession=NULL;
          DLM_queue_entry_free(t);
        }
      }
      else
      {
        fprintf(stderr, "CURL Message: (%d)\n", msg->msg);
      }
    }

#ifdef DLM_QUEUE_THREADED
    if (runningTransfers>0)
    {
      idleCnt=0;
    }
#endif

    if ((USE_CURL_MULTI < 1) || (runningTransfers < USE_CURL_MULTI))
    {
#endif
#endif
      // get the next tile that is idle. Tile is locked for us
      tile=DLM_get_next_tile(DLM_Q_IDLE);

      if (tile)
      {
        tile->state = DLM_Q_RUN;
#ifdef DLM_QUEUE_THREADED
        idleCnt=0;
#endif
        end_critical_section(&(tile->lock), "DLM_transfer_thread: tile unlock");
        //fprintf(stderr,"DLM_transfer_queue: started item %s, qlen=%d\n",tile->desc,DLM_queue_len());

#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
        if (!tile->curlErrBuf)
        {
          tile->curlErrBuf=malloc(CURL_ERROR_SIZE);
        }
        if (!tile->curlSession)
        {
          tile->curlSession=DLM_curl_init(tile->curlErrBuf);
        }

        DLM_curl_set_queue_entry(tile->curlSession, tile);
        curl_easy_setopt(tile->curlSession, CURLOPT_URL, tile->url);
        curl_easy_setopt(tile->curlSession, CURLOPT_FAILONERROR, 1);

        curl_multi_add_handle(multiSession, tile->curlSession);

#else // USE_CURL_MULTI

        DLM_curl_set_queue_entry(mySession, tile);
        curl_easy_setopt(mySession, CURLOPT_URL, tile->url);
        curl_easy_setopt(mySession, CURLOPT_FAILONERROR, 1);

        curl_result = curl_easy_perform(mySession);
        if (tile->stream)
        {
          fclose(tile->stream);
          tile->stream=NULL;
        }
        if (curl_result != CURLE_OK)
        {
          fprintf(stderr, "Download error for %s: curl result %d\nURL:%s\ncurlerr:%s",
                  tile->desc, curl_result, tile->url, errBuf);
        }
        else
        {
          if (!DLM_store_file(tile))
          {
            DLM_queue_progress(1);
          }
        }
#endif // USE_CURL_MULTI
#else // HAVE_LIBCURL

        // We have no other option - use wget, one file at a time
        {
          char cmd[500];
          xastir_snprintf(cmd, sizeof(cmd),
                          "%s --server-response --user-agent=Xastir --tries=1 --timeout=%d --output-document=\'%s\' \'%s\' 2> /dev/null\n",
                          "wget",
                          net_map_timeout,
                          (tile->tempName ? tile->tempName : tile->fileName),
                          tile->url);
          if (system(cmd))
          {
            fprintf(stderr, "Couldn't download the file with wget\n");
          }
          else
          {
            if (!DLM_store_file(tile))
            {
              DLM_queue_progress(1);
            }
          }
        }
#endif // HAVE_LIBCURL

#if defined(HAVE_LIBCURL) && defined(USE_CURL_MULTI)
#else
        tile->state = DLM_Q_STOP;
        //fprintf(stderr,"DLM_transfer_queue: done item %s\n",tile->desc);
        DLM_queue_entry_free(tile);
#endif

#ifdef DLM_QUEUE_THREADED
      }
      else if (idleCnt < 10)
      {
        begin_critical_section(&DLM_state_lock, "DLM_transfer_thread idle check");
        DLM_queue_state = DLM_Q_IDLE;
        end_critical_section(&DLM_state_lock, "DLM_transfer_thread idle check");
        //fprintf(stderr,"DLM_transfer_queue: idling\n");
        sleep(1);
        idleCnt++;
#endif
      }
      else
      {
        begin_critical_section(&DLM_state_lock, "DLM_transfer_thread set quit");
        DLM_queue_state = DLM_Q_QUIT;
        end_critical_section(&DLM_state_lock, "DLM_transfer_thread set quit");
      }
#ifndef DLM_QUEUE_THREADED
      HandlePendingEvents(app_context);
      if (interrupt_drawing_now)
      {
        begin_critical_section(&DLM_state_lock, "DLM_transfer_thread interrupt quit");
        DLM_queue_state = DLM_Q_QUIT;
        end_critical_section(&DLM_state_lock, "DLM_transfer_thread interrupt quit");
      }
#endif
#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
    }
    usleep(100000); // 0.1 seconds - don't hog the CPU :)
    // also limits us to starting 10 downloads/second
    // and staggers the downloads a bit
#endif
#endif

  }

#ifdef HAVE_LIBCURL
#ifdef USE_CURL_MULTI
  curl_multi_cleanup(multiSession);
#else
  curl_easy_cleanup(mySession);
#endif
#endif // HAVE_LIBCURL

  DLM_queue_destroy();
  if (debug_level & 1)
  {
    fprintf(stderr,"DLM_transfer_thread stopped\n");
  }
  begin_critical_section(&DLM_state_lock, "DLM_transfer_thread stop update");
  DLM_queue_state = DLM_Q_STOP;
  end_critical_section(&DLM_state_lock, "DLM_transfer_thread stop update");
  return NULL;
}


/**********************************************************
 * DLM_queue_start_if_needed()
 * Start the transfers if they need starting
 **********************************************************/
static void DLM_queue_start_if_needed(void)
{
#ifdef DLM_QUEUE_THREADED
  if (DLM_get_queue_state() == DLM_Q_STOP)
  {
    // start the thread
    // Queue state lock not needed as there is no other thread running here
    DLM_queue_state = DLM_Q_STARTING;
    if (pthread_create(&DLM_queue_thread, NULL, DLM_transfer_thread, NULL))
    {
      //fprintf(stderr,"Error creating OSM transfer thread\n");
      DLM_queue_state = DLM_Q_STOP;
    }
  }
#endif
}

/**********************************************************
 * DLM_do_transfers() - download all tiles now
 * Does nothing if we are in threaded mode
 **********************************************************/
void DLM_do_transfers(void)
{
#ifdef DLM_QUEUE_THREADED
  if (DLM_queue_len()>0)
  {
    DLM_queue_start_if_needed();
  }
#else
  DLM_transfer_thread(NULL);
#endif
}

/**********************************************************
 * DLM_queue_add() - Queue a prepared entry for download.
 * Internal use only - no checking is done!
 **********************************************************/

static void DLM_queue_add(struct DLM_queue_entry *ent)
{
  if (ent->url && ent->tempName)
  {
    // if the thread is quitting, wait till it's done
    while (DLM_get_queue_state() == DLM_Q_QUIT);

    // queue this tile
    //fprintf(stderr,"OSM queueing %s, qlen=%d\n",tile->fileName,DLM_queue_len());
    begin_critical_section(&DLM_queue_lock, "DLM_queue_add");
    if (DLM_queue)
    {
      DLM_queue->prev=ent;
    }
    ent->next = DLM_queue;
    DLM_queue = ent;
    end_critical_section(&DLM_queue_lock, "DLM_queue_add");

    DLM_queue_start_if_needed();
  }
  else
  {
    DLM_queue_entry_free(ent);
  }
}


/**********************************************************
 * DLM_queue_tile() - queue map tiles for download
 * Written for OpenStreetMap but generic enough to live here.
 **********************************************************/
void DLM_queue_tile(
  char          *serverURL,
  unsigned long      x,
  unsigned long      y,
  int              osm_zl,
  char          *baseDir,
  char          *ext )
{

  struct DLM_queue_entry *tile, *q;
  struct stat sb;
  int    len;

  // see if it's already queued
  begin_critical_section(&DLM_queue_lock, "DLM_queue_tile:check queue");
  q=DLM_queue;
  while (q && ((q->x!=x) || (q->y!=y) || (q->osm_zl!=osm_zl) ))
  {
    q=q->next;
  }
  end_critical_section(&DLM_queue_lock, "DLM_queue_tile:check queue");
  if (q)
  {
    //fprintf(stderr, "OSM %s already queued\n", q->desc);
    return;
  }

  tile=DLM_queue_entry_alloc();
  if (!tile)
  {
    return;
  }

  tile->next      = NULL;
  tile->prev      = NULL;
  //  tile->serverURL = strndup(serverURL, MAX_FILENAME);
  //  tile->baseDir   = strndup(baseDir, MAX_FILENAME);
  //  tile->ext       = strndup(ext, MAX_FILENAME);
  tile->x         = x;
  tile->y         = y;
  tile->osm_zl    = osm_zl;
  tile->state     = DLM_Q_IDLE;

  tile->url       = NULL;
  tile->fileName[0]='\0';

  init_critical_section(&(tile->lock));

#ifdef HAVE_LIBCURL
  tile->stream=NULL;
#ifdef USE_CURL_MULTI
  tile->curlErrBuf=NULL;
  tile->curlSession=NULL;
#endif
#endif

  xastir_snprintf(tile->desc, sizeof(tile->desc), "Tile:%u/%lu/%lu", osm_zl, x, y);

  xastir_snprintf(tile->fileName, sizeof(tile->fileName), "%u/%lu/%lu.%s",
                  osm_zl, x, y, ext);
  len = strlen(serverURL) + strlen(tile->fileName) +2;
  tile->url = malloc(len);
  if (tile->url)
  {
    xastir_snprintf(tile->url, len, "%s/%s", serverURL, tile->fileName);
  }

  xastir_snprintf(tile->fileName, sizeof(tile->fileName),
                  "%s/%u/%lu/%lu.%s.part",
                  baseDir, osm_zl, x, y, ext);
  tile->tempName = strdup(tile->fileName);

  xastir_snprintf(tile->fileName, sizeof(tile->fileName),
                  "%s/%u/%lu/%lu.%s",
                  baseDir, osm_zl, x, y, ext);


  // if we have the file and it's < 7 days old, don't queue it
  if (stat(tile->fileName, &sb) != -1)
  {
    if ((sb.st_mtime + (7 * 24 * 60 * 60)) >= time(NULL))
    {
      //fprintf(stderr,"%s is fresh in cache\n", tile->fileName);
      tile->state=DLM_Q_STOP;
      DLM_queue_entry_free(tile);
      return;
    }
  }

  DLM_queue_add(tile);
}



/**********************************************************
 * DLM_queue_file() - Queue a file for download.
 **********************************************************/
void DLM_queue_file(
  char          *url,
  char          *filename,
  time_t          expiry )
{

  struct DLM_queue_entry *tile, *q;
  struct stat sb;
  char   *p;

  // see if it's already queued
  begin_critical_section(&DLM_queue_lock, "DLM_queue_file:check queue");
  q=DLM_queue;
  while (q && (q->osm_zl>=0) && strncmp(filename,q->fileName,sizeof(q->fileName)))
  {
    q=q->next;
  }
  end_critical_section(&DLM_queue_lock, "DLM_queue_file:check queue");
  if (q)
  {
    //fprintf(stderr, "OSM %s already queued\n", filename);
    return;
  }

  tile=DLM_queue_entry_alloc();
  if (!tile)
  {
    return;
  }

  tile->next      = NULL;
  tile->prev      = NULL;
  //  tile->serverURL = strndup(serverURL, MAX_FILENAME);
  //  tile->baseDir   = strndup(baseDir, MAX_FILENAME);
  //  tile->ext       = strndup(ext, MAX_FILENAME);
  tile->x         = 0;
  tile->y         = 0;
  tile->osm_zl    = -1;
  tile->state     = DLM_Q_IDLE;

  tile->url       = strdup(url);

  init_critical_section(&(tile->lock));

#ifdef HAVE_LIBCURL
  tile->stream=NULL;
#ifdef USE_CURL_MULTI
  tile->curlErrBuf=NULL;
  tile->curlSession=NULL;
#endif
#endif

  p=filename;
  while (p && *p)
  {
    p++;
  }
  p-=sizeof(tile->desc)-6;
  if (p<filename)
  {
    xastir_snprintf(tile->desc, sizeof(tile->desc), "File:%s", filename);
  }
  else
  {
    xastir_snprintf(tile->desc, sizeof(tile->desc), "File:...%s", p+3);
  }

  xastir_snprintf(tile->fileName, sizeof(tile->fileName), "%s.part", filename);
  tile->tempName = strdup(tile->fileName);

  strncpy(tile->fileName, filename, sizeof(tile->fileName));

  // if we have the file and it's < expiry seconds old or expiry<0, don't queue it
  if ((expiry!=0) && stat(tile->fileName, &sb) != -1)
  {
    time_t age = time(NULL) - sb.st_mtime;

    if ((expiry < 0) || (age < expiry))
    {
      //fprintf(stderr,"%s is fresh\n", tile->fileName);
      tile->state=DLM_Q_STOP;
      DLM_queue_entry_free(tile);
      return;
    }
  }

  DLM_queue_add(tile);
}


///////////////////////////////////////////// End of DownLoadManager code ///////////////////////////////////////
