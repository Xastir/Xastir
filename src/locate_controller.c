/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * locate_controller.c - Pure-C controller for locate_gui.c
 *
 * No Motif/X11 headers are included here; all logic is pure C.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "locate_controller.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Private helpers                                                      */
/* ------------------------------------------------------------------ */

/* Strip trailing ASCII spaces in-place. */
static void strip_trailing_spaces(char *s)
{
    int len;
    if (!s)
        return;
    len = (int)strlen(s);
    while (len > 0 && s[len - 1] == ' ')
        s[--len] = '\0';
}

/*
 * Strip trailing "-0" suffix in-place.
 * E.g. "W1AW-0" becomes "W1AW".
 */
static void strip_trailing_dash_zero(char *s)
{
    int len;
    if (!s)
        return;
    len = (int)strlen(s);
    if (len >= 2 && s[len - 1] == '0' && s[len - 2] == '-')
        s[len - 2] = '\0';
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void locate_controller_init(locate_controller_t *lc)
{
    if (!lc)
        return;
    memset(lc, 0, sizeof(*lc));
}

int locate_controller_prepare_call(locate_controller_t *lc,
                                   const char *raw_call)
{
    if (!lc || !raw_call)
        return 0;

    strncpy(lc->station_call, raw_call, LOCATE_CALL_LEN - 1);
    lc->station_call[LOCATE_CALL_LEN - 1] = '\0';

    strip_trailing_spaces(lc->station_call);
    strip_trailing_dash_zero(lc->station_call);

    return (lc->station_call[0] != '\0') ? 1 : 0;
}

int locate_controller_prepare_place_query(locate_controller_t *lc,
                                          const char *place_name,
                                          const char *state_name,
                                          const char *county_name,
                                          const char *quad_name,
                                          const char *type_name,
                                          const char *gnis_filename)
{
    if (!lc)
        return 0;

    strncpy(lc->place_name,    place_name    ? place_name    : "", LOCATE_PLACE_LEN - 1);
    strncpy(lc->state_name,    state_name    ? state_name    : "", LOCATE_PLACE_LEN - 1);
    strncpy(lc->county_name,   county_name   ? county_name   : "", LOCATE_PLACE_LEN - 1);
    strncpy(lc->quad_name,     quad_name     ? quad_name     : "", LOCATE_PLACE_LEN - 1);
    strncpy(lc->type_name,     type_name     ? type_name     : "", LOCATE_PLACE_LEN - 1);
    strncpy(lc->gnis_filename, gnis_filename ? gnis_filename : "", LOCATE_GNIS_LEN  - 1);

    lc->place_name[LOCATE_PLACE_LEN - 1]    = '\0';
    lc->state_name[LOCATE_PLACE_LEN - 1]    = '\0';
    lc->county_name[LOCATE_PLACE_LEN - 1]   = '\0';
    lc->quad_name[LOCATE_PLACE_LEN - 1]     = '\0';
    lc->type_name[LOCATE_PLACE_LEN - 1]     = '\0';
    lc->gnis_filename[LOCATE_GNIS_LEN - 1]  = '\0';

    strip_trailing_spaces(lc->place_name);
    strip_trailing_spaces(lc->state_name);
    strip_trailing_spaces(lc->county_name);
    strip_trailing_spaces(lc->quad_name);
    strip_trailing_spaces(lc->type_name);

    return (lc->place_name[0] != '\0') ? 1 : 0;
}

int locate_controller_has_results(const locate_controller_t *lc)
{
    if (!lc)
        return 0;
    return (lc->match_count > 0) ? 1 : 0;
}

void locate_controller_clear_results(locate_controller_t *lc)
{
    if (!lc)
        return;
    lc->match_count = 0;
}
