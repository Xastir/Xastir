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
| 4 | `bulletin_gui.c`      |   890 |          12 |         3 | ☐      |
| 5 | `track_gui.c`         |  1088 |          21 |         6 | ☐      |
| 6 | `geocoder_gui.c`      |  1179 |          32 |        17 | ☐      |
| 7 | `locate_gui.c`        |  1292 |          32 |         7 | ☐      |
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

## Aggregate counters

Update after each file lands.

- Files migrated:        3 / 13
- Total LOC reviewed:    1919
- Globals retired:       1
- New unit tests added:  53
