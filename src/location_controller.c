/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

/*
 * location_controller.c — logic kernel extracted from location_gui.c.
 *
 * Intentionally free of Motif/X11 and Xastir-global dependencies.
 * Uses only standard C (stdio, string, stdlib).  This keeps the unit
 * test binary dependency-free — no stubs file required.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "location_controller.h"


/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

void location_controller_init(location_controller_t *lc,
                               const char *path,
                               const char *tmp_path)
{
  if (lc == NULL)
  {
    return;
  }

  memset(lc, 0, sizeof(*lc));

  if (path != NULL)
  {
    strncpy(lc->path, path, sizeof(lc->path) - 1);
  }

  if (tmp_path != NULL)
  {
    strncpy(lc->tmp_path, tmp_path, sizeof(lc->tmp_path) - 1);
  }
}


/* ------------------------------------------------------------------ */
/* Pure helpers (no file I/O)                                          */
/* ------------------------------------------------------------------ */

int location_controller_name_valid(const char *name)
{
  size_t len;

  if (name == NULL)
  {
    return 0;
  }

  len = strlen(name);
  return (len > 0 && len < LOCATION_NAME_MAX) ? 1 : 0;
}


/*
 * Parse one mutable line from locations.sys.
 * Format: "name|lat lon zoom"
 * strtok modifies the buffer in place.  The caller must pass a writable
 * copy of the line if the original must be preserved.
 */
int location_controller_parse_line(char *line, location_entry_t *out)
{
  char *name_tok;
  char *pos_tok;
  char lat[LOCATION_COORD_MAX];
  char lon[LOCATION_COORD_MAX];
  char zoom[LOCATION_ZOOM_MAX];

  if (line == NULL || out == NULL)
  {
    return 0;
  }

  /* Minimum meaningful line: at least 9 chars (e.g. "ab|x y 1") */
  if (strlen(line) <= 8)
  {
    return 0;
  }

  name_tok = strtok(line, "|");
  if (name_tok == NULL)
  {
    return 0;
  }

  pos_tok = strtok(NULL, "|");
  if (pos_tok == NULL)
  {
    return 0;
  }

  if (sscanf(pos_tok, "%19s %19s %19s", lat, lon, zoom) != 3)
  {
    return 0;
  }

  strncpy(out->name, name_tok, sizeof(out->name) - 1);
  out->name[sizeof(out->name) - 1] = '\0';

  strncpy(out->lat, lat, sizeof(out->lat) - 1);
  out->lat[sizeof(out->lat) - 1] = '\0';

  strncpy(out->lon, lon, sizeof(out->lon) - 1);
  out->lon[sizeof(out->lon) - 1] = '\0';

  strncpy(out->zoom, zoom, sizeof(out->zoom) - 1);
  out->zoom[sizeof(out->zoom) - 1] = '\0';

  return 1;
}


/* ------------------------------------------------------------------ */
/* Internal helper: read one line from f, strip CR/LF, into buf.      */
/* Returns NULL at EOF or on error.                                    */
/* ------------------------------------------------------------------ */
static char *read_stripped_line(FILE *f, char *buf, size_t bufsz)
{
  if (fgets(buf, (int)bufsz, f) == NULL)
  {
    return NULL;
  }
  buf[strcspn(buf, "\r\n")] = '\0';
  return buf;
}


/* ------------------------------------------------------------------ */
/* File-backed queries                                                  */
/* ------------------------------------------------------------------ */

int location_controller_name_exists(const location_controller_t *lc,
                                     const char *name)
{
  FILE *f;
  char buf[LOCATION_LINE_MAX];
  char line[LOCATION_LINE_MAX];
  char *tok;

  if (lc == NULL || name == NULL)
  {
    return 0;
  }

  f = fopen(lc->path, "r");
  if (f == NULL)
  {
    return 0;
  }

  while (read_stripped_line(f, buf, sizeof(buf)) != NULL)
  {
    if (strlen(buf) <= 8)
    {
      continue;
    }

    strncpy(line, buf, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    tok = strtok(line, "|");
    if (tok != NULL && strcmp(tok, name) == 0)
    {
      fclose(f);
      return 1;
    }
  }

  fclose(f);
  return 0;
}


int location_controller_find(const location_controller_t *lc,
                              const char *name,
                              location_entry_t *out)
{
  FILE *f;
  char buf[LOCATION_LINE_MAX];
  char line[LOCATION_LINE_MAX];
  location_entry_t entry;

  if (lc == NULL || name == NULL || out == NULL)
  {
    return 0;
  }

  f = fopen(lc->path, "r");
  if (f == NULL)
  {
    return 0;
  }

  while (read_stripped_line(f, buf, sizeof(buf)) != NULL)
  {
    if (strlen(buf) <= 8)
    {
      continue;
    }

    strncpy(line, buf, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    if (location_controller_parse_line(line, &entry))
    {
      if (strcmp(entry.name, name) == 0)
      {
        *out = entry;
        fclose(f);
        return 1;
      }
    }
  }

  fclose(f);
  return 0;
}


/* ------------------------------------------------------------------ */
/* File mutations                                                       */
/* ------------------------------------------------------------------ */

int location_controller_add(const location_controller_t *lc,
                             const char *name,
                             const char *lat,
                             const char *lon,
                             const char *zoom)
{
  FILE *f;

  if (lc == NULL || name == NULL || lat == NULL || lon == NULL || zoom == NULL)
  {
    return 0;
  }

  f = fopen(lc->path, "a");
  if (f == NULL)
  {
    return 0;
  }

  fprintf(f, "%s|%s %s %s\n", name, lat, lon, zoom);
  fclose(f);
  return 1;
}


int location_controller_delete(const location_controller_t *lc,
                                const char *name)
{
  FILE *f;
  FILE *fout;
  char buf[LOCATION_LINE_MAX];
  char line[LOCATION_LINE_MAX];
  location_entry_t entry;

  if (lc == NULL || name == NULL)
  {
    return 0;
  }

  if (lc->path[0] == '\0' || lc->tmp_path[0] == '\0')
  {
    return 0;
  }

  f = fopen(lc->path, "r");
  if (f == NULL)
  {
    return 0;
  }

  fout = fopen(lc->tmp_path, "w");
  if (fout == NULL)
  {
    fclose(f);
    return 0;
  }

  while (read_stripped_line(f, buf, sizeof(buf)) != NULL)
  {
    if (strlen(buf) <= 8)
    {
      continue;
    }

    strncpy(line, buf, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    if (location_controller_parse_line(line, &entry))
    {
      if (strcmp(entry.name, name) != 0)
      {
        fprintf(fout, "%s|%s %s %s\n", entry.name, entry.lat, entry.lon, entry.zoom);
      }
    }
  }

  fclose(f);
  fclose(fout);

  if (rename(lc->tmp_path, lc->path) != 0)
  {
    return 0;
  }

  return 1;
}
