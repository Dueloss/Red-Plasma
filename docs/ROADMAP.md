# Roadmap

This is sequenced, not scheduled — no dates, just order of dependency. Each phase is meant to produce something working before moving to the next, rather than designing everything up front and building it all at once.

## Phase 0 — Project setup
- [ ] Git repository initialized
- [ ] `LICENSE` (MPL-2.0), `README.md`, `PHILOSOPHY.md`, `CONTRACTS.md`, `DESIGN_DECISIONS.md` committed
- [ ] Basic repo structure decided (e.g. `/engine`, `/modules`, `/docs`, `/tools`)
- [ ] Rust toolchain set up for the engine core; confirm `extern "C"` module-loading approach on Fedora before relying on it

## Phase 1 — Core loop and timing
- [ ] Integer-based timing (nanosecond/microsecond resolution, scaled by target)
- [ ] Fixed-Hz loop skeleton, no modules yet — just the loop itself
- [ ] Self-tuning average (EMA, bit-shift ÷2) implemented and measured against real loop execution time
- [ ] Persistence of the learned Hz across sessions (simple state file)
- [ ] Developer-defined fixed Hz for first launch (no calibration pass yet)

This phase is intentionally standalone — no module loading, no renderer, no window. Just the loop, proven to work and self-tune on its own.

## Phase 2 — Module loading mechanics
- [ ] Decide and implement the dynamic loading mechanism (`dlopen`/`dlsym` on Linux/Fedora first)
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
- [ ] Naive window module (no widgets, no toolkit) for the primary dev target (Fedora, KDE Plasma) — implemented via GLFW for now
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
