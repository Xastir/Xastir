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
 * Presentation/logic separation: this header is intentionally free of
 * Motif/X11 dependencies so the controller can be unit-tested standalone.
 * Do NOT include xastir.h, main.h, globals.h, or any Xt/Xm header from here.
 */

#ifndef XASTIR_LOCATION_CONTROLLER_H
#define XASTIR_LOCATION_CONTROLLER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Size constants */
#define LOCATION_NAME_MAX   100   /* max name length (exclusive upper bound) */
#define LOCATION_COORD_MAX   20   /* lat/lon string storage */
#define LOCATION_ZOOM_MAX    20   /* zoom string storage */
#define LOCATION_LINE_MAX   256   /* max line length in locations.sys */
#define LOCATION_PATH_MAX   512   /* max filesystem path length */

/*
 * Parsed representation of one entry in config/locations.sys.
 * On-disk format: "name|lat lon zoom\n"
 * e.g.  "Home|4800.00N 01600.00E 100\n"
 */
typedef struct
{
  char name[LOCATION_NAME_MAX];
  char lat[LOCATION_COORD_MAX];
  char lon[LOCATION_COORD_MAX];
  char zoom[LOCATION_ZOOM_MAX];
} location_entry_t;

/*
 * Controller state: the two filesystem paths used by all file operations.
 * path     — config/locations.sys  (primary store)
 * tmp_path — config/locations.sys-tmp  (scratch for atomic rewrite)
 */
typedef struct
{
  char path[LOCATION_PATH_MAX];
  char tmp_path[LOCATION_PATH_MAX];
} location_controller_t;

/* Lifecycle */
void location_controller_init(location_controller_t *lc,
                               const char *path,
                               const char *tmp_path);

/*
 * Pure (no file I/O) helpers.
 * These can be tested with no filesystem setup.
 */

/* Returns 1 if name is a non-empty string shorter than LOCATION_NAME_MAX. */
int location_controller_name_valid(const char *name);

/*
 * Parse one mutable locations.sys line into *out.
 * line must point to a NUL-terminated, writable buffer; strtok modifies it.
 * Returns 1 on success, 0 if the line is malformed or too short.
 */
int location_controller_parse_line(char *line, location_entry_t *out);

/*
 * File-backed queries.
 * All functions return 0 on NULL arguments or when the file cannot be opened.
 */

/* Returns 1 if an entry with the given name exists in lc->path. */
int location_controller_name_exists(const location_controller_t *lc,
                                     const char *name);

/*
 * Finds the entry named *name in lc->path and copies it into *out.
 * Returns 1 on success, 0 if not found.
 */
int location_controller_find(const location_controller_t *lc,
                              const char *name,
                              location_entry_t *out);

/*
 * Appends a new entry to lc->path.
 * zoom must already be a decimal string (e.g. "12345").
 * Returns 1 on success, 0 on failure.
 */
int location_controller_add(const location_controller_t *lc,
                             const char *name,
                             const char *lat,
                             const char *lon,
                             const char *zoom);

/*
 * Rewrites lc->path omitting the entry named *name.
 * Uses lc->tmp_path as scratch; atomically replaces lc->path on success.
 * Returns 1 on success, 0 on failure.
 * Silently succeeds (1) if the name is not found — the file is unchanged.
 */
int location_controller_delete(const location_controller_t *lc,
                                const char *name);

#ifdef __cplusplus
}
#endif

#endif /* XASTIR_LOCATION_CONTROLLER_H */
