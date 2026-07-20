# Design Decisions

This document records the reasoning behind each major decision, including alternatives considered and what's still open. `CONTRACTS.md` is the binding spec; this is the "why," kept so the reasoning isn't lost.

---

## IOS — Interface Operating System

**Decision:** All OS-specific code lives in one place — the `os/` folder. Red Plasma defines its own vocabulary of system functions (`rp_*`) in `os/irp_os.h in ` (declarations only, no subfolder). Each OS target has its own implementation folder (`os/linux/`, `os/windows/`, etc.) that maps those `rp_*` calls to real OS APIs. Nothing outside `os/` is permitted to make a direct OS call — ever.

**Why a named IOS rather than ad-hoc abstraction:** every cross-platform system has a file (or files) where OS-specific code lives. Red Plasma makes it explicit, names it, and enforces it structurally rather than by convention. "It's in `os/` or it shouldn't exist" is auditable by grep — any raw OS call outside `os/` is a bug with a clear location to fix it.

**Why it's a never-ending update file and that's fine:** the IOS grows whenever Red Plasma needs something new from the OS. This is accepted and expected — the alternative (pretending OS interaction is finite and complete) is worse. The discipline is keeping each IOS function a pure thin mapping: `rp_*` call in, OS call out, no logic, no state, no decisions. If business logic appears in the IOS, it's in the wrong place.

**Why compiled in rather than a plugin:** the IOS has the same bootstrapping problem as the bootstrap loader — you can't load the IOS using the IOS. It must exist before anything else. Compiling it in is the only option, and it replaces the need for a separate "bootstrap loader" concept — the IOS implementation *is* the bootstrap layer for its target.

**Porting cost:** implementing `os/<target>/` for a new OS is the complete cost of a port. Engine core, plugins, and modules are untouched. For a constrained target like 16-bit hardware, IOS functions that can't be supported (e.g. `rp_thread_create`) simply aren't implemented — the capability gap is visible in the IOS rather than hidden in scattered ifdefs across the codebase.

---

**Decision:** Red Plasma has two distinct tiers of extensible binary, each with its own contract and purpose.

**Engine plugins (3-slot: `init`, `call`, `delete`)** — pure engine infrastructure with no gameplay or game-developer-facing capability. Loaded by the bootstrap loader (the only piece compiled directly into the engine core) in a fixed hardcoded sequence before anything else. Examples: allocator, logger, sort, handle manager, module loader. Never participate in the game loop. Swappable without recompiling — drop a dynamic library in the plugins folder.

**Modules (6-slot: `init`, `run`, `recall`, `update`, `delete`, `interrupt`)** — what game developers write and use. Loaded by the module loader plugin after bootstrap completes, via the manifest/priority/two-array system. Examples: optimizer, renderer, windowing, physics, audio, input.

**Why split instead of one contract:** engine plugins have a bootstrapping problem — the allocator must exist before anything can allocate memory, the sort plugin must exist before arrays can be sorted, the module loader must exist before modules can be loaded. These things can't use the full module contract because the infrastructure that contract depends on doesn't exist yet when they load. A minimal 3-slot contract for the bootstrap phase solves this without special-casing anything in the engine core.

**Why both tiers are dynamically loaded:** the "drop a file in a folder" model was specifically preserved for engine plugins despite the bootstrap complexity — the bootstrap loader is the only hardcoded piece, and it simply looks for a known fixed list of plugin names in the plugins folder. No recompile needed to swap the allocator, sort algorithm, or any other engine infrastructure. The OS-specific loading mechanism (`dlopen` on Linux, `LoadLibrary` on Windows, or equivalent) is owned entirely by the bootstrap loader — everything above it is OS-agnostic.

**Why the same manifest shape for both:** one validation path in the bootstrap loader rather than two different ones to maintain. The `kind` field (`plugin` or `module`) tells the loader which contract to apply after manifest validation passes.

---

## Module system

**Decision:** Everything game-developer-facing is a module. Each module owns its internal complexity fully; the engine only ever interacts with the 6-slot lifecycle contract and manifest. Foundational engine infrastructure is handled by plugins (see above).

**Why:** allows swapping not just "which physics library" but foundational pieces like the rendering API or even the OS windowing backend, without forking the engine. Hardware-specific concerns (allocator strategy, handle manager table size) can be addressed per-target via swappable plugins, without touching modules or the engine core.

---

## Renderer

**Decision:** Vulkan as the primary/default backend, with DirectX and OpenGL as alternate backend modules behind a shared abstraction. The shared abstraction is **fully opaque** — callers (game code, other modules) interact only with Red Plasma's own rendering vocabulary (e.g. create surface, load mesh, submit draw, present frame) through opaque handles. No backend-specific type or concept (a raw `VkBuffer`, a `VkDescriptorSet`, an `ID3D12Resource`) is ever exposed through the standard contract. Each backend module's job is to translate that vocabulary into whatever its underlying API actually needs internally.

**Why fully opaque rather than exposing the union (or lowest common denominator) of all three APIs:** Vulkan, DirectX, and OpenGL don't have equivalent capabilities — Vulkan in particular exposes very explicit low-level control (manual memory management, explicit synchronization) the others don't have a direct analog for. Trying to expose a common surface across all three would mean either running Vulkan in a deliberately crippled "lowest common denominator" mode, or leaking backend-specific concepts through the abstraction depending on which backend happens to be loaded — both unacceptable given the goal that callers never know or need to know what's actually rendering underneath.

**Why no backend-specific escape hatch is needed in the abstraction itself:** considered exposing some kind of optional extension mechanism for backend-specific features (e.g. Vulkan-only bindless textures or ray tracing) that performance-sensitive games might want. Rejected as unnecessary — the module system itself already *is* the escape hatch. The standard renderer module (and its opaque vocabulary) is a good default, not a mandatory path; anyone who genuinely needs direct Vulkan-specific functionality is free to write or use a different module that talks to Vulkan directly and skips the Red Plasma abstraction entirely, the same way a 16-bit target gets its own handle manager instead of being forced through the desktop one. This keeps the standard abstraction fully opaque with zero compromise, because it was never the only path in the first place.

**Resolved: whether 2D rendering is layered over 3D or a separate path — this isn't an engine-level decision at all.** This question was initially treated as something the engine needed to settle architecturally. On reflection, it doesn't: since renderers are modules, "2D as textured quads over a 3D pipeline" and "2D as a genuinely separate native path" are both just different renderer module implementations, exactly like Vulkan-vs-DirectX-vs-OpenGL. The engine core has no more reason to mandate one 2D strategy than it has to mandate one graphics API. Both can exist as separate modules, and a developer picks whichever fits their game. This mirrors the exact reasoning used to resolve the backend-specific-escape-hatch question above — once a concern is module-scoped rather than core-scoped, the engine doesn't need an opinion on it.

(See below for the resolution on 2D vs. 3D rendering paths.)

---

## Windowing

**Decision:** Each OS/windowing backend (Win32, Xorg, Wayland) is its own module, exposing the same contract. Initial target platform is Fedora Linux, KDE Plasma.

**Why this is harder than it looks:** Win32 and Xorg are both synchronous "ask the server, get an answer" models — straightforward to abstract. Wayland deliberately moved control of window position/decoration to the compositor and made surface setup event-driven (configure events) rather than synchronous. This makes a naive shared "create window, get back the real values" contract impossible to honor truthfully across all three.

**Resolution:** `createWindow()` returns the *requested* values immediately as a placeholder. The *actual* resulting values are delivered later via an `onResize`/`onConfigure` event through the engine's subscriber system. This is the same contract for every backend — Win32/Xorg just happen to fire the event near-instantly, while Wayland fires it whenever the compositor responds. No backend is special-cased; the asynchronous reality is treated as the general case all backends share, not a problem unique to Wayland.

**Considered and rejected:** building native Wayland/Xorg/Win32 handling entirely from scratch for v1. Using GLFW internally within the relevant window module(s) for now to absorb the real complexity of Wayland's protocol/compositor differences (KWin vs. Mutter vs. wlroots-based), without that complexity leaking into the rest of the engine. The module's *internals* using a library doesn't violate the "module owns its complexity" rule — the public contract stays the same either way. A hand-rolled native replacement (raw Xlib/XCB, raw Wayland protocol, raw Win32) is planned for later, once the rest of the engine is proven and the window module's contract is informed by real experience rather than guesswork.

**Window module scope — naive by design:** the window module is intentionally bare — a drawable surface to hand to a renderer module, nothing more. No widgets, no toolkit event loop, no UI chrome. Full application toolkits (Qt, GTK/libadwaita) were considered and explicitly rejected as window backends: they bring their own event loop (which would compete with the engine's own fixed-Hz loop) and a whole widget/theming system the engine has no use for. They remain relevant only for a *possible future editor* — see "Editor / tooling" below.

---

## Editor / tooling

**Decision:** Red Plasma the engine has no knowledge of, or dependency on, any editor or tool. A future editor is a fully separate project that consumes the engine purely through its public API, exactly as a game would.

**Why:** Keeps the engine a clean, embeddable library/runtime rather than an engine-plus-IDE bundle. If an editor is built later, it's free to use a full toolkit (Qt fits Plasma/KDE, GTK/libadwaita fits GNOME) for its own UI — those toolkits were never window-module candidates, they're application-toolkit candidates for tooling, a different layer entirely.

---

## Errors and return values

**Decision:** Every function returns an integer status/error code as its actual return value. Any data the function produces comes back via an out-parameter (pointer / pointer-to-pointer). Error codes are tiered: 0–99 OS, 100–999 engine, 1000+ module-defined (each module owns its own space, opaque to the engine).

**Why:** Considered a single universal result struct (`{ code, data }`) returned by value, but settled on the classic C out-parameter pattern instead — simpler function signatures, more idiomatic C, plays better with a dynamically-loaded shared-library module system where ABI simplicity matters. Tiered codes mean a failure's origin (OS vs. engine vs. specific module) is always identifiable from the number alone, without needing to know which module raised it.

---

## Ownership and the handle manager

**Decision:** Creator owns and is responsible for destroying what it creates. Cross-module access to a resource you don't own goes through a handle manager module (ID → pointer table) rather than a raw shared pointer.

**Why:** The hard problem here is cross-module use-after-free — e.g. the renderer module holding a pointer to a window's surface, and that window getting destroyed without the renderer knowing. Considered reference counting, but rejected as the default because it trades a memory-safety problem for a different bug class (forgotten increments/decrements) and adds memory/CPU overhead that conflicts with the constrained-hardware goal. The ID-based handle lookup alone already converts a crash into a clean, handleable "invalid handle" error code, which captures most of the benefit without that overhead.

**Resolved: destruction notification is blocking, with a timeout, and the caller decides the failure behavior.** This was genuinely difficult to settle — pure blocking is the safer option (dependents get a real chance to react before memory is freed) but risks hanging the engine if a dependent never acknowledges; pure non-blocking avoids any hang risk but reopens the exact use-after-free window the handle manager exists to close. The resolution keeps both: blocking up to a fixed timeout gives dependents a real window to clean up safely in the normal case, while the timeout itself is the safety net against a hang. Critically, the engine does not pick what happens *when* the timeout fires — that decision (leave the resource as a known, deliberate leak vs. terminate the program) belongs to the caller that initiated the destroy, since the right answer genuinely differs by context (a game might prefer to keep running; hardware-facing code might prefer to halt rather than continue in an unverifiable state). What is never optional is the reporting: regardless of which path the caller chooses, the event is always logged loudly, naming the specific resource and the specific unresponsive dependent — consistent with the project's standing rule that failures must be diagnosable, never silent.

**Why a handle manager and not just "be careful":** explicitly named because ownership/lifetime bugs were flagged as a known weak point worth designing around deliberately rather than relying on convention alone.

**Hardware scaling:** the manager is itself a swappable module — a hash table (or flat array, for sequential small IDs) is cheap enough that even constrained/16-bit targets could plausibly use a scaled-down version (smaller ID width, fixed-size table, no locking), rather than needing a fundamentally different mechanism. This was a useful realization — the *concept* generalizes down to constrained hardware even though it was initially assumed to be too heavy for it.

---

## Memory allocation

**Decision:** No reliance on system `malloc`. Custom allocator modules, chosen per use case: pool, arena/linear, stack, and general-purpose, each potentially swapped per hardware target.

**Why:** Consistent with the broader philosophy of controlling cost rather than accepting a general-purpose black box. The handle manager's internal table is expected to sit on top of an allocator module rather than reinvent memory management itself — meaning allocators are architecturally one of the first things that need to exist, since the handle manager (and likely window/renderer modules) will depend on them.

---

## Game loop and timing

**Decision:** The engine core loop is deliberately minimal — it knows nothing about timing, Hz, or optimization. It simply calls `run` on every loaded module in order, checks each module's wait flag, and repeats. All timing intelligence lives in the **optimizer module**.

**Why the engine core knows nothing about timing:** the original design had the core loop measuring execution time and doing EMA math itself. This was replaced when it became clear that timing is just another "module owns its own complexity" case — the timer mechanism varies wildly by hardware (OS syscall on desktop, hardware register on 16-bit), the math is self-contained, and the persistence is an internal concern. Moving it all into a module means the core loop is identical on every hardware target, and the optimizer module is what changes per platform. The engine core became: run modules, check flag, repeat.

**Why a wait flag instead of a special "yield" mechanism:** considered a dedicated yielding API the loop would call after all modules ran. Replaced by a generic per-module wait flag — a boolean the loop checks after every `run` call. The optimizer sets `always_wait: true` in its manifest, so the loop naturally blocks on it without knowing why. Any other module that needs "the loop pauses here" (audio buffer sync, hardware module, thread completion) gets the same mechanism for free. The engine needs no special cases.

**Why a hybrid static/runtime flag:** the manifest `always_wait` declaration is auditable at load time (before any code runs), which matters for hardware-facing modules where a wrong declaration is dangerous. The per-tick runtime override allows dynamic behaviour without a separate mechanism. One cheap boolean check per module per tick — negligible cost even if never changed.

**Convergence behavior (accepted tradeoff):** with a 0.5-weight EMA, a single anomalous session shifts the next session's value by half the gap — recovery is geometric over a few sessions, not instant. Accepted as fine since the system is designed to "even out with time."

**First launch:** developer-defined fixed Hz seeds the first value. Session-mined calibration (mining real playtest logs for the heaviest moment and replaying it) was discussed and deliberately deferred — strong future direction, not lost.

**Hot path constraint:** loop timing inside the optimizer is integer-only (scaled by target), hot path uses only add/subtract/compare/bit-shift — no division, no floating point. Division restricted to `init`-time setup only.

---

## Module lifecycle contract

**Decision:** Every module implements exactly six functions, always: `init`, `run`, `recall`, `update`, `delete`, `interrupt`. Unused ones are no-ops returning success. No capability declaration/flags — the engine always calls all six on every module.

**Why `recall` was added:** the optimizer module needs to bracket the entire tick — record a start timestamp before any other module runs, then record an end timestamp after everything else has finished, calculate elapsed time, update the EMA, and yield. A single `run` call per module in sequence can't express this. Rather than building a dynamic "call me again" flag mechanism, `recall` was added as a fixed sixth slot: the loop does one full pass calling `run` on every module, then a second full pass calling `recall` on every module. The optimizer uses both; every other module leaves `recall` as a no-op. This is consistent with the same reasoning behind the original five slots — no capability flags, no "does this module support recall," just always call it.

**Why two separate priority fields (`run_priority` and `recall_priority`):** a single priority number controlling both passes seems simpler but breaks the optimizer. The optimizer must be first in `run` (to stamp the tick start time before anything runs) but last in `recall` (to stamp the end time after all other recall work is done, then yield). Being first in recall would leave untimed work after it — a bottleneck that isn't measured. Separate integers let the optimizer declare high `run_priority` and low `recall_priority` without compromising either pass.

**Why high number = high priority:** if low number meant highest priority (e.g. 1 = first), module authors would defensively assign 1 to everything "just to be safe," collapsing the system into noise. High number = high priority forces deliberate thought about where a module actually sits in the order.

**Why sorting is a module:** the engine needs sorted pointer arrays before the first tick; *how* they get sorted is an implementation detail the core doesn't need to own. A constrained target may want insertion sort (tiny code size, fast on small arrays); a desktop build uses heapsort by default. The sort module is a core dependency — it must run before any other module's `init`. Default implementation is heapsort: in-place, no extra memory, guaranteed O(n log n) worst case, no pathological inputs unlike quicksort. Predictable cost on any hardware.

**Why arrays hold pointers only:** modules are not owned by the arrays. The arrays are sorted at load time and walked in order by the hot loop — pure pointer iteration, nothing else. Removing a module means removing its pointer from the relevant array(s).

---

## Core implementation language

**Decision:** the engine core (loop, module loader, handle manager, allocators) is implemented in **Rust**. Every module's exported boundary must be a plain **C ABI**, regardless of what language the module is written in internally.

**Why Rust for the core, specifically:** the core owns the project's most ownership-sensitive code — the handle manager and the custom allocators — exactly the kind of code where C++ engines historically accumulate use-after-free, double-free, and dangling-pointer bugs. Rust's compile-time borrow checking catches that class of bug before it ships, directly reinforcing decisions already made elsewhere in the design (the handle manager exists specifically to turn use-after-free into a clean error code; Rust extends that same intent into the implementation language itself).

**Why this doesn't conflict with the C-ABI module boundary:** neither Rust nor C++ has a stable native ABI of its own — both solve cross-compiler/cross-language compatibility the same way, by dropping to a C-shaped boundary (`extern "C"`). The Linux kernel's adoption of Rust is the proven precedent at serious scale: the kernel itself remains C-ABI at its core, and Rust kernel modules call into it through the same `extern "C"` mechanism any C-ABI-respecting language would use. Red Plasma follows the identical pattern — Rust core, C ABI boundary, any language behind that boundary.

**Why not require modules to also be memory-safe:** rejected as unenforceable. Once a pointer crosses the FFI boundary into a module written in C or any other language, Rust's guarantees no longer apply — there's no way for the core to verify safety it didn't itself produce. Rather than imply a guarantee the architecture can't back up, the responsibility is made explicit: module authors are responsible for their own code's safety, and modules are expected to be reviewed/audited before being trusted, the same way any native plugin ecosystem handles third-party code.

**Considered and rejected:** building the core in C++ to match prior project experience and avoid a learning curve. Rejected because this is explicitly a hobby/learning project where momentum isn't the priority, and the core's hardest problems (ownership-sensitive data structures) are exactly where learning Rust pays off most directly.

---

## Coding style

**Decision:** the engine core follows standard idiomatic Rust naming conventions throughout — `snake_case` functions and variables, `UPPER_SNAKE_CASE` constants, `PascalCase` types and enum variants.

**Why not a custom scheme:** an early instinct was `snake_case` functions, `camelCase` variables, `UPPERCASE` constants — closer to a C/C++ habit. On reflection, the variable casing had no strong attachment behind it, and `rustc`/`clippy` enforce `snake_case` for both functions *and* variables by default (`non_snake_case` lint warnings on anything else). Keeping camelCase variables would mean either permanently suppressing that lint project-wide or accepting constant warning noise that makes genuinely useful warnings harder to spot. Since the preference wasn't firm, adopting Rust's own convention outright removes the friction entirely rather than fighting the toolchain indefinitely.

---

## Module manifest and compatibility validation

**Decision:** every module exposes a manifest through a fixed, known exported function (e.g. `get_module_manifest()`), checked by the engine before the module's lifecycle table is ever touched. The manifest contains a magic number, a single incrementing contract-version integer, an optional contract-shape hash, and author/writer metadata. A mismatch on magic number or contract version is a **hard reject** — no partial loading, no backward/forward compatibility shimming.

**Why hard reject instead of tolerant/lenient versioning:** the engine's own ABI is expected to change rarely and deliberately — this was an explicit design stance, not an assumption. Given that, a compatibility matrix (semver-style major/minor reasoning, "older module probably still works") adds real complexity to defend against a scenario that should be rare by design. A single exact-match check is simpler, matches the "fewer conditional paths, fewer silent failure modes" instinct already used for the always-call-all-five-slots rule, and fails loudly and immediately rather than letting a subtly incompatible module run.

**Why an in-binary manifest, never a sidecar file:** this was the deciding factor, and it's a behavioral argument, not a technical one. A sidecar manifest file (e.g. `mymodule.manifest` next to the dynamic library) is easier to inspect without loading the binary — which sounds like a benefit for auditing, but actually undermines it: it gives a reviewer a plausible-looking shortcut (skim the manifest, see a reasonable version and author, move on) that bypasses actually reading the module's code. Since modules are explicitly trusted only after being audited (see `PHILOSOPHY.md` §9), the manifest mechanism itself must not make it easy to skip that audit. An in-binary manifest, accessible only by actually loading/inspecting the real binary, has no such shortcut. This was a deliberate "make it hard to do it wrong" choice, prioritized over the sidecar's convenience.

**Why dependencies are declared in the manifest, not a separate mechanism:** dependency information has the same property the rest of the manifest exists to protect — it must be known and validated *before* a module is trusted enough to call `init` on. Putting it anywhere else would mean checking compatibility in one place and dependencies in another, for no real benefit. Folded into the same field set as version/hash/author, validated in the same pass, rejected with the same hard-reject policy.

**Why the engine doesn't auto-load missing dependencies:** considered and rejected. Auto-loading would mean a module's manifest could implicitly trigger loading other arbitrary dynamic libraries at runtime, which both weakens the audit story (a reviewed module could pull in an unreviewed one without anyone choosing to load it) and adds real complexity (resolving load order, detecting circular dependencies). Instead, a missing dependency is just another hard-reject validation failure with a clear error code — load order is the explicit responsibility of whatever orchestrates startup, not something modules can trigger in each other.

**Why circular dependencies are caught before any module initializes, not at runtime:** drawn directly from real, painful prior experience with Linux package management and game modding ecosystems, where two mods/packages depending on each other can cause hangs, crashes, or subtle broken states that are difficult to diagnose after the fact. Because manifests are readable without calling `init` (§7a), the engine can build the full dependency graph and run a cycle check *before* committing to initializing anything — turning a runtime hang into a load-time, named, refusable error.

**Why the whole batch is rejected, not just the cyclic modules:** considered loading the non-cyclic modules in a batch and only rejecting the ones caught in the cycle. Rejected in favor of all-or-nothing, consistent with the hard-reject stance used everywhere else in the manifest system — a load batch is either fully trustworthy or it isn't.

**Why the error must name the exact cycle path, not just report failure:** this was an explicit, deliberate requirement, not a nice-to-have. A generic error code forces whoever's debugging to manually reconstruct which modules are involved — effortful, error-prone diagnosis that this project is specifically trying to design away from, everywhere. The cycle-detection error names every module on the path, in order, and includes a plain-language next step (update or remove one of the modules involved). This is treated as a real accessibility requirement, not just good practice.

---

## Logging and error string generation

**Decision:** error strings are generated from a single source-of-truth list (code, name, message template), never hand-maintained in a separate lookup. Logging is timestamped and persistent regardless of console output, which is fatal-only by default. Verbosity is selected at **compile time**, with a Linux `-v`-style six-level scheme (silent through trace/every-call). Engine core implements and owns this system; modules aren't required to use it internally, but an "official" module must follow the same message-style convention and produce its own `<module_name>.log`.

**Why generated, not hand-written:** manually keeping a numeric code and its string in sync in two places is exactly the kind of error-prone busywork this project avoids elsewhere (it's the same reasoning behind the manifest's hard-reject simplicity, and behind always-call-all-five-slots instead of capability flags). A single definition generates both the constant and the lookup, so they can't drift apart.

**Why timestamp lives in the logging layer, not the error itself:** an error's static template (`"could not read file: {path}"`) is the same regardless of when or whether it's ever logged. Not every error gets logged — some are handled silently. Baking a timestamp into the error itself would imply every error is always recorded, which isn't true; the timestamp is a property of the act of logging, applied at the moment an error is actually recorded.

**Why console output is filtered to fatal-only by default, while logging captures everything:** keeps the console quiet and usable day-to-day, while still preserving a full timestamped trail for the cases where deeper inspection is actually needed — satisfies both "don't spam me" and "let me look it up later" without contradiction.

**Why compile-time verbosity instead of a runtime flag:** a runtime flag (an actual `-v` argument or config value) was the more flexible option and was seriously considered, but rejected specifically because of the project's hardware-scaling goal. Even a single cheap branch evaluated on every function call is a cost some constrained/16-bit targets may not be able to absorb if paid unconditionally. Compile-time selection allows trace-level logging to be physically removed from a build that doesn't need it — true zero cost rather than merely cheap, consistent with the project's broader stance on hot-path cost.

**Why modules aren't forced to use the engine's logging system, only its conventions:** modules can be written in any language with a C ABI boundary — there's no single logging implementation that could realistically be mandated across all of them. Instead, the requirement is on the *output shape* (message-style convention, a named per-module log file), which is achievable regardless of implementation language, rather than the *mechanism*, which isn't.

---

## Licensing

**Decision:** Mozilla Public License 2.0 (MPL-2.0).

**Why:** File-level copyleft — modifications to Red Plasma's own source files must be shared if distributed, but modules/plugins built against the engine's interfaces, and games built with the engine, are not considered modifications of those files and can be licensed freely, including proprietary. Considered against MIT/Apache (too permissive — wouldn't guarantee engine improvements flow back) and GPL (notorious ambiguity around whether linking against GPL code "infects" the linked program, which would create real uncertainty for commercial games built on the engine). MPL was chosen specifically to avoid that ambiguity at the file boundary, which fits a plugin-based architecture naturally since modules are already separate files/compilation units.
