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
 * geocoder_controller.h — Motif-free kernel extracted from geocoder_gui.c.
 *
 * Owns:
 *   - The country code option table (ISO 3166-1 alpha-2 labels + values)
 *   - Country lookup helpers used by both the search and config dialogs
 *   - Configuration normalization (empty server URL → restore default)
 *
 * Does NOT own:
 *   - nominatim_server_url / nominatim_user_email / nominatim_country_default
 *     (those are owned by nominatim.c; declared in main.h)
 *   - search results (geocode_result_list; owned by geocoder_gui.c)
 *   - Any Motif / X11 headers
 */

#ifndef XASTIR_GEOCODER_CONTROLLER_H
#define XASTIR_GEOCODER_CONTROLLER_H

#include <stddef.h>   /* size_t */

/* ------------------------------------------------------------------
 * Default Nominatim server URL
 * ------------------------------------------------------------------ */
#define GEOCODER_NOMINATIM_DEFAULT_URL "https://nominatim.openstreetmap.org"

/* ------------------------------------------------------------------
 * Controller struct
 *
 * Lightweight at this stage — serves as the anchor for future state
 * migration. The GUI syncs result_count into this after each search
 * so that the controller can gate "Go To / Mark" sensitivity without
 * touching a Widget.
 * ------------------------------------------------------------------ */
typedef struct
{
    int result_count;  /* mirror of current_results.count after a search */
} geocoder_controller_t;

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

/**
 * Zero-fill the controller struct and set defaults.
 * Safe to call with NULL (no-op).
 */
void geocoder_controller_init(geocoder_controller_t *gc);

/* ------------------------------------------------------------------
 * Country option table access
 *
 * The table is a compile-time constant array of (label, value) pairs
 * that maps human-readable country names to ISO 3166-1 alpha-2 codes.
 * The sentinel entry has label == NULL.
 *
 * All index arguments are 0-based.  Passing an out-of-range index
 * returns NULL.  find_country_index returns a 1-based index to match
 * Motif XmList conventions (0 means "not found").
 * ------------------------------------------------------------------ */

/** Number of entries in the country table (excluding the sentinel). */
int geocoder_controller_country_count(void);

/** Return the display label at 0-based index idx, or NULL if out of range. */
const char *geocoder_controller_country_label(int idx);

/** Return the ISO code value at 0-based index idx, or NULL if out of range. */
const char *geocoder_controller_country_value(int idx);

/**
 * Search the country table for an entry whose value matches @p value
 * (case-sensitive).  Returns the 1-based position for XmListSelectPos,
 * or 0 if not found.  NULL or empty @p value always returns 0.
 */
int geocoder_controller_find_country_index(const char *value);

/* ------------------------------------------------------------------
 * Country code resolution
 * ------------------------------------------------------------------ */

/**
 * Given the display label selected in a combo box and (optionally) the
 * text in a "Custom Code" text field, resolve the actual country code
 * string to use for a Nominatim query.
 *
 * Logic (mirrors get_selected_country_code without Motif calls):
 *   1. Find the entry in the country table whose label matches @p label.
 *   2. If the matched value is "custom", use @p custom_text as the code
 *      and set *is_custom = 1 (if is_custom is non-NULL).
 *   3. Otherwise, use the table value directly.
 *   4. If the code is empty (e.g., "None (Worldwide)"), output nothing.
 *
 * Returns 1 and writes to out_code if a non-empty code was resolved.
 * Returns 0 if no code applies (no match, empty code, no custom text).
 *
 * @p out_code is always NUL-terminated on return (even on failure).
 * @p label and @p custom_text may be NULL (treated as empty strings).
 */
int geocoder_controller_label_to_code(const char *label,
                                       const char *custom_text,
                                       char *out_code,
                                       size_t code_sz,
                                       int *is_custom);

/* ------------------------------------------------------------------
 * Configuration helpers
 * ------------------------------------------------------------------ */

/**
 * If @p url is empty (first byte is '\0'), write the default Nominatim
 * URL (GEOCODER_NOMINATIM_DEFAULT_URL) into @p url.
 *
 * Returns 1 if the URL was restored (was empty), 0 if left unchanged.
 * Safe to call with NULL (returns 0).
 */
int geocoder_controller_normalize_server_url(char *url, size_t url_sz);

/**
 * Return 1 if result_count > 0 (buttons should be sensitive).
 * Safe to call with NULL (returns 0).
 */
int geocoder_controller_has_results(const geocoder_controller_t *gc);

#endif  /* XASTIR_GEOCODER_CONTROLLER_H */
