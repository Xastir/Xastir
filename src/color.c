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

#include <stdio.h>
#include <Xm/XmAll.h>

#include "xastir.h"
#include "color.h"
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"



static color_load color_choice[MAX_COLORS];
static int colors_loaded;
static int rm, gm, bm; // rgb masks
static int rs, gs, bs; // rgb shifts
int visual_depth;





/**********************************************************************************/
/* load color file                                                                */
/* load the colors to be used with Xastir                                         */
/* Return 1 if good 0 if not found or error                                       */
/**********************************************************************************/

int load_color_file(void)
{
  FILE *f;
  char temp[40];
  int r,g,b;
  char colorname[50];
  int ok,x;

  xastir_snprintf(temp, sizeof(temp), "config/xastir.rgb");
  colors_loaded=0;
  ok=1;
  f=fopen(get_data_base_dir(temp),"r");
  if (f!=NULL)
  {
    while (!feof(f) && ok)
    {
      if (fscanf(f,"%d %d %d %49s",&r,&g,&b,colorname)==4)
      {
        if (colors_loaded < MAX_COLORS)
        {
          for (x=0; x<colors_loaded; x++)
          {
            if (strcmp(color_choice[x].colorname,colorname)==0)
            {
              ok=0;
              fprintf(stderr,"Error! Duplicate color found %s\n",colorname);
            }
          }
          if (ok)
          {

            memcpy(color_choice[colors_loaded].colorname,
                   colorname,
                   sizeof(color_choice[colors_loaded].colorname));
            // Terminate string
            color_choice[colors_loaded].colorname[sizeof(color_choice[colors_loaded].colorname)-1] = '\0';

            // Do we really want to assign to unsigned short int's here?
            color_choice[colors_loaded].color.red=(unsigned short)(r*257);
            color_choice[colors_loaded].color.blue=(unsigned short)(b*257);
            color_choice[colors_loaded].color.green=(unsigned short)(g*257);
            colors_loaded++;
          }
        }
        else
        {
          ok=0;
          fprintf(stderr,"Error! MAX_COLORS has been exceeded\n");
        }
      }
    }
    (void)fclose(f);
  }
  else
  {
    ok=0;
    fprintf(stderr,"Error! can not find color file: %s\n", get_data_base_dir(temp));
  }
  return(ok);
}





/**********************************************************************************/
/* GetPixelbyName                                                                 */
/* get color for the named choice                                                 */
/* return the pixel data                                                          */
/**********************************************************************************/

Pixel GetPixelByName( Widget w, char *colorname)
{
  Display *dpy = XtDisplay(w);
  int scr = DefaultScreen(dpy);
  char warning[200];
  int i,found;

  found=-1;
  i=0;

  do
  {
    if (strcmp(color_choice[i].colorname, colorname)==0)
    {
      found=i;
    }

    i++;
  }
  while (i<colors_loaded && found <0);

  if (found >= 0)
  {

    // XFreeColors() here generates an error.  Why?
    // "BadAccess (attempt to access private resource denied)"
    // XFreeColors(dpy, cmap, &(color_choice[found].color.pixel),1,0);

    if (XAllocColor(dpy,cmap,&color_choice[found].color))
    {
      return(color_choice[found].color.pixel);
    }
    else
    {
      xastir_snprintf(warning, sizeof(warning), "Couldn't allocate color %s", colorname);
      XtWarning(warning);
      return(BlackPixel(dpy,scr));
    }
  }
  else
  {
    xastir_snprintf(warning, sizeof(warning), "Couldn't find color %s", colorname);
    XtWarning(warning);
    return(BlackPixel(dpy,scr));
  }
}





void setup_visual_info(Display* dpy, int scr)
{
  int visuals_matched, i, j;
  XVisualInfo *visual_list, *vp;
  XVisualInfo visual_template;
  rm = gm = bm = rs = gs = bs = 0;

  visual_list = XGetVisualInfo(dpy, VisualNoMask, &visual_template, &visuals_matched);
  if (visuals_matched)
  {
    if (debug_level & 16)
    {
      fprintf(stderr,"Found %d visuals\n", visuals_matched);
    }

    for (i = 0; i < visuals_matched; i++)
    {
      vp = &visual_list[i];

      if (vp->visualid == XVisualIDFromVisual(DefaultVisual(dpy, scr)))
      {
        if (vp->class == TrueColor ||
            vp->class == DirectColor)
        {
          if (vp->red_mask   == 0xf800 &&
              vp->green_mask == 0x07e0 &&
              vp->blue_mask  == 0x001f)
          {
            visual_type = RGB_565;
          }
          else if (vp->red_mask   == 0x7c00 &&
                   vp->green_mask == 0x03e0 &&
                   vp->blue_mask  == 0x001f)
          {
            visual_type = RGB_555;
          }
          else if (vp->red_mask   == 0xff0000 &&
                   vp->green_mask == 0x00ff00 &&
                   vp->blue_mask  == 0x0000ff)
          {
            visual_type = RGB_888;
          }
          else
          {
            rm = vp->red_mask;
            gm = vp->green_mask;
            bm = vp->blue_mask;
            for (j = 31; j >= 0; j--)
            {
              if (rm >= (1 << j))
              {
                rs = j - 15;
                break;
              }
            }
            for (j = 31; j >= 0; j--)
            {
              if (gm >= (1 << j))
              {
                gs = j - 15;
                break;
              }
            }
            for (j = 31; j >= 0; j--)
            {
              if (bm >= (1 << j))
              {
                bs = j - 15;
                break;
              }
            }
            visual_type = RGB_OTHER;
          }
        }
        else
        {
          visual_type = NOT_TRUE_NOR_DIRECT;
        }
        if (debug_level & 16)
        {
          fprintf(stderr,"\tID:           0x%lx,  Default\n", vp->visualid);
        }
      }
      else if (debug_level & 16)
      {
        fprintf(stderr,"\tID:           0x%lx\n", vp->visualid);
      }

      // Store color depth for use by other routines.
      visual_depth = vp->depth;

      if (debug_level & 16)
      {
        fprintf(stderr,"\tScreen:       %d\n",  vp->screen);
        fprintf(stderr,"\tDepth:        %d\n",  vp->depth);
        fprintf(stderr,"\tClass:        %d",    vp->class);
        switch (vp->class)
        {
          case StaticGray:
            fprintf(stderr,",  StaticGray\n");
            break;
          case GrayScale:
            fprintf(stderr,",  GrayScale\n");
            break;
          case StaticColor:
            fprintf(stderr,",  StaticColor\n");
            break;
          case PseudoColor:
            fprintf(stderr,",  PseudoColor\n");
            break;
          case TrueColor:
            fprintf(stderr,",  TrueColor\n");
            break;
          case DirectColor:
            fprintf(stderr,",  DirectColor\n");
            break;
          default:
            fprintf(stderr,",  ??\n");
            break;
        }
        fprintf(stderr,"\tClrmap Size:  %d\n", vp->colormap_size);
        fprintf(stderr,"\tBits per RGB: %d\n", vp->bits_per_rgb);
        fprintf(stderr,"\tRed Mask:     0x%lx\n",   vp->red_mask);
        fprintf(stderr,"\tGreen Mask:   0x%lx\n",   vp->green_mask);
        fprintf(stderr,"\tBlue Mask:    0x%lx\n\n", vp->blue_mask);
      }
    }
  }
  XFree(visual_list);
}





void pack_pixel_bits(unsigned short r, unsigned short g, unsigned short b, unsigned long* pixel)
{
  switch (visual_type)
  {
    case RGB_565:
      *pixel = (( r       & 0xf800) |
                ((g >> 5) & 0x07e0) |
                (b >> 11));
      break;
    case RGB_555:
      *pixel = (((r >> 1) & 0x7c00) |
                ((g >> 6) & 0x03e0) |
                (b >> 11));
      break;
    case RGB_888:
      *pixel = (((r << 8) & 0xff0000) |
                ( g       & 0x00ff00) |
                (b >> 8));
      break;
    case RGB_OTHER:
      if (rs >= 0)
      {
        *pixel = ((r << rs) & rm);
      }
      else
      {
        *pixel = ((r >> (-rs)) & rm);
      }
      if (gs >= 0)
      {
        *pixel |= ((g << gs) & gm);
      }
      else
      {
        *pixel |= ((g >> (-gs)) & gm);
      }
      if (bs >= 0)
      {
        *pixel |= ((b << bs) & bm);
      }
      else
      {
        *pixel |= ((b >> (-bs)) & bm);
      }
      break;
    case NOT_TRUE_NOR_DIRECT:
    default:
      break;
  }
}


