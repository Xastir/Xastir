/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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

#include "config.h"
#include "snprintf.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"



pid_t play_sound(char *sound_cmd, char *soundfile) {
    pid_t sound_pid;
    char command[600];
    char file[600];

    sound_pid=0;
    if (strlen(sound_cmd)>3 && strlen(soundfile)>1) {
        if (last_sound_pid==0) {
            sound_pid = fork();
            if (sound_pid!=-1) {
                if(sound_pid==0) {
                    strcpy(file,SOUND_DIR);
                    strcat(file,"/");
                    strcat(file,soundfile);
                    xastir_snprintf(command, sizeof(command), "%s %s", sound_cmd, file);
                    /*fprintf(stderr,"PS%d:%s\n",sound_pid,file);*/
                    (void)system(command);  // Note we're not caring about whether it succeeded or not
                    exit(0);
                } else
                    last_sound_pid=sound_pid;
            } else
                fprintf(stderr,"Error! trying to play sound\n");
        } else {
            sound_pid=last_sound_pid;
            /*fprintf(stderr,"Sound already running\n");*/
        }
    }
    return(sound_pid);
}





int sound_done(void) {
    int done;
    int *status;

    status=NULL;
    done=0;
    if(last_sound_pid!=0) {
        if(waitpid(last_sound_pid,status,WNOHANG)==last_sound_pid) {
            done=1;
            last_sound_pid=0;
        }
    }
    return(done);
}


