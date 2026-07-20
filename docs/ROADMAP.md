# Roadmap

This is sequenced, not scheduled — no dates, just order of dependency. Each phase is meant to produce something working before moving to the next, rather than designing everything up front and building it all at once.

## Phase 0 — Project setup
- [ ] Git repository initialized
- [ ] `LICENSE` (MPL-2.0), `README.md`, `PHILOSOPHY.md`, `CONTRACTS.md`, `DESIGN_DECISIONS.md` committed
- [ ] Repo structure established:
  ```
  red_plasma/
  ├── src/        — engine core (Rust)
  ├── os/
  │   ├── irp_os.h  — OS interface declarations (rp_* functions), no implementation
  │   ├── linux/    — Linux implementation (initial target)
  │   └── ...       — future OS targets
  ├── plugins/    — engine plugin binaries
  ├── modules/    — game module binaries
  ├── tools/      — development tooling (Python scripts, pre-commit hooks)
  └── docs/
  ```
- [ ] OS interface header `os/irp_os.h` written — initial `rp_*` function declarations covering: library loading, symbol resolution, time, file I/O, console output, threads, mutex, signal handling
- [ ] Linux IOS implementation in `os/linux/` — maps all `rp_*` functions to Linux APIs
- [ ] Rust toolchain set up; build system selects correct `os/<target>/` at compile time
- [ ] **Python 3** installed (required for development tooling — sorting, linting, pre-commit hooks)
- [ ] `tools/sort_glossary.py` — sorts all tables in GLOSSARY.md alphabetically by first column
- [ ] `tools/pre-commit` — git pre-commit hook (Python): runs glossary sort before every commit, fails commit if raw OS calls are found outside `os/`
- [ ] `setup.sh` — one-command dev environment setup: checks dependencies, installs pre-commit hook
- [ ] Pre-commit hook installed via setup script: `chmod +x setup.sh && ./setup.sh`
- [ ] Confirmed: grep for raw OS calls outside `os/` returns zero hits



## Phase 1 — Bootstrap and core loop

**Engine plugins (3-slot contract: `init`, `call`, `delete`):**
- [ ] IOS implementation compiled in (`os/linux/` for initial target) — replaces the old "bootstrap loader" concept, owns all OS contact
- [ ] Plugin loading via `rp_load_library`/`rp_get_symbol` (IOS calls, not raw OS calls) — loads fixed known list from `plugins/` in order: allocator → logger → sort → handle manager → module loader
- [ ] Manifest validation for plugins: magic number, `kind: plugin`, contract version, hard reject on mismatch
- [ ] Allocator plugin — first plugin loaded, pool allocator initially
- [ ] Logger plugin — loaded second, engine can log from this point forward
- [ ] Sort plugin — heapsort default, in-place, O(n log n) guaranteed, sorts both module arrays at load time
- [ ] Handle manager plugin — ID → pointer table, built on top of allocator
- [ ] Module loader plugin — loads modules from `modules/` folder after bootstrap completes

**Manifest fields implemented (both tiers):** magic number, `kind`, contract version, contract-shape hash, dependencies, author metadata

**Engine core loop:**
- [ ] `all_modules` — flat pointer array sorted by `run_priority` descending
- [ ] `recall_modules` — flat pointer array of `uses_recall: true` modules sorted by `recall_priority` descending
- [ ] Loop: `for ptr in all_modules → ptr.run()`, then `for ptr in recall_modules → ptr.recall()`, wait flag checked after each
- [ ] Module manifest fields: `always_wait`, `uses_recall`, `run_priority`, `recall_priority`
- [ ] 6-slot lifecycle table: `init`, `run`, `recall`, `update`, `delete`, `interrupt`

**First module — optimizer:**
- [ ] Declares `uses_recall: true`, `always_wait: true`, high `run_priority`, low `recall_priority`
- [ ] `run` → records start timestamp
- [ ] `recall` → records end timestamp, calculates elapsed, updates EMA, yields until next tick
- [ ] Integer-based internal timer (nanosecond/microsecond on desktop)
- [ ] EMA self-tuning (bit-shift ÷2), persisted across sessions, held fixed within a session
- [ ] Developer-defined fixed Hz for first launch

**Goal:** bootstrap loads plugins in order, module loader loads optimizer, loop runs both passes in priority order, optimizer brackets each tick — core never knows a timer exists.


## Phase 2 — Module loading mechanics
- [ ] Implement dynamic loading in the bootstrap loader — OS-specific mechanism abstracted here (Linux: `dlopen`/`dlsym`; Windows: `LoadLibrary`/`GetProcAddress`; initial implementation targets Linux dev platform)
- [ ] Module manifest: fixed exported `get_module_manifest()` function — magic number, contract version, optional contract-shape hash, author metadata
- [ ] Manifest validation on load: hard reject on magic number or contract version mismatch, before the lifecycle table is touched
- [ ] Circular dependency detection: build full dependency graph from all manifests in a load batch, cycle-check before any `init` call, hard reject the whole batch on a cycle with the exact module path named in the error
- [ ] Fixed entry point per module returning the 5-slot lifecycle table (`init`, `run`, `update`, `delete`, `interrupt`)
- [ ] Engine calls all five slots unconditionally on every loaded module
- [ ] Minimal dependency declaration mechanism (exact approach still open — see `CONTRACTS.md` open questions)
- [ ] A trivial "do nothing" test module to prove the loading/calling mechanism end to end

## Phase 3 — Foundational modules
- [ ] Allocator module(s): pool allocator first (simplest, needed by the handle manager), then arena/stack, general-purpose later if needed
- [ ] Handle manager module (ID → pointer table), built on top of the pool allocator
- [ ] Destruction notification: blocking-with-timeout via the subscriber system, caller-selectable timeout behavior (leave/terminate), always logged
- [ ] Error code conventions applied consistently across both (0–99 / 100–999 / 1000+ tiers)
- [ ] Generated error-string table (single source list → constants + lookup, no hand-maintained duplication)
- [ ] Logging system: timestamped persistent log, fatal-only console filter by default, compile-time-selected verbosity (silent → trace)

## Phase 4 — First window
- [ ] Naive window module (no widgets, no toolkit) — implemented via GLFW for now; initial dev platform is Fedora Linux KDE Plasma
- [ ] `createWindow()` returning requested values as placeholder
- [ ] Subscriber/event system in core, used for the `onResize`/`onConfigure` event
- [ ] A window opens and stays open, driven by the Phase 1 loop

## Phase 5 — First triangle (renderer)
- [ ] Vulkan renderer module, minimal: take a surface from the window module (via the handle manager), draw a single triangle
- [ ] Renderer abstraction layer design finalized as part of this work (thin vs. thick boundary — see open question in `DESIGN_DECISIONS.md`)
- [ ] This is the proof point for the whole architecture: window module + renderer module + handle manager + loop + error codes all working together

## Phase 6 — Iterate outward (not yet detailed)
- [ ] DirectX and/or OpenGL backend modules against the now-proven renderer abstraction
- [ ] Hand-rolled native window module (raw Xlib/XCB, raw Wayland protocol, raw Win32) to replace the GLFW-based Phase 4 module, once its contract is proven
- [ ] 2D rendering path
- [ ] Profiling/logging module for dev builds (groundwork for future session-mined first-launch calibration)
- [ ] Constrained-hardware variants of swappable modules (handle manager, allocators) — once a real target is identified

## Explicitly deferred (designed for, not built yet)
- Session-mined first-launch calibration (replaying real playtest-identified heavy scenarios instead of a fixed dev-defined Hz)
- 16-bit / constrained hardware module variants
- Reference counting or blocking handshake for cross-module handle destruction (current design relies on the handle manager's invalid-ID error alone)
