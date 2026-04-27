/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * locate_controller.h - Pure-C controller for locate_gui.c
 *
 * Holds all non-Motif state and logic for the "Locate Station" and
 * "Locate Place" dialogs.  No Motif/X11 headers are included here.
 */

#ifndef LOCATE_CONTROLLER_H
#define LOCATE_CONTROLLER_H

#define LOCATE_CALL_LEN        30
#define LOCATE_PLACE_LEN       50
#define LOCATE_GNIS_LEN       200
#define LOCATE_MAX_MATCHES     50
#define LOCATE_MATCH_NAME_LEN 200

typedef struct {
    /* Station search state */
    char station_call[LOCATE_CALL_LEN];
    int  station_case_sensitive;
    int  station_match_exact;

    /* Place search state */
    char place_name[LOCATE_PLACE_LEN];
    char state_name[LOCATE_PLACE_LEN];
    char county_name[LOCATE_PLACE_LEN];
    char quad_name[LOCATE_PLACE_LEN];
    char type_name[LOCATE_PLACE_LEN];
    char gnis_filename[LOCATE_GNIS_LEN];
    int  place_case_sensitive;
    int  place_match_exact;

    /* Place match results */
    int  match_count;
    char match_names[LOCATE_MAX_MATCHES][LOCATE_MATCH_NAME_LEN];
    long match_lat[LOCATE_MAX_MATCHES];
    long match_lon[LOCATE_MAX_MATCHES];
} locate_controller_t;

/* Initialise struct to safe defaults. */
void locate_controller_init(locate_controller_t *lc);

/*
 * Normalise a raw callsign into lc->station_call:
 *   - truncates to LOCATE_CALL_LEN-1
 *   - strips trailing spaces
 *   - strips trailing "-0" suffix
 * Returns 1 if station_call is non-empty after normalisation, 0 otherwise.
 * Returns 0 if lc or raw_call is NULL.
 */
int locate_controller_prepare_call(locate_controller_t *lc,
                                   const char *raw_call);

/*
 * Copy and normalise the six place-search fields into lc.
 * Each field has trailing spaces stripped.
 * NULL fields are treated as empty strings.
 * Returns 1 if place_name is non-empty after normalisation, 0 otherwise.
 * Returns 0 if lc is NULL.
 */
int locate_controller_prepare_place_query(locate_controller_t *lc,
                                          const char *place_name,
                                          const char *state_name,
                                          const char *county_name,
                                          const char *quad_name,
                                          const char *type_name,
                                          const char *gnis_filename);

/* Returns 1 if match_count > 0, 0 otherwise.  Safe with NULL lc. */
int locate_controller_has_results(const locate_controller_t *lc);

/* Resets match_count to 0.  Safe with NULL lc. */
void locate_controller_clear_results(locate_controller_t *lc);

#endif /* LOCATE_CONTROLLER_H */
