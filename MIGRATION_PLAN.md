# Xastir Presentation/Logic Separation — Migration Plan

This plan operationalizes Phase 2 of the Qt migration: extract domain logic
from Motif callbacks into testable controllers, one `*_gui.c` file at a time.
The Qt port itself is downstream of this work; success here is measured by
shrinking the global `Widget` table and growing the unit-test suite, not by
toolkit changes.

## Guiding constraints (carried over from prior decisions)

- **De-spaghetti first.** Each migrated file ends with logic in a controller,
  not in callbacks. Toolkit swap is a later concern.
- **C compiled as C++** is the agreed language strategy. New controllers stay
  in C-style structs and free functions for now — no STL, no RAII rewrites.
- **Map canvas (`maps.c`, `draw_symbols.c`, `map_*.c`) is out of scope.** Don't
  refactor map rendering as part of this work.
- **No new Cairo surface area.** Cairo retires when Qt arrives.
- **Tests are the safety net.** Every migrated file gets at least one new
  `test_<feature>_controller.c` covering its extracted logic, wired into the
  existing Autotest harness in `tests/`.

## The pattern (apply to every file)

For each `src/<feature>_gui.c`:

1. **Inventory.** List every global the file reads or writes — `Widget`
   handles, dialog flags, current selections, transient buffers — and every
   non-trivial helper that doesn't directly call Motif. The result is the
   future controller's state and methods.
2. **Create `src/<feature>_controller.{h,c}`.** Define a struct that owns the
   non-Widget state. Move the pure-logic helpers in as functions taking a
   `<feature>_controller_t *` first argument. No `<Xm/*.h>` includes here.
3. **Thin out `<feature>_gui.c`.** Callbacks now do three things only: pull
   values out of widgets, call into the controller, push results back into
   widgets. The `Widget` handles can stay as file-scope statics for this pass
   — the goal is to drain *logic*, not to redesign Motif ownership yet.
4. **Stubs file.** Add `tests/test_<feature>_controller_stubs.c` for any
   external symbols the controller's `.c` pulls in transitively but the test
   doesn't exercise. Pattern: see `tests/test_util_stubs.c`.
5. **Test file.** Add `tests/test_<feature>_controller.c` exercising the
   extracted logic directly. Use `tests/test_framework.h` macros.
6. **Autotest wiring.** Add `tests/<feature>_controller_tests.at` and
   `m4_include` it from `testsuite.at`. Add a `check_PROGRAMS` entry plus
   `_SOURCES`/`_CPPFLAGS` block in `tests/Makefile.am`.
7. **Build & run.** `./bootstrap.sh && ./configure && make && make check`
   must pass before the file is marked done.
8. **Tick the tracker.** Update `MIGRATION_PROGRESS.md` — set the row to
   "done", record LOC moved, and note any globals still left behind for
   later passes.

## What "done" means for one file

A file is migrated when:

- All non-Motif logic from `<feature>_gui.c` lives in
  `<feature>_controller.{h,c}`.
- `<feature>_gui.c` no longer contains business rules — only widget
  marshalling.
- At least one unit test exercises the new controller via the Autotest
  harness, and `make check` passes.
- The `MIGRATION_PROGRESS.md` row is updated with the actual outcome
  (LOC moved, globals retired, tests added, surprises encountered).

Partial migrations are allowed and expected for the larger files
(`db_gui.c`, `objects_gui.c`, `interface_gui.c`); the tracker records what
got done in each pass.

## Ordering principle

Files are tackled smallest-and-simplest first to refine the controller
pattern on cheap targets before paying for it on the expensive ones. The
ranking in `MIGRATION_PROGRESS.md` is by lines of code combined with
widget/callback density (proxies for coupling).

## What this plan deliberately does *not* do

- It does not consolidate the global `Widget` handle table in `main.c`.
  That is a follow-up pass after enough controllers exist to know the right
  ownership shape.
- It does not introduce a new event loop, threading model, or signal/slot
  layer. Those belong to the Qt port.
- It does not rewrite `maps.c`, `draw_symbols.c`, or `map_*.c`.
- It does not change the on-disk config format or the wire protocol.

## When to revisit this plan

After the first three files (`popup_gui.c`, `location_gui.c`,
`view_message_gui.c`) are done, the controller pattern will be concrete.
At that point:

- If the per-file cost is materially higher than expected, stop and
  redesign the pattern before continuing.
- If a recurring shape emerges (e.g. most dialogs need the same lifecycle
  hooks), promote it to a small shared header rather than copy-pasting.
- Re-rank the remaining files in `MIGRATION_PROGRESS.md` if the metrics
  turned out to mis-predict difficulty.
