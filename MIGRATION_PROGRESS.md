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
| 2 | `location_gui.c`      |   672 |          12 |         4 | ☐      |
| 3 | `view_message_gui.c`  |   830 |          14 |         6 | ☐      |
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

## Aggregate counters

Update after each file lands.

- Files migrated:        1 / 13
- Total LOC reviewed:    417
- Globals retired:       1
- New unit tests added:  11
