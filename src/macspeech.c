/* Copyright (C) 2004  The Xastir Group                                  */
/*                                                                       */
/* $Id$ */
/*                                                                       */
/*  First draft                                                          */
/*                  KB3EGH 03/24/2004                                    */
/* needs -I/Developer/Headers/FlatCarbon                                 */
/*=======================================================================*/
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

static char last_speech_text[8000];
static time_t last_speech_time = (time_t)0;
static SpeechChannel channel;

int SayText(char *text) {
    OSErr err;
    //if (debug_level & 2)
    //fprintf(stderr,"SayText: %s\n",text);

    // Check whether the last text was the same and it hasn't been
    // enough time between them (30 seconds).
    if ( (strcmp(last_speech_text,text) == 0) // Strings match
            && (last_speech_time + 30 > sec_now()) ) {

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

    err = SpeakText(channel, text, strlen(text));
    while (SpeechBusy() == true);
    return(0);
}

int SayTextInit(void) {
    OSErr err;
    long response;
    long mask;
    VoiceSpec defaultVoiceSpec;
    VoiceDescription voiceDesc;

    err = Gestalt(gestaltSpeechAttr, &response);
    if (err != noErr) {
	fprintf(stderr,"can't init Mac Speech Synthesis\n");
	return(1);
    }
    
    mask = 1 << gestaltSpeechMgrPresent;
    if ((response & mask) == 0) {
	fprintf(stderr,"Mac Speech not supported\n");
	return(1);
    }

    err = GetVoiceDescription(nil, &voiceDesc, sizeof(voiceDesc));
    defaultVoiceSpec = voiceDesc.voice;
    err = NewSpeechChannel( &defaultVoiceSpec, &channel );
    if (err != noErr) {
	DisposeSpeechChannel(channel);
	fprintf(stderr,"Failed to open a speech channel\n");
	return(1);
    }
 
    last_speech_text[0] = '\0';
    last_speech_time = (time_t)0;

    return(0);
}

/* cleanup should err = DisposeSpeechChannel( channel ); */
