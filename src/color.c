/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2002  The Xastir Group
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

#include <stdio.h>
#include <Xm/XmAll.h>

#include "xastir.h"
#include "color.h"
#include "xa_config.h"


static color_load color_choice[MAX_COLORS];
static int colors_loaded;

/**********************************************************************************/
/* load color file                                                                */
/* load the colors to be used with Xastir                                         */
/* Return 1 if good 0 if not found or error                                       */
/**********************************************************************************/

int load_color_file(void) {
    FILE *f;
    char temp[40];
    int r,g,b;
    char colorname[50];
    int ok,x;

    xastir_snprintf(temp, sizeof(temp), "config/xastir.rgb");
    colors_loaded=0;
    ok=1;
    f=fopen(get_data_base_dir(temp),"r");
    if (f!=NULL) {
        while (!feof(f) && ok) {
            if (fscanf(f,"%d %d %d %49s",&r,&g,&b,colorname)==4) {
                if (colors_loaded < MAX_COLORS) {
                    for (x=0; x<colors_loaded;x++) {
                        if (strcmp(color_choice[x].colorname,colorname)==0) {
                            ok=0;
                            printf("Error! Duplicate color found %s\n",colorname);
                        }
                    }
                    if (ok) {
                        strcpy(color_choice[colors_loaded].colorname,colorname);
// Do we really want to assign to unsigned short int's here?
                        color_choice[colors_loaded].color.red=(unsigned short)(r*257);
                        color_choice[colors_loaded].color.blue=(unsigned short)(b*257);
                        color_choice[colors_loaded].color.green=(unsigned short)(g*257);
                        colors_loaded++;
                    }
                } else {
                    ok=0;
                    printf("Error! MAX_COLORS has been exceeded\n");
                }
            }
        }
        (void)fclose(f);
    } else {
        ok=0;
        printf("Error! can not find color file: %s\n", get_data_base_dir(temp));
    }
    return(ok);
}

/**********************************************************************************/
/* GetPixelbyName                                                                 */
/* get color for the named choice                                                 */
/* return the pixel data                                                          */
/**********************************************************************************/

Pixel GetPixelByName( Widget w, char *colorname) {
    Display *dpy = XtDisplay(w);
    int scr = DefaultScreen(dpy);
/*    Colormap cmap=DefaultColormap(dpy,scr);  KD6ZWR - now set in main() */
    /*XColor color, ignore;*/
    char warning[200];
    int i,found;

    found=-1;
    i=0;
    do {
        if (strcmp(color_choice[i].colorname, colorname)==0)
            found=i;

        i++;
    } while (i<colors_loaded && found <0);

    if (found >= 0) {
        if (XAllocColor(dpy,cmap,&color_choice[found].color))
            return(color_choice[found].color.pixel);
        else {
            xastir_snprintf(warning, sizeof(warning), "Couldn't allocate color %s", colorname);
            XtWarning(warning);
            return(BlackPixel(dpy,scr));
        }
    } else {
        xastir_snprintf(warning, sizeof(warning), "Couldn't find color %s", colorname);
        XtWarning(warning);
        return(BlackPixel(dpy,scr));
    }
}
