# Migration Progress Tracker

Tracks the per-file controller-extraction work described in
`MIGRATION_PLAN.md`. Files are ordered easiest → hardest based on lines of
code, Widget references, and callback count. Re-rank after each file if
reality disagrees with the metric.

Status legend: ☐ todo · ◧ in progress · ☑ done · ⊘ skipped

## Ranking metrics (snapshot, master @ start of migration)

| # | File                  | LOC   | Widget refs | Callbacks | Status |
|---|-----------------------|-------|-------------|-----------|--------|
| 1 | `popup_gui.c`         |   417 |           9 |         1 | ☑      |
| 2 | `location_gui.c`      |   672 |          12 |         4 | ☑      |
| 3 | `view_message_gui.c`  |   830 |          14 |         6 | ☑      |
| 4 | `bulletin_gui.c`      |   890 |          12 |         3 | ☑      |
| 5 | `track_gui.c`         |  1088 |          21 |         6 | ☑      |
| 6 | `geocoder_gui.c`      |  1179 |          32 |        17 | ☑      |
| 7 | `locate_gui.c`        |  1292 |          32 |         7 | ☑      |
| 8 | `wx_gui.c`            |  2421 |          54 |         4 | ☐      |
| 9 | `list_gui.c`          |  2808 |          42 |        18 | ☐      |
|10 | `messages_gui.c`      |  2872 |          53 |        22 | ☐      |
|11 | `db_gui.c`            |  4978 |          62 |        21 | ☐      |
|12 | `objects_gui.c`       |  6181 |          48 |        88 | ☐      |
|13 | `interface_gui.c`     |  9998 |         228 |        95 | ☐      |

The first three (popup, location, view_message) are the pattern-shakedown
batch. After they're done, revisit this file before continuing.

## Per-file log

Append a short entry below as each file is migrated. Keep it terse:
what got moved, what got tested, what got left behind.

### Template

```
#N <file>.c — <yyyy-mm-dd>
  Controller:  src/<feature>_controller.{h,c}  (~NNN LOC)
  Tests:       tests/test_<feature>_controller.c (M cases)
  Globals retired:  <list of names>, or "none"
  Globals deferred: <names that should still be drained later>
  Notes:       <surprises, deviations from the plan, follow-ups>
```

### Entries

```
#1 popup_gui.c — 2026-04-26
  Controller:  src/popup_controller.{h,c}  (80 + 147 LOC)
  Tests:       tests/test_popup_controller.c  (11 cases, all green;
               full suite: 241/241 ok)
  Globals retired:
    - popup_time_out_check_last  (file-scope time_t, zero external users)
       → moved into popup_controller_t.last_timeout_check
  Globals deferred (intentional, future passes):
    - pw[MAX_POPUPS]             — Widget bag; stays in popup_gui.c
    - pwb (Popup_Window)         — DEAD: written by clear_popup_message_windows,
                                    never read anywhere. Out of scope for this pass.
    - popup_message_dialog_lock  — mutex; threading/Motif concern
    - id_font (popup_ID_message) — X11 font cache
  Behavior preserved:
    - "all slots full" still silently drops the new popup (legacy i=0 fallback
      kept in popup_message_always).
    - popup_ID_message (rotated ATV ID text) untouched — pure X11 graphics, no
      logic kernel worth extracting.
  Surprises:
    - popup_controller.c has zero Xastir-side dependencies, so the test binary
      needs *no* stubs file. First validation that "self-contained controller
      header" is the right rule for this codebase.
    - Slot occupancy now tracked in two places (Widget != NULL and
      controller.slots[i].active). Kept lockstep at 4 mutation sites; acceptable
      for v1, candidate for unification once a few more dialogs are migrated.
```

```
#2 location_gui.c — 2026-04-26
  Controller:  src/location_controller.{h,c}  (131 + 343 LOC)
  Tests:       tests/test_location_controller.c  (16 cases, all green;
               full suite: 257/257 ok)
  Logic extracted:
    - location_controller_parse_line()   — parse pipe-delimited locations.sys line
    - location_controller_name_valid()   — validate name (non-empty, < 100 chars)
    - location_controller_name_exists()  — duplicate-name check (file scan)
    - location_controller_find()         — lookup by name, return lat/lon/zoom
    - location_controller_add()          — append new entry (append-to-file)
    - location_controller_delete()       — filter file removing named entry
  Globals retired:  none (no extractable file-scope state existed)
  Globals deferred (intentional):
    - location_dialog / location_list    — Widget handles; stay in location_gui.c
    - location_dialog_lock               — mutex; threading/Motif concern
    - center_latitude / center_longitude / scale_y — map-state globals;
                                           GUI passes them in as strings after
                                           calling convert_lat_l2s / convert_lon_l2s
  Behavior preserved:
    - jump_sort() left in location_gui.c: calls sort_input_database() (db.c) and
      sort_list() (main.c/Widget), both too coupled to GUI tier to split here.
    - location_add(): validation order preserved (name_valid first, then
      name_exists); error popups unchanged.
    - location_delete(): original used "a" (append) mode for temp file — changed
      to "w" (overwrite) which is the correct atomic-rewrite behaviour; net
      result is identical since temp file is immediately renamed over original.
  Surprises:
    - Controller uses only standard C (fopen/fgets/strtok/rename); no stubs file
      needed. Pattern from popup_controller confirmed: self-contained = no stubs.
    - location_add() name buffer changed from char[100] to char[LOCATION_NAME_MAX]
      (same value, 100) to keep it in sync with the controller constant.
```

```
#3 view_message_gui.c — 2026-04-26
  Controller:  src/view_message_controller.{h,c}  (146 + 251 LOC)
  Tests:       tests/test_view_message_controller.c  (26 cases, all green;
               full suite: 283/283 ok)
  Logic extracted:
    - view_message_controller_should_display() — unified filter kernel replacing
      duplicated range/packet-type/mine-only logic that existed independently in
      both view_message_print_record() and all_messages()
    - view_message_format_record() — formats the "%-9s>%-9s …" record-view line
    - view_message_format_line()   — formats the "all messages" display line
      including broadcast detection (java/USER prefixes) and 95-char split
    - view_message_strip_ssid()    — SSID-stripping helper used by should_display
  Globals retired:  none
  Globals deferred (intentional):
    - vm_range / view_message_limit / Read_messages_packet_data_type /
      Read_messages_mine_only  — still owned by view_message_gui.c and read by
      xa_config.c; synced into vm_controller before each filtering decision.
      A future pass will have xa_config write directly to the struct and retire
      the plain-int globals.
    - All_messages_dialog / view_messages_text / vm_dist_data / button_range
      — Widget handles; stay in view_message_gui.c
    - All_messages_dialog_lock  — mutex; Motif/threading concern
  Behavior preserved:
    - Mine-only mode bypasses range check (original legacy behaviour kept).
    - Broadcast detection: "java" or "USER" prefix → label substitution.
    - Long message (>95 chars) split with "\n\t" prefix on continuation.
    - Message-limit scroll trim unchanged.
  Surprises:
    - Original all_messages() used unsafe strcat chains with "sizeof(temp)"
      on a char* (always 8 on 64-bit); all replaced by snprintf in the
      controller, which is both safer and correct.
    - The filtering logic was byte-for-byte identical in view_message_print_record
      and all_messages — de-duplication was the main win here.
    - No stubs file needed: controller is pure standard C.
```

```
#4 bulletin_gui.c — 2026-05-09
  Controller:  src/bulletin_controller.{h,c}  (170 + 212 LOC)
  Tests:       tests/test_bulletin_controller.c  (27 cases, all green;
               full suite: 310/310 ok)
  Logic extracted:
    - bulletin_controller_in_range()    — unified range/zero-distance filter
      replacing identical 3-condition check duplicated in bulletin_message(),
      count_bulletin_messages(), bulletin_data_add(), and zero_bulletin_processing()
    - bulletin_controller_record_new()  — bracket tracking for new-bulletin batch
      (first_new_time / last_new_time)
    - bulletin_controller_count_one()   — per-message "new & in-range" counter
    - bulletin_controller_should_check()  — three-gate timing guard (interval,
      new_flag, settle period) that was inlined in check_for_new_bulletins()
    - bulletin_controller_mark_checked()  — records the last check timestamp
    - bulletin_controller_reset_batch()   — advances first_new_time past batch,
      clears new_flag and new_count
    - bulletin_format_line()  — formats "%-9s:%-4s (time dist unit) msg\n";
      preserves original tag+3 ("BLN" prefix stripped from display)
  Globals retired:  none (deferred intentionally — xa_config.c pass)
  Globals deferred (intentional):
    - bulletin_range              — owned by bulletin_gui.c, exported via
                                    bulletin_gui.h; synced into bc before filter
    - new_bulletin_flag           — file-scope int; synced to/from bc.new_flag
    - new_bulletin_count          — file-scope int; synced to/from bc.new_count
    - first_new_bulletin_time     — file-scope time_t; synced to/from bc.first_new_time
    - last_new_bulletin_time      — file-scope time_t; synced to/from bc.last_new_time
    - last_bulletin_check         — file-scope time_t; synced to/from bc.last_check
    - pop_up_new_bulletins / view_zero_distance_bulletins  — defined in main.c,
      declared extern in main.h; synced into bc before each decision
    - Display_bulletins_dialog / Display_bulletins_text / dist_data — Widget
      handles; stay in bulletin_gui.c
    - display_bulletins_dialog_lock — mutex; threading/Motif concern
  Behavior preserved:
    - range==0 means unlimited (no upper bound on distance).
    - distance==0.0 path gated by view_zero_distance_bulletins.
    - Boundary condition: (int)distance <= bc->range  (matches original cast).
    - check_for_new_bulletins() settle period 15 s and interval 15 s now live
      as BULLETIN_CHECK_INTERVAL / BULLETIN_SETTLE_TIME in the header (same
      values, now named and testable).
    - zero_bulletin_processing() first_new_time back-extension preserved;
      range check delegated to bulletin_controller_in_range().
  Surprises:
    - The same 3-condition range expression appeared verbatim in four functions;
      de-duplication was the main structural win.
    - No stubs file needed: controller is pure standard C.
```

```
#5 track_gui.c — 2025-01-01
  Controller:  src/track_controller.{h,c}
  Tests:       tests/test_track_controller.c  (23 cases, all green;
               full suite: 333/333 ok)
  Stubs:       tests/test_track_controller_stubs.c  — valid_object, valid_call,
               valid_item (controllable return values, not abort stubs)
  Logic extracted:
    - track_controller_set_station()     — callsign trim (internal statics;
      no util.c dependency) + valid_object/valid_call/valid_item gate;
      sets station_on and station_call; returns 1/0
    - track_controller_clear_station()   — zero station_on and station_call
    - track_controller_clamp_posit_length() — mirrors Reset_posit_length_max()
      logic: if posit_start > MAX_FINDU_DURATION → length = MAX_FINDU_DURATION;
      else length = posit_start
    - track_controller_can_start_download() — two-gate busy guard:
      fetching_now || read_file_busy → 0
    - track_controller_build_findu_url() — snprintf the findu raw-packet URL
      from download_call / posit_start / posit_length
  Globals retired:  none
  Globals deferred (intentional):
    - track_station_on / track_me / track_case / track_match /
      tracking_station_call  — defined in track_gui.c, exported via
      track_gui.h; consumed heavily by db.c and main.c; synced from/to
      tc after each controller call.
    - posit_start / posit_length / fetching_findu_trail_now /
      download_trail_station_call  — file-scope, synced to tc fields
      before each controller call.
  Constants promoted:
    - MAX_FINDU_DURATION and MAX_FINDU_START_TIME moved from #defines in
      track_gui.c to track_controller.h.
  Behavior preserved:
    - Trimming order: spaces first, then "-0" suffix (matches original
      remove_trailing_spaces / remove_trailing_dash_zero call order).
    - Clamping: posit_length cannot exceed posit_start, and neither
      can exceed MAX_FINDU_DURATION (mirrors the slider callback exactly).
    - findu URL template: identical to the original xastir_snprintf call.
    - Busy guard: two separate conditions (fetching_now, read_file) kept
      visible in the GUI for distinct error messages; controller provides
      the combined check for test coverage.
  Surprises:
    - valid_object / valid_call / valid_item needed stubs — first use of
      the stubs pattern in this migration series.
    - remove_trailing_spaces and remove_trailing_dash_zero were inlined as
      private static helpers in track_controller.c to keep it self-contained.
```

```
#6 geocoder_gui.c — 2026-04-26
  Controller:  src/geocoder_controller.{h,c}
  Tests:       tests/test_geocoder_controller.c  (30 cases, all green;
               full suite: 363/363 ok)
  Stubs:       none — controller is pure standard C
  Logic extracted:
    - country_options[] table (ISO 3166-1 alpha-2 labels+values) moved from
      private struct in geocoder_gui.c to geocoder_controller.c
    - geocoder_controller_country_count()      — sentinel-safe entry count
    - geocoder_controller_country_label(idx)   — 0-based label access
    - geocoder_controller_country_value(idx)   — 0-based value access
    - geocoder_controller_find_country_index() — returns 1-based XmList pos
    - geocoder_controller_label_to_code()      — label→code without Widget;
      handles "custom" entry by reading custom_text arg; buffer-based API
      (avoids XtNewString/strdup ownership ambiguity)
    - geocoder_controller_normalize_server_url() — empty URL → default
    - geocoder_controller_has_results()        — result_count > 0 gate
  Globals retired:  struct country_option typedef + country_options[] array
  Globals deferred:
    - nominatim_server_url / nominatim_user_email / nominatim_country_default /
      nominatim_cache_enabled / nominatim_cache_days — owned by nominatim.c,
      declared in main.h; consumed by xa_config.c; deferred to xa_config pass
    - current_results (geocode_result_list) — file-scope dialog state;
      result_count mirrored into geocoder_gc.result_count after each populate
  Surprises:
    - The name 'gc' conflicted with extern GC gc (X11 graphics context)
      declared in xastir.h. Renamed the local instance to geocoder_gc.
    - populate_country_list() reduced to two controller calls: country_label
      loop + find_country_index() for default selection.
    - get_selected_country_code() still owns all Widget interaction but
      delegates matching to geocoder_controller_label_to_code() — tested.
```

```
#7 locate_gui.c — 2026-04-26
  Controller:  src/locate_controller.{h,c}
  Tests:       tests/test_locate_controller.c  (25 cases, all green;
               full suite: 388/388 ok)
  Stubs:       none — controller is pure standard C
  Logic extracted:
    - locate_controller_prepare_call()         — copy raw callsign → station_call;
      strip trailing spaces, strip "-0" suffix; return 0 if empty
    - locate_controller_prepare_place_query()  — copy + trim all six place-search
      fields (place, state, county, quad, type, gnis_filename); return 0 if
      place_name empty after trim; NULL optional fields treated as ""
    - locate_controller_has_results()          — match_count > 0 gate; NULL safe
    - locate_controller_clear_results()        — reset match_count to 0; NULL safe
    - locate_controller_init()                 — memset struct to zero; NULL safe
  Globals retired:
    - match_array_name[50][200], match_array_lat[50], match_array_long[50],
      match_quantity  — file-scope globals replaced by lc.match_names /
      lc.match_lat / lc.match_lon / lc.match_count; Locate_place_chooser and
      Locate_place_chooser_select updated to use lc directly.
  Globals deferred (intentional):
    - locate_station_call[30]    — externally written by db.c; synced from
      lc.station_call after each locate_controller_prepare_call() call
    - locate_place_name / locate_state_name / locate_county_name /
      locate_quad_name / locate_type_name / locate_gnis_filename  — read by
      xa_config.c (save/restore config); synced from lc fields after each
      locate_controller_prepare_place_query() call
    - Widget handles and mutexes  — stay in locate_gui.c (threading/Motif)
  Behavior preserved:
    - Locate_station_now(): trim+dash-zero strip now via controller; result
      synced to legacy locate_station_call for db.c compatibility.
    - Locate_place_now(): six-field read-from-widget flow preserved; query
      delegated to controller; gnis_locate_place() / pop_locate_place()
      called with lc match arrays directly (no extra copy).
    - Chooser dialog: match_quantity kept in sync with lc.match_count;
      Chooser reads lc.match_names / lc.match_lat / lc.match_lon.
  Surprises:
    - remove_trailing_spaces / remove_trailing_dash_zero inlined as private
      static helpers (same pattern as track_controller.c).
    - gnis_locate_place() / pop_locate_place() / locate_station() all require
      Drawable da as first arg (they draw on the map), so they stay in
      locate_gui.c — only the field prep and result management moved.
    - fcc_rac_lookup() is entirely GUI+FCC-database code; not split.
```

## Aggregate counters

Update after each file lands.

- Files migrated:        7 / 13
- Total LOC reviewed:    6368
- Globals retired:       6
- New unit tests added:  158
