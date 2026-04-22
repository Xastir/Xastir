/*
 * cairo_text.h - Cairo-based antialiased text rendering for Xastir
 *
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef CAIRO_TEXT_H
#define CAIRO_TEXT_H

/* Alignment constants (match rotated.h values) */
#ifndef NONE
  #define NONE     0
#endif
#ifndef BLEFT
  #define BLEFT    7
  #define BCENTRE  8
  #define BRIGHT   9
#endif

#ifdef HAVE_CAIRO

#include <X11/Xlib.h>

/*
 * Draw antialiased (optionally rotated) text onto an X11 Pixmap using Cairo.
 *
 *   dpy          - X11 Display
 *   target       - Pixmap to draw into
 *   visual       - Visual associated with the pixmap (use DefaultVisual)
 *   cmap         - Colormap for pixel-to-RGB conversion (use DefaultColormap)
 *   x, y         - Anchor point in pixmap coordinates
 *   angle_deg    - Rotation in degrees; 0 = horizontal, positive = CCW
 *                  (same convention as xvertext / the old rotated.c)
 *   text         - NUL-terminated string to draw
 *   fontspec     - Either an XLFD string ("-adobe-helvetica-medium-r-normal--12-*...")
 *                  or a simple spec "FamilyName" or "FamilyName:size=N"
 *   fg_pixel     - X11 pixel value for text foreground
 *   draw_outline - Non-zero to draw a 1-pixel outline around the text
 *   outline_pixel- X11 pixel value for outline color (used when draw_outline != 0)
 *   align        - BLEFT, BCENTRE, or BRIGHT
 */
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
    int           align);

/*
 * Return the pixel width of 'text' rendered with 'fontspec'.
 * Uses a temporary off-screen image surface so no display round-trip
 * is needed beyond what Cairo already does.
 */
int xastir_cairo_text_width(
    const char *text,
    const char *fontspec);

/*
 * Return the pixel height (ascent + descent) of text rendered
 * with 'fontspec'.
 */
int xastir_cairo_text_height(
    const char *fontspec);

#endif /* HAVE_CAIRO */

#endif /* CAIRO_TEXT_H */
