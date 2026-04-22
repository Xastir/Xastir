/*
 * cairo_text.c - Cairo-based antialiased text rendering for Xastir
 *
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
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "cairo_text.h"

#ifdef HAVE_CAIRO

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "snprintf.h"

/* ---------------------------------------------------------------------- */
/* Internal helpers                                                        */
/* ---------------------------------------------------------------------- */

/*
 * Parse a fontspec into a family name and point size understood by Cairo's
 * toy font API.  We accept two forms:
 *
 *   "FamilyName"              -> family="FamilyName", size=10
 *   "FamilyName:size=N"       -> family="FamilyName", size=N
 *
 * XLFD strings (starting with '-') are also accepted; we extract the
 * family field (position 2) and the pixel-size field (position 7).
 * If parsing fails we fall back to "sans" at 10pt.
 */
static void parse_fontspec(const char *fontspec,
                            char *family, int family_len,
                            double *size)
{
  *size = 10.0;
  xastir_snprintf(family, family_len, "%s", "sans");

  if (!fontspec || fontspec[0] == '\0')
    return;

  if (fontspec[0] == '-')
  {
    /* XLFD: -fndry-family-weight-slant-swidth-adstyl-pxlsz-... */
    /* We want field 2 (family) and field 7 (pixel size). */
    char buf[256];
    xastir_snprintf(buf, sizeof(buf), "%s", fontspec);

    char *fields[14];
    int nf = 0;
    char *p = buf + 1; /* skip leading '-' */
    fields[nf++] = p;
    while (*p && nf < 14)
    {
      if (*p == '-')
      {
        *p = '\0';
        fields[nf++] = p + 1;
      }
      p++;
    }
    /* field[1] = family, field[6] = pixel size */
    if (nf > 2 && fields[1][0] != '\0' && fields[1][0] != '*')
      xastir_snprintf(family, family_len, "%s", fields[1]);

    if (nf > 7 && fields[6][0] != '\0' && fields[6][0] != '*')
    {
      int px = atoi(fields[6]);
      if (px > 0)
        *size = (double)px;
    }
    return;
  }

  /* "FamilyName" or "FamilyName:size=N" */
  const char *colon = strchr(fontspec, ':');
  if (!colon)
  {
    xastir_snprintf(family, family_len, "%s", fontspec);
    return;
  }

  /* Copy family part */
  int flen = (int)(colon - fontspec);
  if (flen >= family_len) flen = family_len - 1;
  strncpy(family, fontspec, flen);
  family[flen] = '\0';

  /* Scan for size=N */
  const char *sz = strstr(colon, "size=");
  if (sz)
  {
    double v = atof(sz + 5);
    if (v > 0)
      *size = v;
  }
}

/*
 * Convert an X11 pixel value to Cairo RGBA [0..1] components by querying
 * the colormap.  Falls back to white on error.
 */
static void pixel_to_rgba(Display *dpy, Colormap cmap,
                           unsigned long pixel,
                           double *r, double *g, double *b)
{
  XColor xc;
  xc.pixel = pixel;
  if (XQueryColor(dpy, cmap, &xc))
  {
    *r = xc.red   / 65535.0;
    *g = xc.green / 65535.0;
    *b = xc.blue  / 65535.0;
  }
  else
  {
    *r = 1.0; *g = 1.0; *b = 1.0;
  }
}

/* ---------------------------------------------------------------------- */
/* Internal drawing primitive                                               */
/* ---------------------------------------------------------------------- */

/*
 * Draw text at (x,y) with optional outline.
 *
 * Outline is painted as 4 copies offset by ±0.5px in the cardinal directions
 * (cairo_show_text, which is confirmed to render correctly on Xlib surfaces).
 * Then the foreground text is painted on top.  Half-pixel offsets keep the
 * halo narrow so copies don't merge into a solid rectangle.
 *
 * NOTE: cairo_text_path() + fill was tried but produces no output when using
 * the toy font API with CAIRO_FILL_RULE_WINDING: glyph paths from FreeType
 * have counter-clockwise exterior contours, so Cairo's winding counter hits
 * zero at the interior and the fill is a no-op.  cairo_show_text() does not
 * have this issue.
 */
static void cairo_draw_text_at(cairo_t *cr,
                                int x, int y,
                                float angle_deg,
                                const char *text,
                                const char *family,
                                double size,
                                int align,
                                double r, double g, double b,
                                int draw_outline,
                                double or_, double og, double ob)
{
  cairo_text_extents_t te;

  cairo_select_font_face(cr, family,
                         CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, size);

  cairo_text_extents(cr, text, &te);

  double angle_rad = angle_deg * M_PI / 180.0;

  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, -angle_rad);

  double tx = 0.0;
  if (align == BCENTRE)
    tx = -te.width / 2.0 - te.x_bearing;
  else if (align == BRIGHT)
    tx = -te.width - te.x_bearing;
  else /* BLEFT */
    tx = -te.x_bearing;

  if (draw_outline)
  {
    /* 4-copy cardinal outline at ±0.5px — thin halo, won't merge into
     * a solid block even at large font sizes. */
    static const double od = 0.75;
    static const double offsets[4][2] = {{od,0},{-od,0},{0,od},{0,-od}};
    cairo_set_source_rgb(cr, or_, og, ob);
    for (int i = 0; i < 4; i++)
    {
      cairo_move_to(cr, tx + offsets[i][0], offsets[i][1]);
      cairo_show_text(cr, text);
    }
  }

  /* Foreground text */
  cairo_set_source_rgb(cr, r, g, b);
  cairo_move_to(cr, tx, 0.0);
  cairo_show_text(cr, text);

  cairo_restore(cr);
}

/* ---------------------------------------------------------------------- */
/* Public API                                                              */
/* ---------------------------------------------------------------------- */

void xastir_cairo_draw_text(
    Display      *dpy,
    Pixmap        target,
    int           x,
    int           y,
    float         angle_deg,
    const char   *text,
    const char   *fontspec,
    unsigned long fg_pixel,
    int           draw_outline,
    unsigned long outline_pixel,
    int           align)
{
  if (!text || text[0] == '\0')
    return;

  /* Get pixmap geometry so we can create a correctly-sized surface. */
  Window root_ret;
  int x_ret, y_ret;
  unsigned int width, height, border, depth;
  if (!XGetGeometry(dpy, target, &root_ret, &x_ret, &y_ret,
                    &width, &height, &border, &depth))
  {
    fprintf(stderr, "xastir_cairo_draw_text: XGetGeometry failed\n");
    return;
  }

  int screen = DefaultScreen(dpy);
  Visual *visual = DefaultVisual(dpy, screen);

  Colormap cmap = DefaultColormap(dpy, screen);

  cairo_surface_t *surface =
    cairo_xlib_surface_create(dpy, target, visual,
                               (int)width, (int)height);
  if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
  {
    fprintf(stderr, "xastir_cairo_draw_text: cairo_xlib_surface_create failed\n");
    if (surface) cairo_surface_destroy(surface);
    return;
  }

  cairo_t *cr = cairo_create(surface);
  if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS)
  {
    fprintf(stderr, "xastir_cairo_draw_text: cairo_create failed\n");
    if (cr) cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return;
  }

  /* CAIRO_ANTIALIAS_GRAY: grayscale AA.  Safe on any bit depth and does
   * not trigger LCD subpixel rendering, which would write color-fringe
   * data into the pixmap's R/G/B channels and corrupt colours. */
  cairo_set_antialias(cr, CAIRO_ANTIALIAS_GRAY);

  char family[128];
  double size;
  parse_fontspec(fontspec, family, sizeof(family), &size);

  double fr, fg_, fb;
  pixel_to_rgba(dpy, cmap, fg_pixel, &fr, &fg_, &fb);

  if (draw_outline)
  {
    double or_, og, ob;
    pixel_to_rgba(dpy, cmap, outline_pixel, &or_, &og, &ob);
    /* Single call: vector stroke+fill — no 8-copy halo merge */
    cairo_draw_text_at(cr, x, y, angle_deg, text, family, size,
                       align, fr, fg_, fb, 1, or_, og, ob);
  }
  else
  {
    cairo_draw_text_at(cr, x, y, angle_deg, text, family, size,
                       align, fr, fg_, fb, 0, 0.0, 0.0, 0.0);
  }

  /* Flush all pending Cairo drawing to the X server before releasing
   * the surface so that the pixmap is fully updated. */
  cairo_surface_flush(surface);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}


int xastir_cairo_text_width(const char *text, const char *fontspec)
{
  if (!text || text[0] == '\0')
    return 0;

  char family[128];
  double size;
  parse_fontspec(fontspec, family, sizeof(family), &size);

  /* Use an image surface - no display needed */
  cairo_surface_t *surface =
    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t *cr = cairo_create(surface);

  cairo_select_font_face(cr, family,
                         CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, size);

  cairo_text_extents_t te;
  cairo_text_extents(cr, text, &te);
  int width = (int)(te.width + 0.5);

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  return width;
}


int xastir_cairo_text_height(const char *fontspec)
{
  char family[128];
  double size;
  parse_fontspec(fontspec, family, sizeof(family), &size);

  cairo_surface_t *surface =
    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t *cr = cairo_create(surface);

  cairo_select_font_face(cr, family,
                         CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, size);

  cairo_font_extents_t fe;
  cairo_font_extents(cr, &fe);
  int height = (int)(fe.ascent + fe.descent + 0.5);

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  return height;
}

#endif /* HAVE_CAIRO */
