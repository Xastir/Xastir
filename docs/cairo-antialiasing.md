# Cairo Antialiased Text Rendering

## Goal

Replace Xastir's X11 core-font text rendering (XFontStruct / XDrawString / xvertext 5.0
stipple-rotation) with Cairo so that all on-screen text — map grid labels, station
callsigns, speed/course/wx overlays — renders with smooth antialiasing.

## Branch

`master` (fd953a61 "Try to fix that badge")

## Status

| Area | Status |
|---|---|
| Build system (configure.ac + Makefile.am) | ✅ Done |
| `src/cairo_text.c` and `src/cairo_text.h` | ✅ Done |
| Map rotation labels (`maps.c`) | ✅ Done – renders AA |
| Station callsigns / overlays (`draw_symbols.c`) | ✅ Done – renders AA |
| Font string defaults (`xa_config.c`) | ✅ Done |
| **Border grid coordinate labels** | ✅ Done – renders AA |
| **Statusbar text widgets (`main.c`)** | ⚠️ Partial – Cairo renderer works, inset look restored, sizing policy still needs work |
| **Interface activity indicator (`iface_da`)** | ⚠️ Partial – aligned better, but still buggy |

---

## Files Changed

### `configure.ac`
Added before the `use_curl` block:

```m4
use_cairo=no
AC_ARG_WITH(cairo,
  [AS_HELP_STRING([--without-cairo],[disable Cairo antialiased text])],
  cairo_desired=$withval, cairo_desired=yes)
if test "${cairo_desired}" = "yes"; then
  PKG_CHECK_MODULES([CAIRO], [cairo-xlib],
    [use_cairo=yes
     CPPFLAGS="$CPPFLAGS $CAIRO_CFLAGS"
     LIBS="$LIBS $CAIRO_LIBS"
     AC_DEFINE(HAVE_CAIRO,1,[Cairo available])],
    [AC_MSG_WARN([cairo-xlib not found; building without Cairo])])
fi
```

### `src/Makefile.am`
```makefile
XASTIR_SRC += cairo_text.c cairo_text.h
```

### `src/cairo_text.h` (new file)
Public API:

```c
/* Alignment constants (same as xvertext: BLEFT, BCENTRE, BRIGHT) */
#define BLEFT   0
#define BCENTRE 1
#define BRIGHT  2

void xastir_cairo_draw_text(Display *dpy, Pixmap target,
    int x, int y, float angle_deg,
    const char *text, const char *fontspec,
    unsigned long fg_pixel,
    int draw_outline, unsigned long outline_pixel,
    int align);

int  xastir_cairo_text_width (const char *text, const char *fontspec);
int  xastir_cairo_text_height(const char *fontspec);
```

### `src/cairo_text.c` (new file)
Key implementation notes:
- Uses `cairo_xlib_surface_create()` wrapping Xastir's X11 `Pixmap` targets.
- `DefaultVisual` / `DefaultColormap` — pixmap depth confirmed 24-bit, matching
  default visual (depth mismatch was ruled out via debug logging).
- Font parsing: accepts both `"family:size=N"` (Fontconfig) and XLFD strings starting
  with `-`.
- Text normalization: validates UTF-8 and converts legacy Latin-1 strings to UTF-8
  before measuring or drawing. This is required because coordinate-format helpers still
  emit `0xB0` for the degree symbol.
- Antialiasing: `CAIRO_ANTIALIAS_GRAY` (not BEST — avoids LCD subpixel color fringing).
- Outline approach: 4 copies of `cairo_show_text()` at ±0.75 px cardinal offsets, then
  the foreground text on top. (The `cairo_text_path()` + `cairo_fill()` approach was
  tried but produced no output — glyph path winding rules cause zero fill coverage with
  the toy font API.)
- UI text box helper: `xastir_cairo_draw_text_box()` now draws a Motif-like inset frame
  using the widget's `XmNtopShadowColor`, `XmNbottomShadowColor`, and
  `XmNshadowThickness`, then clips/centers the Cairo text inside the inner rect.
- `cairo_surface_flush()` called before `cairo_surface_destroy()`.

### `src/main.c`
- Added a Cairo-specific statusbar renderer for the status text widgets using
  `xmDrawingAreaWidgetClass` plus expose/resize callbacks.
- The Cairo status widgets now store per-widget text and a minimum width in
  `cairo_status_widget_data_t`.
- Statusbar text widgets use `XmSHADOW_IN` again so the old inset look is back.
- `XmNhighlightThickness` is forced to `0` for the Cairo status widgets to avoid extra
  Motif chrome making them taller than the parent bar.
- The common height is now driven by `CAIRO_STATUS_WIDGET_HEIGHT`
  (`xastir_cairo_text_height(FONT_SYSTEM) + 4`) instead of the earlier looser height.
- The interface activity widget (`iface_da`) had its extra `XmNbottomOffset` removed so
  it aligns better with the rest of the status bar.

### `src/maps.c`
`draw_rotated_label_text_common()` — the `#ifdef HAVE_CAIRO` path now makes **two
separate** `xastir_cairo_draw_text()` calls:
1. Outline pass: `fg = outline_color`, `draw_outline = 1` (4 offset copies).
2. Foreground pass: `fg = text_color`, `draw_outline = 0`.

Width/height metric helpers (`get_rotated_label_text_length_pixels()`,
`get_rotated_label_text_height_pixels()`) use `xastir_cairo_text_width/height()`.

### `src/xa_config.c`
Default font name strings switched to `"sans:size=N"` under `#ifdef HAVE_CAIRO` for
`FONT_TINY` through `FONT_BORDER` and `FONT_ATV_ID`. XLFD strings in the `#else` branch
for non-Cairo builds.

### `src/draw_symbols.c`
- `draw_nice_string()`: replaced `XQueryFont` / `XDrawString` body with Cairo calls
  using `FONT_STATION`. Style 0 (outline) uses `draw_outline=1`; styles 1/2 (box
  backgrounds) keep their `XFillRectangle` and then call Cairo for text; style 3
  (shadow) does an offset copy then foreground.
- `get_text_width()`: returns `xastir_cairo_text_width(text,
  rotated_label_fontname[FONT_STATION])`.
- `draw_symbol()`: font metrics preamble (`font_width`, `font_height`) replaced with
  `xastir_cairo_text_width("0", ...)` / `xastir_cairo_text_height(...)`.

---

## Resolved Bug: Border Grid Labels Were Invisible

### Symptom
The four fat white `XDrawLine` strips that form the map border render correctly. But the
coordinate tick labels that should appear on those white strips were not visible. The
border and grid rendered, but the text did not.

### Root Cause
The coordinate-format helpers in `util.c` (`convert_lat_l2s()` / `convert_lon_l2s()`)
emit the degree symbol as the single byte `0xB0` in Latin-1 / ISO-8859-1.

That worked with the legacy X11 text path (`XDrawString` / xvertext), which treated text
as raw bytes. Cairo's toy text API (`cairo_show_text`) expects UTF-8. As a result, the
border grid labels were being fed invalid text whenever the selected coordinate format
included a degree symbol, so the labels failed to render even though other Cairo text
(station callsigns, range scale, etc.) still worked.

### Fix Applied
`src/cairo_text.c` now:
1. Validates incoming text as UTF-8.
2. Converts non-UTF-8 strings from Latin-1 to UTF-8 before calling Cairo.
3. Uses the normalized UTF-8 text for both metrics (`cairo_text_extents`) and drawing.

This preserves compatibility with Xastir's legacy string formatting while allowing Cairo
to render the existing border coordinate labels correctly with antialiasing.

### Relevant Call Chains
```
draw_grid(w)
  → draw_complete_lat_lon_grid(w)      [for lat/lon coordinate system]
      → draw_rotated_label_text_to_target(w, 270, x, screen_height, ...,
            colors[0x09], grid_label, FONT_BORDER, pixmap_final,
            outline_border_labels, colors[outline_border_labels_color=0x20])
          → draw_rotated_label_text_common(w, rotation, x, y, ...,
                color=colors[0x09],   ← fg
                outline_bg_color=colors[0x20])  ← outline
              → xastir_cairo_draw_text(..., fg=colors[0x09], outline=colors[0x20])
                  → normalize_text_for_cairo(...)
                      → Latin-1 `0xB0` degree symbol converted to UTF-8
```

---

## Current Statusbar Issues

### 1. Interface Indicator Still Buggy
The connection/interface activity area (`iface_da`, the row of green/yellow status
blocks) is still not in a fully clean state.

What changed:
- It no longer uses the extra bottom offset that made it visibly taller than the rest
  of the status bar.

What is still wrong:
- It still behaves like a special-case widget with its own fixed `20x20` symbol drawing
  path, so visually it does not yet integrate cleanly with the Cairo statusbar widgets.
- It should still be considered buggy / not fully resolved.

### 2. Statusbar Boxes Reflow Too Aggressively
The Cairo statusbar text boxes currently resize themselves from the current text width.
That avoids immediate text cropping, but it also means they keep reflowing as the text
changes.

Current behavior:
- Each widget grows and shrinks according to the current text, bounded by its original
  minimum width.

Desired behavior:
- The statusbar rectangles should feel fixed and stable in normal operation.
- They should be wide enough to hold a reasonable amount of text.
- If a widget has already expanded for a long string, it should probably not shrink again
  just because the next string is shorter.

Recommended follow-up:
- Replace the current "always fit current text" sizing with either:
  1. fixed widths chosen from realistic worst-case content, or
  2. grow-only widths that expand when needed but do not shrink afterward.

## Build Instructions

```bash
cd /home/hjf/dev/Xastir/build
make -j$(nproc)
src/xastir
```

To reconfigure from scratch:
```bash
cd /home/hjf/dev/Xastir/build
../configure
make -j$(nproc)
```
