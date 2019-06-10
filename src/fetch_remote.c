/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2019 The Xastir Group
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

#include "snprintf.h"

#if TIME_WITH_SYS_TIME
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <unistd.h>
#include <signal.h>
#include <string.h>

// Needed for Solaris
#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif  // HAVE_STRINGS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#ifdef HAVE_LIBCURL
  #include <curl/curl.h>
#endif  // HAVE_LIBCURL

// Needed for size_t
#include <sys/types.h>


// Must be last include file
#include "leak_detection.h"

extern int debug_level;
extern int net_map_timeout;

/* curl routines */
#ifdef HAVE_LIBCURL

struct FtpFile
{
  char *filename;
  FILE *stream;
};





size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
  struct FtpFile *out = (struct FtpFile *)stream;
  if (out && !out->stream)
  {
    out->stream=fopen(out->filename, "wb");
    if (!out->stream)
    {
      return -1;
    }
  }
  return fwrite(buffer, size, nmemb, out->stream);
}

/*
 * xastir_curl_init - create curl session with common options
 */
CURL *xastir_curl_init(char *errBuf)
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

    // write function
    curl_easy_setopt(mySession, CURLOPT_WRITEFUNCTION, curl_fwrite);

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
} // xastir_curl_init()

/*
 * fetch_remote_tile - downloads file using an open curl session
 * Returns curl result code.
 */
int fetch_remote_tile(CURL *session, char *tileURL, char *tileFileName)
{
  CURLcode res;
  struct FtpFile ftpfile;

  curl_easy_setopt(session, CURLOPT_URL, tileURL);
  ftpfile.filename = tileFileName;
  ftpfile.stream = NULL;
  curl_easy_setopt(session, CURLOPT_FILE, &ftpfile);

  res = curl_easy_perform(session);

  if (ftpfile.stream)
  {
    fclose(ftpfile.stream);
  }

  return(res);

} // fetch_remote_tile()

#endif  // HAVE_LIBCURL


/*
 * fetch_remote_file
 * Returns: 0 If file retrieved
 *          1 If there was a problem getting the file
 */
int fetch_remote_file(char *fileimg, char *local_filename)
{
#ifdef HAVE_LIBCURL
  CURL *curl;
  CURLcode res;
  char curlerr[CURL_ERROR_SIZE];
  struct FtpFile ftpfile;

//fprintf(stderr, "Fetching remote file: %s\n", fileimg);

  curl = xastir_curl_init(curlerr);

  if (curl)
  {

    // download from fileimg
    curl_easy_setopt(curl, CURLOPT_URL, fileimg);
    ftpfile.filename = local_filename;
    ftpfile.stream = NULL;
    curl_easy_setopt(curl, CURLOPT_FILE, &ftpfile);

    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    if (CURLE_OK != res)
    {
      fprintf(stderr, "curl told us %d\n", res);
      fprintf(stderr, "curlerr: %s\n", curlerr);
      fprintf(stderr,
              "Perhaps a timeout? Try increasing \"Internet Map Timout\".\n");
    }

    if (ftpfile.stream)
    {
      fclose(ftpfile.stream);
    }

    // Return error-code if we had trouble
    if (CURLE_OK != res)
    {
      return(1);
    }

  }
  else
  {
    fprintf(stderr,"Couldn't download the file %s\n", fileimg);
    fprintf(stderr,
            "Perhaps a timeout? Try increasing \"Internet Map Timout\".\n");

    return(1);
  }
  return(0);  // Success!

#else   // HAVE_LIBCURL

#ifdef HAVE_WGET

  char tempfile[500];

  //"%s --server-response --timestamping --user-agent=Xastir --tries=1 --timeout=%d --output-document=%s \'%s\' 2> /dev/null\n",
  xastir_snprintf(tempfile, sizeof(tempfile),
                  "%s --server-response --user-agent=Xastir --tries=1 --timeout=%d --output-document=%s \'%s\' 2> /dev/null\n",
                  WGET_PATH,
                  net_map_timeout,
                  local_filename,
                  fileimg);

  if (debug_level & 2)
  {
    fprintf(stderr,"%s",tempfile);
  }

  if ( system(tempfile) )     // Go get the file
  {
    fprintf(stderr,"Couldn't download the file\n");
    fprintf(stderr,
            "Perhaps a timeout? Try increasing \"Internet Map Timout\".\n");

    return(1);
  }
  return(0);  // Success!

#else // HAVE_WGET

  fprintf(stderr,"libcurl or 'wget' not installed.  Can't download file\n");
  return(1);

#endif  // HAVE_WGET
#endif  // HAVE_LIBCURL

}
