/* Copyright (C) 2000-2019 The Xastir Group                             */
/*                                                                       */
/*                                                                       */
/*  First draft                                                          */
/*                  KB3EGH 03/24/2004                                    */
/* needs -I/Developer/Headers/FlatCarbon                                 */
/*=======================================================================*/

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <Speech.h>
/* can't include this or X11's Cursor conflicts */
/* #include "xastir.h"*/
#include "snprintf.h"

// Must be last include file
#include "leak_detection.h"



static char last_speech_text[8000];
static time_t last_speech_time = (time_t)0;
static SpeechChannel channel;
int macspeech_processes = 0;





int SayText(char *text)
{
  OSErr err;
  pid_t child_pid;


  //if (debug_level & 2)
  //fprintf(stderr,"SayText: %s\n",text);

  // Check whether the last text was the same and it hasn't been
  // enough time between them (30 seconds).
  if ( (strcmp(last_speech_text,text) == 0) // Strings match
       && (last_speech_time + 30 > sec_now()) )
  {

    //fprintf(stderr,"Same text, skipping speech: %d seconds, %s\n",
    //    (int)(sec_now() - last_speech_time),
    //    text);

    return(1);
  }

  //fprintf(stderr,"Speaking: %s\n",text);

  xastir_snprintf(last_speech_text,
                  sizeof(last_speech_text),
                  "%s",
                  text);
  last_speech_time = sec_now();

  // Check for the variable going out-of-bounds
  if (macspeech_processes < 0)
  {
    macspeech_processes = 0;
  }

  // Allow only so many processes to be queued up ready to send
  // text to the speech subsystem.
  //
  if (macspeech_processes > 10)   // Too many processes queued up!
  {
    return(1);  // Don't send this string, return to calling program
  }

  // Create a separate process to handle the speech so that our
  // main process can continue processing packets and displaying
  // maps.
  //
  child_pid = fork();

  if (child_pid == -1)    // The fork failed
  {
    return(1);
  }

  if (child_pid == 0)     // Child process
  {

    macspeech_processes++;


    // Go back to default signal handler instead of calling
    // restart() on SIGHUP
    (void) signal(SIGHUP,SIG_DFL);


    // Wait for the speech system to be freed up.  Note that if
    // we have multiple processes queued up we don't know in
    // which order they'll get access to the speech subsystem.
    //
    while (SpeechBusy() == true)
    {
      usleep(1000000);
    }

    // The speech system is free, so send it our text.  Right
    // now we ignore any errors.
    //
    err = SpeakText(channel, text, strlen(text));

    macspeech_processes--;

    // Exit this child process.  We don't need it anymore.
    exit(0);
  }

  else                    // Parent process
  {
    // Drop through to the return
  }

  return(0);  // Return to the calling program
}





int SayTextInit(void)
{
  OSErr err;
  long response;
  long mask;
  VoiceSpec defaultVoiceSpec;
  VoiceDescription voiceDesc;

  err = Gestalt(gestaltSpeechAttr, &response);
  if (err != noErr)
  {
    fprintf(stderr,"can't init Mac Speech Synthesis\n");
    return(1);
  }

  mask = 1 << gestaltSpeechMgrPresent;
  if ((response & mask) == 0)
  {
    fprintf(stderr,"Mac Speech not supported\n");
    return(1);
  }

  err = GetVoiceDescription(nil, &voiceDesc, sizeof(voiceDesc));
  defaultVoiceSpec = voiceDesc.voice;
  err = NewSpeechChannel( &defaultVoiceSpec, &channel );
  if (err != noErr)
  {
    DisposeSpeechChannel(channel);
    fprintf(stderr,"Failed to open a speech channel\n");
    return(1);
  }

  last_speech_text[0] = '\0';
  last_speech_time = (time_t)0;

  return(0);
}

/* cleanup should err = DisposeSpeechChannel( channel ); */


